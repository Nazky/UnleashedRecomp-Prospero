#include "mod_loader.h"
#include "ini_file.h"

#include <api/Hedgehog/Base/System/hhAllocator.h>
#include <cpu/guest_stack_var.h>
#include <kernel/function.h>
#include <kernel/heap.h>
#include <user/config.h>
#include <user/paths.h>
#include <os/logger.h>
#include <os/process.h>
#include <xxHashMap.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <fstream>
#include <iterator>
#include <limits>
#include <set>
#include <tuple>
#include <system_error>
#include <sstream>

enum class ModType
{
    HMM,
    UMM
};

struct Mod
{
    ModType type{};
    std::filesystem::path root;
    std::filesystem::path modIniPath;
    bool managed = false;
    std::vector<std::filesystem::path> includeDirs;
    bool merge = false;
    ankerl::unordered_dense::set<std::filesystem::path> readOnly;
};

static std::vector<Mod> g_mods;
static std::vector<UserModInfo> g_userMods;
static std::atomic<uint64_t> g_modGeneration{};

static std::filesystem::path NormalizeModPath(const std::filesystem::path& path)
{
    std::error_code error;
    std::filesystem::path absolutePath = std::filesystem::absolute(path, error);
    return (error ? path : absolutePath).lexically_normal();
}

static bool HasPathPrefix(const std::filesystem::path& parent, const std::filesystem::path& child)
{
    auto parentIt = parent.begin();
    auto childIt = child.begin();

    for (; parentIt != parent.end(); ++parentIt, ++childIt)
    {
        if (childIt == child.end() || *parentIt != *childIt)
            return false;
    }

    return true;
}

static bool IsProsperoPermissionStatusError(const std::error_code& error)
{
#if defined(__PROSPERO__)
    return error == std::errc::operation_not_permitted || error == std::errc::permission_denied;
#else
    (void)error;
    return false;
#endif
}

static std::filesystem::file_status GetPathStatusWithFallback(
    const std::filesystem::path& path,
    std::error_code& error,
    bool& isSymlink)
{
    auto status = std::filesystem::symlink_status(path, error);
    isSymlink = !error && std::filesystem::is_symlink(status);

    // Prospero's filesystem layer can return EPERM for symlink_status even on
    // ordinary user-data entries. Fall back to status() so valid mod files are
    // still visible; the managed-path lexical containment checks remain active.
    if (error == std::errc::operation_not_permitted || error == std::errc::permission_denied)
    {
        error.clear();
        status = std::filesystem::status(path, error);
        isSymlink = false;
    }

    return status;
}

static bool IsSafeManagedModPath(const Mod& mod, const std::filesystem::path& candidate)
{
    if (!mod.managed)
        return true;

    const std::filesystem::path root = mod.root.lexically_normal();
    const std::filesystem::path normalizedCandidate = NormalizeModPath(candidate);
    if (!HasPathPrefix(root, normalizedCandidate))
        return false;

    const std::filesystem::path relativePath = normalizedCandidate.lexically_relative(root);
    std::filesystem::path currentPath = root;
    for (const auto& component : relativePath)
    {
        if (component == ".")
            continue;
        if (component == "..")
            return false;

        currentPath /= component;
        std::error_code error;
        bool isSymlink = false;
        const auto status = GetPathStatusWithFallback(currentPath, error, isSymlink);
        if (!error)
        {
            if (isSymlink)
                return false;
        }
        else if (error == std::errc::no_such_file_or_directory)
        {
            break;
        }
        else if (IsProsperoPermissionStatusError(error))
        {
            // Treat Prospero's unreliable metadata result as unknown. Lexical
            // containment is still enforced above; known symlinks are rejected.
            continue;
        }
        else
        {
            return false;
        }
    }

    return true;
}

static bool IsSafeManagedWritePath(
    const std::filesystem::path& root,
    const std::filesystem::path& candidate)
{
    const std::filesystem::path normalizedRoot = NormalizeModPath(root);
    const std::filesystem::path normalizedCandidate = NormalizeModPath(candidate);
    if (!HasPathPrefix(normalizedRoot, normalizedCandidate))
        return false;

#if defined(__PROSPERO__)
    // Do not call weakly_canonical on /data paths: jailbroken Prospero can
    // report EPERM or resolve the app-data mount differently despite valid
    // access. Keep a lexical containment guard here; the managed mod scanner
    // separately rejects symlinked mod folders/config entries where status is
    // available.
    return true;
#else
    std::error_code error;
    const std::filesystem::path canonicalRoot = std::filesystem::weakly_canonical(normalizedRoot, error);
    if (error || canonicalRoot.lexically_normal() != normalizedRoot)
        return false;

    error.clear();
    const std::filesystem::path canonicalCandidate = std::filesystem::weakly_canonical(normalizedCandidate, error);
    if (error || canonicalCandidate.lexically_normal() != normalizedCandidate)
        return false;

    return HasPathPrefix(canonicalRoot, canonicalCandidate);
#endif
}

static std::filesystem::path PathFromUtf8(const std::string& value)
{
    return std::filesystem::path(std::u8string_view(reinterpret_cast<const char8_t*>(value.data()), value.size()));
}

static std::string PathToUtf8(const std::filesystem::path& path)
{
    const std::u8string value = path.u8string();
    return std::string(reinterpret_cast<const char*>(value.data()), value.size());
}

static std::string_view TrimAscii(std::string_view value)
{
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front())))
        value.remove_prefix(1);
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back())))
        value.remove_suffix(1);
    return value;
}

static bool IsSafeRelativeModPath(const std::filesystem::path& path)
{
    if (path.empty() || path.is_absolute() || path.has_root_name() || path.has_root_directory())
        return false;

    for (const auto& component : path.lexically_normal())
    {
        if (component == "..")
            return false;
    }

    return true;
}

static bool ParseModConfiguration(
    const std::filesystem::path& modIniFilePath,
    const IniFile& modIni,
    Mod& mod,
    std::filesystem::path& saveFilePath,
    bool restrictToModRoot)
{
    mod = {};
    saveFilePath.clear();
    mod.modIniPath = NormalizeModPath(modIniFilePath);
    mod.root = restrictToModRoot ? NormalizeModPath(modIniFilePath.parent_path()) : modIniFilePath.parent_path();
    mod.managed = restrictToModRoot;

    std::string modSaveFilePathU8;
    if (modIni.contains("Details") || modIni.contains("Filesystem")) // UMM
    {
        mod.type = ModType::UMM;
        mod.includeDirs.emplace_back(mod.root);
        mod.merge = modIni.getBool("Details", "Merge", modIni.getBool("Filesystem", "Merge", false));

        std::string readOnly = modIni.getString("Details", "Read-only", modIni.getString("Filesystem", "Read-only", std::string()));
        std::replace(readOnly.begin(), readOnly.end(), '\\', '/');
        std::string_view readOnlySplit = readOnly;

        while (!readOnlySplit.empty())
        {
            size_t index = readOnlySplit.find(',');
            if (index == std::string_view::npos)
            {
                mod.readOnly.emplace(readOnlySplit);
                break;
            }

            mod.readOnly.emplace(readOnlySplit.substr(0, index));
            readOnlySplit.remove_prefix(index + 1);
        }

        modSaveFilePathU8 = modIni.getString("Details", "Save", modIni.getString("Filesystem", "Save", std::string()));
    }
    else // HMM
    {
        mod.type = ModType::HMM;

        size_t includeDirCount = modIni.get<size_t>("Main", "IncludeDirCount", 0);
        if (restrictToModRoot)
            includeDirCount = std::min<size_t>(includeDirCount, 256);

        auto addIncludeDirectory = [&](std::string includeDirU8)
        {
            if (restrictToModRoot)
            {
                const std::string_view trimmedPath = TrimAscii(includeDirU8);
                if (trimmedPath.empty())
                    return;
                includeDirU8.assign(trimmedPath);
            }
            else if (includeDirU8.empty())
            {
                return;
            }

            std::replace(includeDirU8.begin(), includeDirU8.end(), '\\', '/');
            std::filesystem::path includeDir = mod.root / PathFromUtf8(includeDirU8);
            if (restrictToModRoot)
                includeDir = includeDir.lexically_normal();
            mod.includeDirs.emplace_back(std::move(includeDir));
        };

        for (size_t i = 0; i < includeDirCount; ++i)
            addIncludeDirectory(modIni.getString("Main", fmt::format("IncludeDir{}", i), ""));

        // Also accept the comma/semicolon list used by newer HMM manifests.
        if (includeDirCount == 0)
        {
            std::string includeDirList = modIni.getString("Main", "IncludeDir", "");
            std::string_view remaining = includeDirList;
            while (!remaining.empty())
            {
                const size_t separator = remaining.find_first_of(",;");
                addIncludeDirectory(std::string(remaining.substr(0, separator)));
                if (separator == std::string_view::npos)
                    break;
                remaining.remove_prefix(separator + 1);
            }
        }

        modSaveFilePathU8 = modIni.getString("Main", "SaveFile", std::string());
    }

    if (restrictToModRoot)
    {
        std::vector<std::filesystem::path> safeIncludeDirs;
        safeIncludeDirs.reserve(mod.includeDirs.size());
        for (const auto& includeDir : mod.includeDirs)
        {
            if (!IsSafeManagedModPath(mod, includeDir))
                continue;

            std::error_code error;
            const bool isDirectory = std::filesystem::is_directory(includeDir, error);
            if ((!error && isDirectory) || IsProsperoPermissionStatusError(error))
                safeIncludeDirs.emplace_back(includeDir);
        }
        mod.includeDirs = std::move(safeIncludeDirs);
    }

    if (!modSaveFilePathU8.empty())
    {
        std::replace(modSaveFilePathU8.begin(), modSaveFilePathU8.end(), '\\', '/');
        const std::filesystem::path relativeSavePath = PathFromUtf8(modSaveFilePathU8);
        if (restrictToModRoot && !IsSafeRelativeModPath(relativeSavePath))
            return false;

        saveFilePath = mod.root / relativeSavePath;
        if (restrictToModRoot)
            saveFilePath = saveFilePath.lexically_normal();

        // Save file paths in HMM mods are treated as folders.
        if (mod.type == ModType::HMM)
            saveFilePath /= "SYS-DATA";

        if (restrictToModRoot && !IsSafeManagedModPath(mod, saveFilePath))
            return false;
    }

    return true;
}

struct UserModCandidate
{
    std::string folderName;
    std::filesystem::path modIniPath;
};

#if defined(__PROSPERO__)
struct ModDatabaseEntry
{
    std::string id;
    std::string modIniPath;
};

struct ModDatabaseData
{
    std::vector<ModDatabaseEntry> activeMods;
    std::vector<std::filesystem::path> managedModOrder;
    std::vector<std::string> codes;
    std::set<std::string> ids;
    std::set<std::filesystem::path> paths;
};

static std::vector<UserModCandidate> g_userModCandidates;
static std::filesystem::path g_userModsRoot;
static std::filesystem::path g_managedModsDatabasePath;

static constexpr size_t MAX_MOD_DATABASE_ENTRY_COUNT = 4096;

static bool IsSafeIniId(std::string_view value)
{
    if (value.empty() || TrimAscii(value) != value || value.front() == '[' || value.front() == ';')
        return false;

    return value.find_first_of("=\r\n\"") == std::string_view::npos;
}

static bool IsSafeIniValue(std::string_view value)
{
    return value.find_first_of("\r\n\"") == std::string_view::npos;
}

static bool ReadRegularTextFile(const std::filesystem::path& filePath, std::string& contents)
{
    // Avoid a symlink_status preflight here: Prospero reports EPERM for some
    // absent files under /data even though opening/writing the directory works.
    std::ifstream input(filePath, std::ios::binary);
    if (!input)
        return false;

    contents.assign(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
    return input.eof() || static_cast<bool>(input);
}

static bool WriteTextAtomically(const std::filesystem::path& filePath, std::string_view contents)
{
    std::error_code error;
    std::string currentContents;
    if (ReadRegularTextFile(filePath, currentContents) && currentContents == contents)
        return true;

    std::filesystem::path temporaryPath = filePath;
    temporaryPath += ".tmp";
    std::filesystem::remove(temporaryPath, error);
    error.clear();

    {
        std::ofstream output(temporaryPath, std::ios::binary | std::ios::trunc);
        if (!output)
        {
            LOGFN_WARNING("Could not write temporary file '{}'.", temporaryPath.string());
            return false;
        }
        output.write(contents.data(), static_cast<std::streamsize>(contents.size()));
        if (!output)
        {
            output.close();
            std::filesystem::remove(temporaryPath, error);
            LOGFN_WARNING("Could not finish writing '{}'.", temporaryPath.string());
            return false;
        }
    }

    std::filesystem::rename(temporaryPath, filePath, error);
    if (error)
    {
        std::filesystem::remove(temporaryPath, error);
        LOGFN_WARNING("Could not replace '{}'.", filePath.string());
        return false;
    }

    return true;
}

static std::string RemoveJsonCommentsAndTrailingCommas(std::string_view input)
{
    std::string uncommented;
    uncommented.reserve(input.size());

    bool inString = false;
    bool escaped = false;
    bool inLineComment = false;
    bool inBlockComment = false;

    for (size_t i = 0; i < input.size(); ++i)
    {
        const char character = input[i];
        const char next = i + 1 < input.size() ? input[i + 1] : '\0';

        if (inString)
        {
            uncommented.push_back(character);
            if (escaped)
                escaped = false;
            else if (character == '\\')
                escaped = true;
            else if (character == '"')
                inString = false;
            continue;
        }

        if (inLineComment)
        {
            if (character == '\n' || character == '\r')
            {
                uncommented.push_back(character);
                inLineComment = false;
            }
            else
            {
                uncommented.push_back(' ');
            }
            continue;
        }

        if (inBlockComment)
        {
            if (character == '*' && next == '/')
            {
                uncommented.append("  ");
                ++i;
                inBlockComment = false;
            }
            else
            {
                uncommented.push_back(character == '\n' || character == '\r' ? character : ' ');
            }
            continue;
        }

        if (character == '"')
        {
            inString = true;
            uncommented.push_back(character);
        }
        else if (character == '/' && next == '/')
        {
            uncommented.append("  ");
            ++i;
            inLineComment = true;
        }
        else if (character == '/' && next == '*')
        {
            uncommented.append("  ");
            ++i;
            inBlockComment = true;
        }
        else
        {
            uncommented.push_back(character);
        }
    }

    if (uncommented.size() >= 3 &&
        static_cast<unsigned char>(uncommented[0]) == 0xEF &&
        static_cast<unsigned char>(uncommented[1]) == 0xBB &&
        static_cast<unsigned char>(uncommented[2]) == 0xBF)
    {
        uncommented.erase(0, 3);
    }

    std::string result;
    result.reserve(uncommented.size());
    inString = false;
    escaped = false;

    for (size_t i = 0; i < uncommented.size(); ++i)
    {
        const char character = uncommented[i];
        if (inString)
        {
            result.push_back(character);
            if (escaped)
                escaped = false;
            else if (character == '\\')
                escaped = true;
            else if (character == '"')
                inString = false;
            continue;
        }

        if (character == '"')
        {
            inString = true;
            result.push_back(character);
            continue;
        }

        if (character == ',')
        {
            size_t next = i + 1;
            while (next < uncommented.size() && std::isspace(static_cast<unsigned char>(uncommented[next])))
                ++next;
            if (next < uncommented.size() && (uncommented[next] == ']' || uncommented[next] == '}'))
                continue;
        }

        result.push_back(character);
    }

    return result;
}

static std::string JsonValueToText(const nlohmann::json& value)
{
    if (value.is_string())
        return value.get<std::string>();
    if (value.is_boolean())
        return value.get<bool>() ? "true" : "false";
    if (value.is_number())
        return value.dump();
    if (value.is_array())
    {
        std::string text;
        for (const auto& line : value)
        {
            if (!text.empty())
                text.push_back('\n');
            if (line.is_string())
                text += line.get<std::string>();
            else if (!line.is_null())
                text += line.dump();
        }
        return text;
    }
    return {};
}

static std::string GetJsonText(const nlohmann::json& object, std::string_view key, std::string defaultValue = {})
{
    if (!object.is_object())
        return defaultValue;

    const auto it = object.find(std::string(key));
    return it == object.end() ? defaultValue : JsonValueToText(*it);
}

static void ReadUserModInfo(const UserModCandidate& candidate, UserModInfo& info)
{
    info = {};
    info.folderName = candidate.folderName;
    info.title = candidate.folderName;

    IniFile modIni;
    if (!modIni.read(candidate.modIniPath))
        return;

    info.title = modIni.getString("Desc", "Title", candidate.folderName);
    info.description = modIni.getString("Desc", "Description", std::string());
    info.author = modIni.getString("Desc", "Author", std::string());

    const std::string schemaPathText = modIni.getString("Main", "ConfigSchemaFile", std::string());
    if (schemaPathText.empty())
        return;

    const std::filesystem::path modRoot = NormalizeModPath(candidate.modIniPath.parent_path());
    const std::filesystem::path schemaRelativePath = PathFromUtf8(schemaPathText);
    if (!IsSafeRelativeModPath(schemaRelativePath))
        return;

    Mod managedMod;
    managedMod.root = modRoot;
    managedMod.managed = true;
    const std::filesystem::path schemaPath = (modRoot / schemaRelativePath).lexically_normal();
    if (!IsSafeManagedModPath(managedMod, schemaPath))
        return;

    std::string schemaContents;
    if (!ReadRegularTextFile(schemaPath, schemaContents))
        return;

    nlohmann::json schema = nlohmann::json::parse(RemoveJsonCommentsAndTrailingCommas(schemaContents), nullptr, false);
    if (schema.is_discarded() || !schema.is_object())
        return;

    std::string iniFileText = GetJsonText(schema, "IniFile", "mod.ini");
    if (iniFileText.empty())
        iniFileText = "mod.ini";
    const std::filesystem::path iniRelativePath = PathFromUtf8(iniFileText);
    if (!IsSafeRelativeModPath(iniRelativePath))
        return;

    const std::filesystem::path iniPath = (modRoot / iniRelativePath).lexically_normal();
    if (!IsSafeManagedModPath(managedMod, iniPath))
        return;

    IniFile optionIni;
    optionIni.read(iniPath);

    const auto groupsIt = schema.find("Groups");
    const auto enumsIt = schema.find("Enums");
    if (groupsIt == schema.end() || !groupsIt->is_array())
        return;

    for (const auto& group : *groupsIt)
    {
        if (!group.is_object())
            continue;

        const std::string groupName = GetJsonText(group, "Name");
        const std::string groupDisplayName = GetJsonText(group, "DisplayName", groupName);
        const auto elementsIt = group.find("Elements");
        if (groupName.empty() || elementsIt == group.end() || !elementsIt->is_array())
            continue;

        for (const auto& element : *elementsIt)
        {
            if (!element.is_object())
                continue;

            ModConfigOption option;
            option.sectionName = groupName;
            option.groupDisplayName = groupDisplayName;
            option.name = GetJsonText(element, "Name");
            option.displayName = GetJsonText(element, "DisplayName", option.name);
            option.description = GetJsonText(element, "Description");
            option.type = GetJsonText(element, "Type");
            option.iniFile = iniFileText;
            option.defaultValue = GetJsonText(element, "DefaultValue");
            if (option.name.empty() || !IsSafeIniId(option.name) || !IsSafeIniId(option.sectionName))
                continue;

            option.value = optionIni.getString(option.sectionName, option.name, option.defaultValue);

            if (enumsIt != schema.end() && enumsIt->is_object())
            {
                const auto enumIt = enumsIt->find(option.type);
                if (enumIt != enumsIt->end() && enumIt->is_array())
                {
                    for (const auto& choiceJson : *enumIt)
                    {
                        if (!choiceJson.is_object())
                            continue;

                        ModConfigChoice choice;
                        choice.displayName = GetJsonText(choiceJson, "DisplayName");
                        choice.value = GetJsonText(choiceJson, "Value");
                        choice.description = GetJsonText(choiceJson, "Description");
                        if (choice.displayName.empty() || !IsSafeIniValue(choice.value))
                            continue;
                        option.choices.emplace_back(std::move(choice));
                    }
                }
            }

            if (option.choices.empty())
            {
                std::string normalizedType = option.type;
                std::transform(normalizedType.begin(), normalizedType.end(), normalizedType.begin(), [](unsigned char character)
                {
                    return static_cast<char>(std::tolower(character));
                });

                if (normalizedType == "bool" || normalizedType == "boolean")
                {
                    option.choices.push_back({ "Off", "false", {} });
                    option.choices.push_back({ "On", "true", {} });
                }
            }

            info.options.emplace_back(std::move(option));
        }
    }
}

static std::filesystem::path FindConfiguredModsDatabase(const std::filesystem::path& userPath)
{
    const std::array<std::filesystem::path, 2> configPaths =
    {
        userPath / "cpkredir.ini",
        GetGamePath() / "cpkredir.ini"
    };

    for (const auto& configPath : configPaths)
    {
        IniFile configIni;
        if (!configIni.read(configPath))
            continue;

        const std::string databasePathU8 = configIni.getString("CPKREDIR", "ModsDbIni", "");
        if (databasePathU8.empty())
            continue;

        std::string normalizedDatabasePathU8 = databasePathU8;
        std::replace(normalizedDatabasePathU8.begin(), normalizedDatabasePathU8.end(), '\\', '/');
        std::filesystem::path databasePath = PathFromUtf8(normalizedDatabasePathU8);
        if (databasePath.is_relative())
            databasePath = configPath.parent_path() / databasePath;
        return NormalizeModPath(databasePath);
    }

    return {};
}

static void ReadExistingModDatabase(
    const std::filesystem::path& databasePath,
    const std::filesystem::path& modsRoot,
    ModDatabaseData& database)
{
    if (databasePath.empty())
        return;

    IniFile source;
    if (!source.read(databasePath))
        return;

    const size_t activeModCount = std::min<size_t>(
        source.get<size_t>("Main", "ActiveModCount", 0),
        MAX_MOD_DATABASE_ENTRY_COUNT);

    for (size_t i = 0; i < activeModCount; ++i)
    {
        const std::string id = source.getString("Main", fmt::format("ActiveMod{}", i), "");
        const std::string modIniPathU8 = source.getString("Mods", id, "");
        if (!IsSafeIniId(id) || modIniPathU8.empty() || !IsSafeIniValue(modIniPathU8))
            continue;

        std::string normalizedModIniPathU8 = modIniPathU8;
        std::replace(normalizedModIniPathU8.begin(), normalizedModIniPathU8.end(), '\\', '/');
        std::filesystem::path resolvedPath = PathFromUtf8(normalizedModIniPathU8);
        if (resolvedPath.is_relative())
            resolvedPath = databasePath.parent_path() / resolvedPath;
        resolvedPath = NormalizeModPath(resolvedPath);

        // Managed entries are regenerated from the current valid folder scan,
        // but their prior order is retained when rebuilding ModsDB.ini.
        if (HasPathPrefix(modsRoot, resolvedPath))
        {
            if (!database.paths.contains(resolvedPath))
            {
                database.paths.emplace(resolvedPath);
                database.managedModOrder.emplace_back(resolvedPath);
            }
            continue;
        }

        if (database.ids.contains(id) || database.paths.contains(resolvedPath))
            continue;

        database.ids.emplace(id);
        database.paths.emplace(resolvedPath);
        database.activeMods.push_back({ id, PathToUtf8(resolvedPath) });
    }

    const size_t codeCount = std::min<size_t>(
        source.get<size_t>("Codes", "CodeCount", 0),
        MAX_MOD_DATABASE_ENTRY_COUNT);
    for (size_t i = 0; i < codeCount; ++i)
    {
        std::string code = source.getString("Codes", fmt::format("Code{}", i), "");
        if (!code.empty() && IsSafeIniValue(code))
            database.codes.emplace_back(std::move(code));
    }
}

static bool WriteManagedModDatabase(
    const std::filesystem::path& databasePath,
    const ModDatabaseData& database)
{
    std::ostringstream contents;
    contents << "[Main]\n";
    contents << "ActiveModCount = " << database.activeMods.size() << "\n";
    for (size_t i = 0; i < database.activeMods.size(); ++i)
        contents << "ActiveMod" << i << " = \"" << database.activeMods[i].id << "\"\n";

    if (!database.activeMods.empty())
    {
        contents << "\n[Mods]\n";
        for (const auto& mod : database.activeMods)
            contents << mod.id << " = \"" << mod.modIniPath << "\"\n";
    }

    if (!database.codes.empty())
    {
        contents << "\n[Codes]\n";
        contents << "CodeCount = " << database.codes.size() << "\n";
        for (size_t i = 0; i < database.codes.size(); ++i)
            contents << "Code" << i << " = \"" << database.codes[i] << "\"\n";
    }

    const std::string serialized = contents.str();
    return WriteTextAtomically(databasePath, serialized);
}

static bool ReadCpkredirTemplate(const std::filesystem::path& userPath, std::string& contents)
{
    const std::filesystem::path userConfigPath = userPath / "cpkredir.ini";
    if (ReadRegularTextFile(userConfigPath, contents))
        return true;

    // A missing/unreadable per-user config is a normal first-run case. Start
    // from installation defaults when available, then create the user config.
    contents.clear();
    ReadRegularTextFile(GetGamePath() / "cpkredir.ini", contents);
    return true;
}

static std::string BuildManagedCpkredirConfig(
    std::string_view existingContents,
    const std::filesystem::path& databasePath)
{
    const std::string databasePathU8 = PathToUtf8(NormalizeModPath(databasePath));
    const std::string databaseProperty = "ModsDbIni = \"" + databasePathU8 + "\"";
    const std::string enabledProperty = "Enabled = 1";

    if (existingContents.empty())
    {
        return "[CPKREDIR]\nEnabled = 1\nEnableSaveFileRedirection = 0\n" + databaseProperty + "\n";
    }

    std::string normalizedExistingContents(existingContents);
    if (normalizedExistingContents.size() >= 3 &&
        static_cast<unsigned char>(normalizedExistingContents[0]) == 0xEF &&
        static_cast<unsigned char>(normalizedExistingContents[1]) == 0xBB &&
        static_cast<unsigned char>(normalizedExistingContents[2]) == 0xBF)
    {
        normalizedExistingContents.erase(0, 3);
    }

    std::istringstream input{ normalizedExistingContents };
    std::vector<std::string> lines;
    std::string line;
    bool inCpkredirSection = false;
    bool foundCpkredirSection = false;
    bool foundDatabaseProperty = false;
    bool foundEnabledProperty = false;

    auto appendRequiredPropertiesIfNeeded = [&]()
    {
        if (!inCpkredirSection)
            return;

        if (!foundEnabledProperty)
        {
            lines.emplace_back(enabledProperty);
            foundEnabledProperty = true;
        }
        if (!foundDatabaseProperty)
        {
            lines.emplace_back(databaseProperty);
            foundDatabaseProperty = true;
        }
    };

    while (std::getline(input, line))
    {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();

        const std::string_view trimmedLine = TrimAscii(line);
        if (!trimmedLine.empty() && trimmedLine.front() == '[')
        {
            appendRequiredPropertiesIfNeeded();

            const size_t closeBracket = trimmedLine.find(']');
            const std::string sectionName = closeBracket == std::string_view::npos
                ? std::string()
                : std::string(trimmedLine.substr(1, closeBracket - 1));

            inCpkredirSection = sectionName == "CPKREDIR";
            if (inCpkredirSection)
            {
                foundCpkredirSection = true;
                foundDatabaseProperty = false;
                foundEnabledProperty = false;
            }

            lines.emplace_back(line);
            continue;
        }

        if (inCpkredirSection)
        {
            const size_t equals = line.find('=');
            if (equals != std::string::npos)
            {
                std::string propertyName(TrimAscii(std::string_view(line).substr(0, equals)));
                std::transform(propertyName.begin(), propertyName.end(), propertyName.begin(), [](unsigned char character)
                {
                    return static_cast<char>(std::tolower(character));
                });
                if (propertyName == "enabled")
                {
                    lines.emplace_back(enabledProperty);
                    foundEnabledProperty = true;
                    continue;
                }
                if (propertyName == "modsdbini")
                {
                    lines.emplace_back(databaseProperty);
                    foundDatabaseProperty = true;
                    continue;
                }
            }
        }

        lines.emplace_back(line);
    }

    appendRequiredPropertiesIfNeeded();

    if (!foundCpkredirSection)
    {
        if (!lines.empty() && !lines.back().empty())
            lines.emplace_back();
        lines.emplace_back("[CPKREDIR]");
        lines.emplace_back("Enabled = 1");
        lines.emplace_back("EnableSaveFileRedirection = 0");
        lines.emplace_back(databaseProperty);
    }

    std::ostringstream output;
    for (const auto& outputLine : lines)
        output << outputLine << '\n';
    return output.str();
}

static bool EnsureManagedCpkredirConfig(
    const std::filesystem::path& userPath,
    const std::filesystem::path& databasePath)
{
    const std::string databasePathU8 = PathToUtf8(NormalizeModPath(databasePath));
    if (!IsSafeIniValue(databasePathU8))
    {
        LOGFN_WARNING("Cannot write unsafe ModsDB path '{}'.", databasePathU8);
        return false;
    }
    if (!IsSafeManagedWritePath(userPath, userPath / "cpkredir.ini"))
    {
        LOGFN_WARNING("Refusing to write cpkredir.ini through a symlinked user path '{}'.", userPath.string());
        return false;
    }

    std::string existingContents;
    if (!ReadCpkredirTemplate(userPath, existingContents))
        return false;

    const std::string updatedContents = BuildManagedCpkredirConfig(existingContents, databasePath);
    return WriteTextAtomically(userPath / "cpkredir.ini", updatedContents);
}

static std::string MakeUniqueModDatabaseId(std::string_view folderName, const std::set<std::string>& usedIds)
{
    std::string candidate(folderName);
    if (!usedIds.contains(candidate))
        return candidate;

    for (size_t suffix = 1; suffix < MAX_MOD_DATABASE_ENTRY_COUNT; ++suffix)
    {
        candidate = fmt::format("{} [UR {}]", folderName, suffix);
        if (!usedIds.contains(candidate) && IsSafeIniId(candidate))
            return candidate;
    }

    return {};
}

static void LoadUserDirectoryMods(const std::filesystem::path& userPath)
{
    ModLoader::s_detectedModCount.store(0, std::memory_order_relaxed);
    ModLoader::s_enabledModCount.store(0, std::memory_order_relaxed);
    ModLoader::s_startupLoadingObserved.store(false, std::memory_order_relaxed);
    // Keep startup status enabled even when the scan finds zero mods or exits
    // early, so startup still reports a zero count instead of showing nothing.
    ModLoader::s_showStartupStatus.store(true, std::memory_order_release);
    g_userMods.clear();
    g_userModCandidates.clear();
    g_managedModsDatabasePath.clear();
    const std::filesystem::path modsRoot = NormalizeModPath(userPath / "mods");
    g_userModsRoot = modsRoot;
    std::error_code error;
    std::filesystem::create_directories(modsRoot, error);
    if (error)
    {
        LOGFN_WARNING("Could not create mods directory '{}': {}.", modsRoot.string(), error.message());
        return;
    }

    // Do not preflight /data with status or read/write permission probes on
    // Prospero. The directory scan and actual writes below report real errors.
    const std::filesystem::path existingDatabasePath = FindConfiguredModsDatabase(userPath);
    std::vector<UserModCandidate> candidates;
    for (std::filesystem::directory_iterator it(
             modsRoot,
             std::filesystem::directory_options::skip_permission_denied,
             error), end;
         !error && it != end;
         it.increment(error))
    {
        const std::string folderName = PathToUtf8(it->path().filename());
        if (folderName == "ModsDB.ini" || folderName == "ModsDB.ini.tmp")
            continue;

        std::error_code entryError;
        bool isSymlink = false;
        bool isDirectoryUnknown = false;
        const auto entryStatus = GetPathStatusWithFallback(it->path(), entryError, isSymlink);
        if (entryError)
        {
            if (IsProsperoPermissionStatusError(entryError))
            {
                entryError.clear();
                isDirectoryUnknown = true;
            }
            else
            {
                LOGFN_WARNING("Could not inspect mod directory entry '{}': {}.", it->path().string(), entryError.message());
                continue;
            }
        }
        if (!isDirectoryUnknown && !std::filesystem::is_directory(entryStatus))
            continue;
        if (isSymlink)
        {
            LOGFN_WARNING("Skipping symlinked mod folder '{}'.", it->path().string());
            continue;
        }
        if (!IsSafeManagedWritePath(modsRoot, it->path() / "mod.ini"))
        {
            LOGFN_WARNING("Skipping mod folder '{}': managed paths resolve through a symlink.", it->path().string());
            continue;
        }
        std::string foldedFolderName = folderName;
        std::transform(foldedFolderName.begin(), foldedFolderName.end(), foldedFolderName.begin(), [](unsigned char character)
        {
            return static_cast<char>(std::tolower(character));
        });
        if (foldedFolderName == "bak")
            continue;

        if (!IsSafeIniId(folderName))
        {
            LOGFN_WARNING("Skipping mod folder '{}': its folder name cannot be represented safely in ModsDB.ini.", folderName);
            continue;
        }

        const std::filesystem::path modIniFilePath = it->path() / "mod.ini";
        entryError.clear();
        isSymlink = false;
        bool modIniStatusUnknown = false;
        const auto modIniStatus = GetPathStatusWithFallback(modIniFilePath, entryError, isSymlink);
        if (entryError)
        {
            if (IsProsperoPermissionStatusError(entryError))
            {
                entryError.clear();
                modIniStatusUnknown = true;
            }
            else
            {
                LOGFN_WARNING("Could not inspect mod.ini for '{}': {}.", folderName, entryError.message());
                continue;
            }
        }
        if (!modIniStatusUnknown && (!std::filesystem::is_regular_file(modIniStatus) || isSymlink))
        {
            LOGFN_WARNING("Skipping '{}': mod.ini is missing, not a regular file, or a symlink.", folderName);
            continue;
        }

        IniFile modIni;
        if (!modIni.read(modIniFilePath))
        {
            LOGFN_WARNING("Skipping mod folder '{}': mod.ini could not be read.", folderName);
            continue;
        }

        Mod mod;
        std::filesystem::path saveFilePath;
        if (!ParseModConfiguration(modIniFilePath, modIni, mod, saveFilePath, true))
        {
            LOGFN_WARNING("Skipping '{}': mod.ini contains an invalid managed path.", folderName);
            continue;
        }
        if (mod.includeDirs.empty())
        {
            LOGFN_WARNING("Skipping '{}': mod.ini has no valid include directories.", folderName);
            continue;
        }
        candidates.push_back({ folderName, NormalizeModPath(modIniFilePath) });
    }

    if (error)
        LOGFN_WARNING("Mod folder scan stopped: {}.", error.message());

    std::sort(candidates.begin(), candidates.end(), [](const UserModCandidate& left, const UserModCandidate& right)
    {
        return left.folderName < right.folderName;
    });

    const uint32_t validModCount = static_cast<uint32_t>(candidates.size());
    ModLoader::s_detectedModCount.store(validModCount, std::memory_order_release);

    ModDatabaseData database;
    ReadExistingModDatabase(existingDatabasePath, modsRoot, database);

    // ModsDB.ini is also the persistent load-order store. Keep valid folders in
    // their previously saved order and append newly discovered folders by name.
    std::vector<UserModCandidate> orderedCandidates;
    std::set<std::filesystem::path> alreadyOrdered;
    orderedCandidates.reserve(candidates.size());
    for (const auto& savedPath : database.managedModOrder)
    {
        const auto candidate = std::find_if(candidates.begin(), candidates.end(), [&](const UserModCandidate& value)
        {
            return value.modIniPath == savedPath;
        });
        if (candidate != candidates.end() && alreadyOrdered.emplace(candidate->modIniPath).second)
            orderedCandidates.emplace_back(*candidate);
    }
    for (const auto& candidate : candidates)
    {
        if (alreadyOrdered.emplace(candidate.modIniPath).second)
            orderedCandidates.emplace_back(candidate);
    }
    candidates = std::move(orderedCandidates);

    for (const auto& candidate : candidates)
    {
        const std::string id = MakeUniqueModDatabaseId(candidate.folderName, database.ids);
        const std::string modIniPathU8 = PathToUtf8(candidate.modIniPath);
        if (id.empty() || !IsSafeIniValue(modIniPathU8))
        {
            LOGFN_WARNING("Could not add mod folder '{}' to ModsDB.ini.", candidate.folderName);
            continue;
        }

        database.ids.emplace(id);
        database.paths.emplace(candidate.modIniPath);
        database.activeMods.push_back({ id, modIniPathU8 });
    }

    const std::filesystem::path managedDatabasePath = NormalizeModPath(modsRoot / "ModsDB.ini");
    if (!WriteManagedModDatabase(managedDatabasePath, database))
        return;

    if (!EnsureManagedCpkredirConfig(userPath, managedDatabasePath))
        return;

    g_userModCandidates = candidates;
    g_managedModsDatabasePath = managedDatabasePath;
    g_userMods.reserve(candidates.size());
    for (const auto& candidate : candidates)
    {
        UserModInfo info;
        ReadUserModInfo(candidate, info);
        g_userMods.emplace_back(std::move(info));
    }

    ModLoader::s_enabledModCount.store(static_cast<uint32_t>(database.activeMods.size()), std::memory_order_relaxed);
    ModLoader::s_showStartupStatus.store(true, std::memory_order_release);
    LOGFN_UTILITY("Detected {} valid user mod folder(s); all are enabled in '{}'.", candidates.size(), managedDatabasePath.string());
}

static std::string TrimIniLine(std::string_view value)
{
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front())))
        value.remove_prefix(1);
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back())))
        value.remove_suffix(1);
    return std::string(value);
}

static bool SetIniTextProperty(
    std::string& contents,
    std::string_view sectionName,
    std::string_view propertyName,
    std::string_view value)
{
    if (!IsSafeIniId(sectionName) || !IsSafeIniId(propertyName) || !IsSafeIniValue(value))
        return false;

    bool hasBom = contents.size() >= 3 &&
        static_cast<unsigned char>(contents[0]) == 0xEF &&
        static_cast<unsigned char>(contents[1]) == 0xBB &&
        static_cast<unsigned char>(contents[2]) == 0xBF;
    if (hasBom)
        contents.erase(0, 3);

    std::vector<std::string> lines;
    std::istringstream input(contents);
    std::string line;
    while (std::getline(input, line))
    {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        lines.emplace_back(std::move(line));
    }

    bool inSection = false;
    bool sectionExists = false;
    bool propertyExists = false;
    size_t insertionIndex = lines.size();
    const std::string propertyLine = fmt::format("{} = \"{}\"", propertyName, value);

    for (size_t i = 0; i < lines.size(); ++i)
    {
        const std::string_view trimmed = TrimAscii(lines[i]);
        if (!trimmed.empty() && trimmed.front() == '[')
        {
            if (inSection)
                insertionIndex = i;

            const size_t closeBracket = trimmed.find(']');
            const std::string_view currentSection = closeBracket == std::string_view::npos
                ? std::string_view{}
                : trimmed.substr(1, closeBracket - 1);
            inSection = currentSection == sectionName;
            if (inSection)
            {
                sectionExists = true;
                insertionIndex = i + 1;
            }
            continue;
        }

        if (!inSection || trimmed.empty() || trimmed.front() == ';')
            continue;

        const size_t equals = lines[i].find('=');
        if (equals == std::string::npos)
            continue;

        std::string existingName = TrimIniLine(std::string_view(lines[i]).substr(0, equals));
        if (existingName.size() >= 2 && existingName.front() == '"' && existingName.back() == '"')
            existingName = existingName.substr(1, existingName.size() - 2);
        if (existingName == propertyName)
        {
            lines[i] = propertyLine;
            propertyExists = true;
        }
    }

    if (!propertyExists)
    {
        if (!sectionExists)
        {
            if (!lines.empty() && !lines.back().empty())
                lines.emplace_back();
            lines.emplace_back(fmt::format("[{}]", sectionName));
            insertionIndex = lines.size();
        }
        lines.insert(lines.begin() + std::min(insertionIndex, lines.size()), propertyLine);
    }

    std::ostringstream output;
    if (hasBom)
        output << "\xEF\xBB\xBF";
    for (const auto& outputLine : lines)
        output << outputLine << '\n';
    contents = output.str();
    return true;
}

static const UserModCandidate* FindUserModCandidate(std::string_view folderName)
{
    const auto it = std::find_if(g_userModCandidates.begin(), g_userModCandidates.end(), [&](const UserModCandidate& candidate)
    {
        return candidate.folderName == folderName;
    });
    return it == g_userModCandidates.end() ? nullptr : &*it;
}

static void RollBackFileWrites(const std::vector<std::tuple<std::filesystem::path, bool, std::string>>& backups)
{
    for (auto it = backups.rbegin(); it != backups.rend(); ++it)
    {
        const auto& [path, existed, originalContents] = *it;
        if (existed)
        {
            (void)WriteTextAtomically(path, originalContents);
        }
        else
        {
            std::error_code error;
            std::filesystem::remove(path, error);
        }
    }
}

static bool SaveUserModChanges(const std::vector<UserModInfo>& mods, std::string& errorMessage)
{
    if (g_managedModsDatabasePath.empty() || g_userModsRoot.empty())
    {
        errorMessage = "The managed mods database is not available.";
        return false;
    }

    if (mods.size() != g_userMods.size())
    {
        errorMessage = "The mod list changed while the Mods menu was open.";
        return false;
    }
    if (!IsSafeManagedWritePath(g_userModsRoot, g_managedModsDatabasePath))
    {
        errorMessage = "The managed ModsDB.ini path is not a safe regular user-data path.";
        return false;
    }

    struct FileUpdate
    {
        std::filesystem::path path;
        std::vector<std::tuple<std::string, std::string, std::string>> properties;
        bool existed = false;
        std::string originalContents;
        std::string newContents;
    };

    std::vector<FileUpdate> fileUpdates;
    std::set<std::string> folderNames;
    for (const auto& mod : mods)
    {
        if (!folderNames.emplace(mod.folderName).second)
        {
            errorMessage = "A mod folder appears more than once in the list.";
            return false;
        }

        const UserModCandidate* candidate = FindUserModCandidate(mod.folderName);
        if (candidate == nullptr)
        {
            errorMessage = fmt::format("The mod folder '{}' is no longer available.", mod.folderName);
            return false;
        }

        const auto originalMod = std::find_if(g_userMods.begin(), g_userMods.end(), [&](const UserModInfo& value)
        {
            return value.folderName == mod.folderName;
        });
        if (originalMod == g_userMods.end() || mod.options.size() != originalMod->options.size())
        {
            errorMessage = fmt::format("The configuration schema for '{}' changed while the menu was open.", mod.folderName);
            return false;
        }

        if (!IsSafeManagedWritePath(g_userModsRoot, candidate->modIniPath))
        {
            errorMessage = fmt::format("The mod folder '{}' no longer resolves to a safe user-data path.", mod.folderName);
            return false;
        }

        const std::filesystem::path modRoot = NormalizeModPath(candidate->modIniPath.parent_path());
        Mod managedMod;
        managedMod.root = modRoot;
        managedMod.managed = true;

        for (const auto& option : mod.options)
        {
            const auto originalOption = std::find_if(originalMod->options.begin(), originalMod->options.end(), [&](const ModConfigOption& value)
            {
                return value.sectionName == option.sectionName && value.name == option.name && value.iniFile == option.iniFile;
            });
            if (originalOption == originalMod->options.end())
            {
                errorMessage = fmt::format("The option '{}' in '{}' is no longer available.", option.name, mod.folderName);
                return false;
            }
            if (option.value == originalOption->value)
                continue;

            if (!option.choices.empty() && std::none_of(option.choices.begin(), option.choices.end(), [&](const ModConfigChoice& choice)
                {
                    return choice.value == option.value;
                }))
            {
                errorMessage = fmt::format("The selected value for '{}' is not in its schema.", option.displayName);
                return false;
            }
            if (!IsSafeIniId(option.sectionName) || !IsSafeIniId(option.name) || !IsSafeIniValue(option.value))
            {
                errorMessage = fmt::format("The selected value for '{}' cannot be saved safely.", option.displayName);
                return false;
            }

            const std::filesystem::path relativeIniPath = PathFromUtf8(option.iniFile);
            if (!IsSafeRelativeModPath(relativeIniPath))
            {
                errorMessage = fmt::format("The config file for '{}' is not a safe relative path.", mod.folderName);
                return false;
            }

            const std::filesystem::path targetPath = (modRoot / relativeIniPath).lexically_normal();
            if (!IsSafeManagedModPath(managedMod, targetPath) ||
                !IsSafeManagedWritePath(g_userModsRoot, targetPath))
            {
                errorMessage = fmt::format("The config file for '{}' escapes its safe mod folder.", mod.folderName);
                return false;
            }

            auto update = std::find_if(fileUpdates.begin(), fileUpdates.end(), [&](const FileUpdate& value)
            {
                return value.path == targetPath;
            });
            if (update == fileUpdates.end())
            {
                FileUpdate newUpdate;
                newUpdate.path = targetPath;
                fileUpdates.emplace_back(std::move(newUpdate));
                update = std::prev(fileUpdates.end());
            }
            update->properties.emplace_back(option.sectionName, option.name, option.value);
        }
    }

    if (folderNames.size() != g_userMods.size())
    {
        errorMessage = "The mod list is incomplete.";
        return false;
    }

    for (auto& update : fileUpdates)
    {
        if (!IsSafeManagedWritePath(g_userModsRoot, update.path))
        {
            errorMessage = fmt::format("Refusing to edit '{}' because its resolved path is not inside the managed mods tree.", update.path.string());
            return false;
        }

#if defined(__PROSPERO__)
        // Do not gate PS5 edits on symlink_status/status permission probes.
        // Read the existing INI directly so its unrelated properties are kept.
        update.existed = ReadRegularTextFile(update.path, update.originalContents);
#else
        std::error_code statusError;
        bool isSymlink = false;
        const auto status = GetPathStatusWithFallback(update.path, statusError, isSymlink);
        if (statusError && statusError != std::errc::no_such_file_or_directory)
        {
            errorMessage = fmt::format("Could not inspect '{}': {}.", update.path.string(), statusError.message());
            return false;
        }
        update.existed = !statusError && std::filesystem::exists(status);
        if (isSymlink || (update.existed && !std::filesystem::is_regular_file(status)))
        {
            errorMessage = fmt::format("Refusing to edit a non-regular mod config file '{}'.", update.path.string());
            return false;
        }
        if (update.existed && !ReadRegularTextFile(update.path, update.originalContents))
        {
            errorMessage = fmt::format("Could not read mod config file '{}'.", update.path.string());
            return false;
        }
#endif

        update.newContents = update.originalContents;
        for (const auto& [sectionName, propertyName, value] : update.properties)
        {
            if (!SetIniTextProperty(update.newContents, sectionName, propertyName, value))
            {
                errorMessage = fmt::format("Could not update '{}' in '{}'.", propertyName, update.path.string());
                return false;
            }
        }
    }

    ModDatabaseData database;
    ReadExistingModDatabase(g_managedModsDatabasePath, g_userModsRoot, database);
    std::vector<UserModCandidate> orderedCandidates;
    orderedCandidates.reserve(mods.size());
    for (const auto& mod : mods)
    {
        const UserModCandidate* candidate = FindUserModCandidate(mod.folderName);
        if (candidate == nullptr)
        {
            errorMessage = fmt::format("The mod folder '{}' is no longer available.", mod.folderName);
            return false;
        }

        const std::string id = MakeUniqueModDatabaseId(candidate->folderName, database.ids);
        const std::string modIniPathU8 = PathToUtf8(candidate->modIniPath);
        if (id.empty() || !IsSafeIniValue(modIniPathU8))
        {
            errorMessage = fmt::format("Could not save the load order for '{}'.", candidate->folderName);
            return false;
        }

        database.ids.emplace(id);
        database.paths.emplace(candidate->modIniPath);
        database.activeMods.push_back({ id, modIniPathU8 });
        orderedCandidates.emplace_back(*candidate);
    }

    std::vector<std::tuple<std::filesystem::path, bool, std::string>> writtenFiles;
    for (const auto& update : fileUpdates)
    {
        if (update.newContents == update.originalContents && update.existed)
            continue;
        if (!IsSafeManagedWritePath(g_userModsRoot, update.path))
        {
            RollBackFileWrites(writtenFiles);
            errorMessage = fmt::format("Refusing to write '{}': its resolved path is outside the managed mods tree.", update.path.string());
            return false;
        }
        if (!WriteTextAtomically(update.path, update.newContents))
        {
            RollBackFileWrites(writtenFiles);
            errorMessage = fmt::format("Could not save mod configuration '{}'.", update.path.string());
            return false;
        }
        writtenFiles.emplace_back(update.path, update.existed, update.originalContents);
    }

    if (!IsSafeManagedWritePath(g_userModsRoot, g_managedModsDatabasePath) ||
        !WriteManagedModDatabase(g_managedModsDatabasePath, database))
    {
        RollBackFileWrites(writtenFiles);
        errorMessage = "Could not save the mod load order to ModsDB.ini.";
        return false;
    }

    g_userMods = mods;
    g_userModCandidates = std::move(orderedCandidates);
    return true;
}
#endif

const std::vector<UserModInfo>& ModLoader::GetUserMods()
{
    return g_userMods;
}

bool ModLoader::CommitUserMods(const std::vector<UserModInfo>& mods, std::string& errorMessage)
{
#if defined(__PROSPERO__)
    return SaveUserModChanges(mods, errorMessage);
#else
    (void)mods;
    errorMessage = "The Mods menu is currently supported only on PS5.";
    return false;
#endif
}

std::filesystem::path ModLoader::ResolvePath(std::string_view path)
{
    std::string_view root;

    size_t sepIndex = path.find(":\\");
    if (sepIndex != std::string_view::npos)
    {
        root = path.substr(0, sepIndex);
        path.remove_prefix(sepIndex + 2);
    }

    if (root == "save")
    {
        if (!ModLoader::s_saveFilePath.empty())
        {
            if (path == "SYS-DATA")
                return ModLoader::s_saveFilePath;
            else
                return ModLoader::s_saveFilePath.parent_path() / path;
        }

        return {};
    }

    if (g_mods.empty())
        return {};

    thread_local xxHashMap<std::filesystem::path> s_cache;
    thread_local uint64_t s_cachedGeneration = std::numeric_limits<uint64_t>::max();
    const uint64_t generation = g_modGeneration.load(std::memory_order_acquire);
    if (s_cachedGeneration != generation)
    {
        s_cache.clear();
        s_cachedGeneration = generation;
    }

    XXH64_hash_t hash = XXH3_64bits(path.data(), path.size());
    auto findResult = s_cache.find(hash);
    if (findResult != s_cache.end())
        return findResult->second;

    std::string pathStr(path);
    std::replace(pathStr.begin(), pathStr.end(), '\\', '/');
    std::filesystem::path fsPath(std::move(pathStr));

    bool canBeMerged = 
        path.find(".arl") == (path.size() - 4) ||
        path.find(".ar.") == (path.size() - 6) ||
        path.find(".ar") == (path.size() - 3);

    for (auto& mod : g_mods)
    {
        if (mod.type == ModType::UMM && mod.merge && canBeMerged && !mod.readOnly.contains(fsPath))
            continue;

        for (auto& includeDir : mod.includeDirs)
        {
            std::filesystem::path modPath = includeDir / fsPath;
            if (IsSafeManagedModPath(mod, modPath) && std::filesystem::exists(modPath))
                return s_cache.emplace(hash, modPath).first->second;
        }
    }

    return s_cache.emplace(hash, std::filesystem::path{}).first->second;
}

std::vector<std::filesystem::path>* ModLoader::GetIncludeDirectories(size_t modIndex)
{
    return modIndex < g_mods.size() ? &g_mods[modIndex].includeDirs : nullptr;
}

void ModLoader::Init()
{
    g_mods.clear();
    g_userMods.clear();
    g_modGeneration.fetch_add(1, std::memory_order_release);
    s_saveFilePath.clear();
    s_isLogTypeConsole = false;

    const std::filesystem::path& userPath = GetUserPath();

#if defined(__PROSPERO__)
    // Generate the standard per-user CPKREDIR database before the regular loader reads it.
    LoadUserDirectoryMods(userPath);
#endif

    IniFile configIni;
    if (!configIni.read(userPath / "cpkredir.ini"))
    {
        configIni = {};

        if (!configIni.read(GetGamePath() / "cpkredir.ini"))
            return;
    }

    if (!configIni.getBool("CPKREDIR", "Enabled", true))
        return;

    if (configIni.getBool("CPKREDIR", "EnableSaveFileRedirection", false))
    {
        std::string saveFilePathU8 = configIni.getString("CPKREDIR", "SaveFileFallback", "");
        if (!saveFilePathU8.empty())
            ModLoader::s_saveFilePath = std::u8string_view((const char8_t*)saveFilePathU8.c_str());
        else
            ModLoader::s_saveFilePath = userPath / "mlsave";

        ModLoader::s_saveFilePath /= "SYS-DATA";
    }

    if (configIni.getString("CPKREDIR", "LogType", std::string()) == "console")
    {
        os::process::ShowConsole();
        s_isLogTypeConsole = true;
    }

    std::string modsDbIniFilePathU8 = configIni.getString("CPKREDIR", "ModsDbIni", "");
    if (modsDbIniFilePathU8.empty())
        return;

    IniFile modsDbIni;
    if (!modsDbIni.read(std::u8string_view((const char8_t*)modsDbIniFilePathU8.c_str())))
        return;

    bool foundModSaveFilePath = false;

    size_t activeModCount = modsDbIni.get<size_t>("Main", "ActiveModCount", 0);
    for (size_t i = 0; i < activeModCount; ++i)
    {
        std::string modId = modsDbIni.getString("Main", fmt::format("ActiveMod{}", i), "");
        if (modId.empty())
            continue;

        std::string modIniFilePathU8 = modsDbIni.getString("Mods", modId, "");
        if (modIniFilePathU8.empty())
            continue;

        std::filesystem::path modIniFilePath(std::u8string_view((const char8_t*)modIniFilePathU8.c_str()));

        IniFile modIni;
        if (!modIni.read(modIniFilePath))
            continue;

        Mod mod;
        std::filesystem::path modSaveFilePath;
        if (!ParseModConfiguration(modIniFilePath, modIni, mod, modSaveFilePath, false))
            continue;

        if (!foundModSaveFilePath && !modSaveFilePath.empty())
        {
            ModLoader::s_saveFilePath = std::move(modSaveFilePath);
            foundModSaveFilePath = true;
        }

        if (!mod.includeDirs.empty())
            g_mods.emplace_back(std::move(mod));
    }

    // The folder scan counts mods managed under the per-user mods directory.
    // Also include valid legacy/external CPKREDIR entries that the regular
    // loader actually accepted, so the startup status does not say zero while
    // those mods are active in-game.
    const uint32_t scannedModCount = ModLoader::s_detectedModCount.load(std::memory_order_relaxed);
    const uint32_t loadedModCount = static_cast<uint32_t>(g_mods.size());
    if (loadedModCount > scannedModCount)
        ModLoader::s_detectedModCount.store(loadedModCount, std::memory_order_release);

    auto codeCount = modsDbIni.get<size_t>("Codes", "CodeCount", 0);

    if (codeCount)
    {
        std::vector<std::string> codes{};

        for (size_t i = 0; i < codeCount; i++)
        {
            auto name = modsDbIni.getString("Codes", fmt::format("Code{}", i), "");

            if (name.empty())
                continue;

            codes.push_back(name);
        }

        for (auto& def : g_configDefinitions)
        {
            if (!def->IsHidden() || def->GetSection() != "Codes")
                continue;

            /* NOTE: this is inefficient, but it happens
               once on boot for a handful of codes at release
               and is temporary until we support real code mods. */
            for (size_t i = 0; i < codes.size(); i++)
            {
                if (def->GetName() == codes[i])
                {
                    LOGF_IMPL(Utility, "Mod Loader", "Loading code: \"{}\"", codes[i]);
                    *(bool*)def->GetValue() = true;
                    break;
                }
            }
        }
    }
}

void ModLoader::NotifyStartupLoadingStarted()
{
    if (s_showStartupStatus.load(std::memory_order_acquire))
        s_startupLoadingObserved.store(true, std::memory_order_relaxed);
}

void ModLoader::NotifyStartupLoadingFinished()
{
    if (s_startupLoadingObserved.exchange(false, std::memory_order_relaxed))
        s_showStartupStatus.store(false, std::memory_order_release);
}

static constexpr uint32_t LZX_SIGNATURE = 0xFF512EE;

static std::span<uint8_t> decompressLzx(PPCContext& ctx, uint8_t* base, const uint8_t* compressedData, size_t compressedDataSize, be<uint32_t>* scratchSpace)
{
    assert(g_memory.IsInMemoryRange(compressedData));

    bool shouldFreeScratchSpace = false;
    if (scratchSpace == nullptr)
    {
        scratchSpace = reinterpret_cast<be<uint32_t>*>(g_userHeap.Alloc(sizeof(uint32_t) * 2));
        shouldFreeScratchSpace = true;
    }

    // Initialize decompressor
    ctx.r3.u32 = 1;
    ctx.r4.u32 = uint32_t((compressedData + 0xC) - base);
    ctx.r5.u32 = *reinterpret_cast<const be<uint32_t>*>(compressedData + 0x8);
    ctx.r6.u32 = uint32_t(reinterpret_cast<uint8_t*>(scratchSpace) - base);
    sub_831CE1A0(ctx, base);

    uint64_t decompressedDataSize = *reinterpret_cast<const be<uint64_t>*>(compressedData + 0x18);
    uint8_t* decompressedData = reinterpret_cast<uint8_t*>(g_userHeap.Alloc(decompressedDataSize));

    uint32_t blockSize = *reinterpret_cast<const be<uint32_t>*>(compressedData + 0x28);
    size_t decompressedDataOffset = 0;
    size_t compressedDataOffset = 0x30;

    while (decompressedDataOffset < decompressedDataSize)
    {
        size_t decompressedBlockSize = decompressedDataSize - decompressedDataOffset;

        if (decompressedBlockSize > blockSize)
            decompressedBlockSize = blockSize;

        *(scratchSpace + 1) = decompressedBlockSize;

        uint32_t compressedBlockSize = *reinterpret_cast<const be<uint32_t>*>(compressedData + compressedDataOffset);

        // Decompress
        ctx.r3.u32 = *scratchSpace;
        ctx.r4.u32 = uint32_t((decompressedData + decompressedDataOffset) - base);
        ctx.r5.u32 = uint32_t(reinterpret_cast<uint8_t*>(scratchSpace + 1) - base);
        ctx.r6.u32 = uint32_t((compressedData + compressedDataOffset + 0x4) - base);
        ctx.r7.u32 = compressedBlockSize;
        sub_831CE0D0(ctx, base);

        decompressedDataOffset += *(scratchSpace + 1);
        compressedDataOffset += 0x4 + compressedBlockSize;
    }

    // Deinitialize decompressor
    ctx.r3.u32 = *scratchSpace;
    sub_831CE150(ctx, base);

    if (shouldFreeScratchSpace)
        g_userHeap.Free(scratchSpace);

    return { decompressedData, decompressedDataSize };
}

// Hedgehog::Database::CDatabaseLoader::ReadArchiveList
PPC_FUNC_IMPL(__imp__sub_82E0D3E8);
PPC_FUNC(sub_82E0D3E8)
{
    if (g_mods.empty())
    {
        __imp__sub_82E0D3E8(ctx, base);
        return;
    }

    thread_local ankerl::unordered_dense::set<std::string> s_fileNames;
    s_fileNames.clear();

    auto parseArlFileData = [&](const uint8_t* arlFileData, size_t arlFileSize)
        {
            struct ArlHeader
            {
                uint32_t signature;
                uint32_t splitCount;
            };

            auto* arlHeader = reinterpret_cast<const ArlHeader*>(arlFileData);
            size_t arlHeaderSize = sizeof(ArlHeader) + arlHeader->splitCount * sizeof(uint32_t);
            const uint8_t* arlFileNames = arlFileData + arlHeaderSize;

            while (arlFileNames < arlFileData + arlFileSize)
            {
                uint8_t fileNameSize = *arlFileNames;
                ++arlFileNames;

                s_fileNames.emplace(reinterpret_cast<const char*>(arlFileNames), fileNameSize);

                arlFileNames += fileNameSize;
            }

            return arlHeaderSize;
        };

    auto parseArFileData = [&](const uint8_t* arFileData, size_t arFileSize)
        {
            struct ArEntry
            {
                uint32_t entrySize;
                uint32_t dataSize;
                uint32_t dataOffset;
                uint32_t fileDateLow;
                uint32_t fileDateHigh;
            };

            for (size_t i = 16; i < arFileSize; )
            {
                auto entry = reinterpret_cast<const ArEntry*>(arFileData + i);
                s_fileNames.emplace(reinterpret_cast<const char*>(entry + 1));
                i += entry->entrySize;
            }
        };

    auto r3 = ctx.r3;
    auto r4 = ctx.r4;
    auto r5 = ctx.r5;
    auto r6 = ctx.r6;

    auto loadFile = [&]<typename TFunction>(const std::filesystem::path& filePath, const TFunction& function)
    {
        std::ifstream stream(filePath, std::ios::binary);
        if (stream.good())
        {
            if (ModLoader::s_isLogTypeConsole)
                LOGF_IMPL(Utility, "Mod Loader", "Loading file: \"{}\"", reinterpret_cast<const char*>(filePath.u8string().c_str()));

            be<uint32_t> signature{};
            stream.read(reinterpret_cast<char*>(&signature), sizeof(signature));

            stream.seekg(0, std::ios::end);
            size_t arlFileSize = stream.tellg();
            stream.seekg(0, std::ios::beg);

            if (signature == LZX_SIGNATURE)
            {
                void* compressedFileData = g_userHeap.Alloc(arlFileSize);
                stream.read(reinterpret_cast<char*>(compressedFileData), arlFileSize);
                stream.close();

                auto fileData = decompressLzx(ctx, base, reinterpret_cast<uint8_t*>(compressedFileData), arlFileSize, nullptr);

                g_userHeap.Free(compressedFileData);

                function(fileData.data(), fileData.size());

                g_userHeap.Free(fileData.data());
            }
            else
            {
                thread_local std::vector<uint8_t> s_fileData;

                s_fileData.resize(arlFileSize);
                stream.read(reinterpret_cast<char*>(s_fileData.data()), arlFileSize);
                stream.close();

                function(s_fileData.data(), arlFileSize);
            }

            return true;
        }

        return false;
    };

    thread_local xxHashMap<std::vector<std::pair<std::filesystem::path, bool>>> s_cache;

    std::u8string_view arlFilePathU8(reinterpret_cast<const char8_t*>(base + PPC_LOAD_U32(ctx.r4.u32)));
    XXH64_hash_t hash = XXH3_64bits(arlFilePathU8.data(), arlFilePathU8.size());
    auto findResult = s_cache.find(hash);

    if (findResult != s_cache.end())
    {
        for (const auto& [arlFilePath, isArchiveList] : findResult->second)
        {
            if (isArchiveList)
                loadFile(arlFilePath, parseArlFileData);
            else
                loadFile(arlFilePath, parseArFileData);
        }
    }
    else
    {
        std::vector<std::pair<std::filesystem::path, bool>> arlFilePaths;
        std::filesystem::path arlFilePath;
        std::filesystem::path arFilePath;
        std::filesystem::path appendArlFilePath;

        for (auto& mod : g_mods)
        {
            for (auto& includeDir : mod.includeDirs)
            {
                auto loadUncachedFile = [&](const std::filesystem::path& filePath, bool isArchiveList)
                {
                    if (mod.type == ModType::UMM && mod.readOnly.contains(filePath))
                        return false;

                    std::filesystem::path combinedFilePath = includeDir / filePath;
                    if (!IsSafeManagedModPath(mod, combinedFilePath))
                        return false;

                    bool success;
                    if (isArchiveList)
                        success = loadFile(combinedFilePath, parseArlFileData);
                    else
                        success = loadFile(combinedFilePath, parseArFileData);

                    if (success)
                        arlFilePaths.emplace_back(std::move(combinedFilePath), isArchiveList);

                    return success;
                };

                if (mod.type == ModType::UMM)
                {
                    if (mod.merge)
                    {
                        if (arlFilePath.empty())
                        {
                            arlFilePath = arlFilePathU8;
                            arlFilePath += ".arl";
                        }

                        if (!loadUncachedFile(arlFilePath, true))
                        {
                            if (arFilePath.empty())
                            {
                                arFilePath = arlFilePathU8;
                                arFilePath += ".ar";
                            }

                            if (!loadUncachedFile(arFilePath, false))
                            {
                                thread_local std::filesystem::path s_tempPath;

                                for (uint32_t i = 0; ; i++)
                                {
                                    s_tempPath = arFilePath;
                                    s_tempPath += fmt::format(".{:02}", i);

                                    if (!loadUncachedFile(s_tempPath, false))
                                        break;
                                }
                            }
                        }
                    }
                }
                else if (mod.type == ModType::HMM)
                {
                    if (appendArlFilePath.empty())
                    {
                        if (arlFilePath.empty())
                        {
                            arlFilePath = arlFilePathU8;
                            arlFilePath += ".arl";
                        }

                        appendArlFilePath = arlFilePath.parent_path();
                        appendArlFilePath /= "+";
                        appendArlFilePath += arlFilePath.filename();
                    }

                    loadUncachedFile(appendArlFilePath, true);
                }
            }
        }

        s_cache.emplace(hash, std::move(arlFilePaths));
    }

    ctx.r3 = r3;
    ctx.r4 = r4;
    ctx.r5 = r5;
    ctx.r6 = r6;

    if (s_fileNames.empty())
    {
        __imp__sub_82E0D3E8(ctx, base);
        return;
    }

    size_t arlHeaderSize = parseArlFileData(base + ctx.r5.u32, ctx.r6.u32);
    size_t arlFileSize = arlHeaderSize;

    for (auto& fileName : s_fileNames)
    {
        arlFileSize += 1;
        arlFileSize += fileName.size();
    }

    uint8_t* newArlFileData = reinterpret_cast<uint8_t*>(g_userHeap.Alloc(arlFileSize));
    memcpy(newArlFileData, base + ctx.r5.u32, arlHeaderSize);

    uint8_t* arlFileNames = newArlFileData + arlHeaderSize;
    for (auto& fileName : s_fileNames)
    {
        *arlFileNames = uint8_t(fileName.size());
        ++arlFileNames;
        memcpy(arlFileNames, fileName.data(), fileName.size());
        arlFileNames += fileName.size();
    }

    ctx.r5.u32 = uint32_t(newArlFileData - base);
    ctx.r6.u32 = uint32_t(arlFileSize);

    __imp__sub_82E0D3E8(ctx, base);

    g_userHeap.Free(newArlFileData);
}

// Load elements have an unused "pretty name" field. We will use this field to store the archive file path,
// prefixed with a magic string. When the first load detects this string, it will load append archives
// and then clear the field to prevent remaining splits from loading the append archives again.
// We cannot rely on .ar.00 being the first split to be loaded, so this approach is necessary.
static thread_local uint32_t g_prefixedArFilePath = NULL;

// Hedgehog::Database::CDatabaseLoader::LoadArchives
PPC_FUNC_IMPL(__imp__sub_82E0CC38);
PPC_FUNC(sub_82E0CC38)
{
    if (g_mods.empty())
    {
        __imp__sub_82E0CC38(ctx, base);
        return;
    }

    auto r3 = ctx.r3;
    auto r4 = ctx.r4;
    auto r5 = ctx.r5;
    auto r6 = ctx.r6;
    auto r7 = ctx.r7;
    auto r8 = ctx.r8;

    const char* arFilePath = reinterpret_cast<const char*>(base + PPC_LOAD_U32(r5.u32));

    // __HH_ALLOC
    ctx.r3.u32 = 22 + strlen(arFilePath);
    sub_822C0988(ctx, base);
    char* prefixedArFilePath = reinterpret_cast<char*>(base + ctx.r3.u32);

    *reinterpret_cast<be<uint32_t>*>(prefixedArFilePath) = 1;
    strcpy(prefixedArFilePath + 0x4, "/UnleashedRecomp/");
    strcpy(prefixedArFilePath + 0x15, arFilePath);

    ctx.r1.u32 -= 0x10;
    uint32_t stackSpace = ctx.r1.u32;
    PPC_STORE_U32(stackSpace, static_cast<uint32_t>(reinterpret_cast<uint8_t*>(prefixedArFilePath) - base) + 0x4);
    g_prefixedArFilePath = stackSpace;

    ctx.r3 = r3;
    ctx.r4 = r4;
    ctx.r5 = r5;
    ctx.r6 = r6;
    ctx.r7 = r7;
    ctx.r8 = r8;
    __imp__sub_82E0CC38(ctx, base);

    // Hedgehog::Base::CSharedString::~CSharedString
    ctx.r3.u32 = stackSpace;
    sub_82DFB148(ctx, base);

    g_prefixedArFilePath = NULL;
    ctx.r1.u32 += 0x10;
}

// Hedgehog::Database::SLoadElement::SLoadElement
PPC_FUNC_IMPL(__imp__sub_82E140D8);
PPC_FUNC(sub_82E140D8)
{
    // Store the prefixed archive file path as the pretty name. It's unused for archives we want to append to.
    if (!g_mods.empty() && PPC_LOAD_U32(ctx.r5.u32) == 0x8200A621 && g_prefixedArFilePath != NULL)
        ctx.r5.u32 = g_prefixedArFilePath;

    __imp__sub_82E140D8(ctx, base);
}

// Hedgehog::Database::CDatabaseLoader::CCreateFromArchive::CreateCallback
PPC_FUNC_IMPL(__imp__sub_82E0B500);
PPC_FUNC(sub_82E0B500)
{
    if (g_mods.empty())
    {
        __imp__sub_82E0B500(ctx, base);
        return;
    }

    uint32_t prefixedArFilePath = PPC_LOAD_U32(ctx.r5.u32);
    std::u8string_view arFilePathU8(reinterpret_cast<const char8_t*>(base + prefixedArFilePath));
    if (!arFilePathU8.starts_with(u8"/UnleashedRecomp/"))
    {
        __imp__sub_82E0B500(ctx, base);
        return;
    }

    // Immediately clear the string, so the remaining splits don't load append archives again.
    PPC_STORE_U8(prefixedArFilePath, 0x00);
    arFilePathU8.remove_prefix(0x11);

    auto r3 = ctx.r3; // Callback
    auto r4 = ctx.r4; // Database
    auto r5 = ctx.r5; // Name
    auto r6 = ctx.r6; // Data
    auto r7 = ctx.r7; // Size
    auto r8 = ctx.r8; // Callback data

    auto loadArchive = [&](const std::filesystem::path& arFilePath)
        {
            std::ifstream stream(arFilePath, std::ios::binary);
            if (stream.good())
            {
                if (ModLoader::s_isLogTypeConsole)
                    LOGF_IMPL(Utility, "Mod Loader", "Loading file: \"{}\"", reinterpret_cast<const char*>(arFilePath.u8string().c_str()));

                stream.seekg(0, std::ios::end);
                size_t arFileSize = stream.tellg();

                void* arFileData = g_userHeap.Alloc(arFileSize);
                stream.seekg(0, std::ios::beg);
                stream.read(reinterpret_cast<char*>(arFileData), arFileSize);
                stream.close();

                auto arFileDataHolder = reinterpret_cast<be<uint32_t>*>(g_userHeap.Alloc(sizeof(uint32_t) * 2));

                if (*reinterpret_cast<be<uint32_t>*>(arFileData) == LZX_SIGNATURE)
                {
                    auto fileData = decompressLzx(ctx, base, reinterpret_cast<uint8_t*>(arFileData), arFileSize, arFileDataHolder);

                    g_userHeap.Free(arFileData);

                    arFileData = fileData.data();
                    arFileSize = fileData.size();
                }

                arFileDataHolder[0] = g_memory.MapVirtual(arFileData);
                arFileDataHolder[1] = NULL;

                ctx.r3 = r3;
                ctx.r4 = r4;
                ctx.r5 = r5;
                ctx.r6.u32 = g_memory.MapVirtual(arFileDataHolder);
                ctx.r7.u32 = uint32_t(arFileSize);
                ctx.r8 = r8;

                __imp__sub_82E0B500(ctx, base);

                g_userHeap.Free(arFileDataHolder);
                g_userHeap.Free(arFileData);

                return true;
            }

            return false;
        };

    thread_local xxHashMap<std::vector<std::filesystem::path>> s_cache;

    XXH64_hash_t hash = XXH3_64bits(arFilePathU8.data(), arFilePathU8.size());
    auto findResult = s_cache.find(hash);
    if (findResult != s_cache.end())
    {
        for (const auto& arFilePath : findResult->second)
            loadArchive(arFilePath);
    }
    else
    {
        std::vector<std::filesystem::path> arFilePaths;
        std::filesystem::path arFilePath;
        std::filesystem::path appendArFilePath;

        for (auto& mod : g_mods)
        {
            for (auto& includeDir : mod.includeDirs)
            {
                auto loadUncachedArchive = [&](const std::filesystem::path& arFilePath)
                    {
                        if (mod.type == ModType::UMM && mod.readOnly.contains(arFilePath))
                            return false;

                        std::filesystem::path combinedFilePath = includeDir / arFilePath;
                        if (!IsSafeManagedModPath(mod, combinedFilePath))
                            return false;

                        bool success = loadArchive(combinedFilePath);
                        if (success)
                            arFilePaths.emplace_back(std::move(combinedFilePath));

                        return success;
                    };

                auto loadArchives = [&](const std::filesystem::path& arFilePath)
                    {
                        thread_local std::filesystem::path s_tempPath;
                        s_tempPath = arFilePath;
                        s_tempPath += "l";

                        if (mod.type == ModType::UMM && mod.readOnly.contains(s_tempPath))
                            return;

                        const std::filesystem::path archiveListPath = includeDir / s_tempPath;
                        if (!IsSafeManagedModPath(mod, archiveListPath))
                            return;

                        std::ifstream stream(archiveListPath, std::ios::binary);
                        if (stream.good())
                        {
                            be<uint32_t> signature{};
                            uint32_t splitCount{};
                            stream.read(reinterpret_cast<char*>(&signature), sizeof(signature));

                            if (signature == LZX_SIGNATURE)
                            {
                                stream.seekg(0, std::ios::end);
                                size_t arlFileSize = stream.tellg();
                                stream.seekg(0, std::ios::beg);

                                void* compressedFileData = g_userHeap.Alloc(arlFileSize);
                                stream.read(reinterpret_cast<char*>(compressedFileData), arlFileSize);
                                stream.close();

                                auto fileData = decompressLzx(ctx, base, reinterpret_cast<uint8_t*>(compressedFileData), arlFileSize, nullptr);

                                g_userHeap.Free(compressedFileData);

                                splitCount = *reinterpret_cast<uint32_t*>(fileData.data() + 0x4);

                                g_userHeap.Free(fileData.data());
                            }
                            else
                            {
                                stream.read(reinterpret_cast<char*>(&splitCount), sizeof(splitCount));
                                stream.close();
                            }

                            if (splitCount == 0)
                            {
                                loadUncachedArchive(arFilePath);
                            }
                            else
                            {
                                for (uint32_t i = 0; i < splitCount; i++)
                                {
                                    s_tempPath = arFilePath;
                                    s_tempPath += fmt::format(".{:02}", i);
                                    loadUncachedArchive(s_tempPath);
                                }
                            }
                        }
                        else if (mod.type == ModType::UMM)
                        {
                            if (!loadUncachedArchive(arFilePath))
                            {
                                for (uint32_t i = 0; ; i++)
                                {
                                    s_tempPath = arFilePath;
                                    s_tempPath += fmt::format(".{:02}", i);
                                    if (!loadUncachedArchive(s_tempPath))
                                        break;
                                }
                            }
                        }
                    };

                if (mod.type == ModType::UMM)
                {
                    if (mod.merge)
                    {
                        if (arFilePath.empty())
                            arFilePath = arFilePathU8;

                        loadArchives(arFilePath);
                    }
                }
                else if (mod.type == ModType::HMM)
                {
                    if (appendArFilePath.empty())
                    {
                        if (arFilePath.empty())
                            arFilePath = arFilePathU8;

                        appendArFilePath = arFilePath.parent_path();
                        appendArFilePath /= "+";
                        appendArFilePath += arFilePath.filename();
                    }

                    loadArchives(appendArFilePath);
                }
            }
        }

        s_cache.emplace(hash, std::move(arFilePaths));
    }

    ctx.r3 = r3;
    ctx.r4 = r4;
    ctx.r5 = r5;
    ctx.r6 = r6;
    ctx.r7 = r7;
    ctx.r8 = r8;

    __imp__sub_82E0B500(ctx, base);
}

// CriAuObjLoc::AttachCueSheet
PPC_FUNC_IMPL(__imp__sub_8314A310);
PPC_FUNC(sub_8314A310)
{
    // allocator: 0x4
    // capacity: 0x24
    // count: 0x28
    // data: 0x2C
    uint32_t capacity = PPC_LOAD_U32(ctx.r3.u32 + 0x24);
    if (capacity == PPC_LOAD_U32(ctx.r3.u32 + 0x28))
    {
        auto r3 = ctx.r3;
        auto r4 = ctx.r4;
        auto r5 = ctx.r5;

        // Allocate
        ctx.r3.u32 = PPC_LOAD_U32(r3.u32 + 0x4);
        ctx.r4.u32 = (capacity * 2) * sizeof(uint32_t);
        ctx.r5.u32 = 0x82195248; // AuObjCueSheet
        ctx.r6.u32 = 0x4;
        sub_83167FD8(ctx, base);

        // Copy
        uint32_t oldData = PPC_LOAD_U32(r3.u32 + 0x2C);
        uint32_t newData = ctx.r3.u32;

        memcpy(base + newData, base + oldData, capacity * sizeof(uint32_t));
        memset(base + newData + (capacity * sizeof(uint32_t)), 0, capacity * sizeof(uint32_t));

        PPC_STORE_U32(r3.u32 + 0x24, capacity * 2);
        PPC_STORE_U32(r3.u32 + 0x2C, newData);

        // Deallocate
        ctx.r3.u32 = PPC_LOAD_U32(r3.u32 + 0x4);
        ctx.r4.u32 = oldData;
        sub_83168100(ctx, base);

        ctx.r3 = r3;
        ctx.r4 = r4;
        ctx.r5 = r5;
    }

    __imp__sub_8314A310(ctx, base);
}
