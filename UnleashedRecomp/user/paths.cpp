// This file gets included in both config.h and config.cpp, with their own macros changing
// the preprocessed output. The header is only going to have the declarations this way.

#include "paths.h"
#include <os/process.h>
#include <fstream>

#if defined(__PROSPERO__)
#include <ps5/elevation.hpp>
#include <array>
#include <cerrno>
#include <cstdio>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

std::filesystem::path g_executableRoot = os::process::GetExecutableRoot();
std::filesystem::path g_userPath = BuildUserPath();

#if defined(__PROSPERO__)
namespace
{
bool EnsureWritableDirectory(const std::filesystem::path& directory)
{
    if (directory.empty())
        return false;

    const std::string directoryString = directory.string();
    if (mkdir(directoryString.c_str(), 0777) != 0 && errno != EEXIST)
        return false;

    struct stat directoryStat{};
    if (stat(directoryString.c_str(), &directoryStat) != 0 || !S_ISDIR(directoryStat.st_mode))
        return false;

    const std::string prefix = directoryString + "/.unleashed-write-probe-" + std::to_string(getpid());
    for (unsigned attempt = 0; attempt < 8; ++attempt)
    {
        const std::string probePath = prefix + "-" + std::to_string(attempt);
        const int descriptor = open(probePath.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0600);
        if (descriptor < 0)
        {
            if (errno == EEXIST)
                continue;
            return false;
        }

        constexpr char probeData = 'U';
        const bool wroteData = write(descriptor, &probeData, sizeof(probeData)) == sizeof(probeData);
        const bool closed = close(descriptor) == 0;
        const bool removed = unlink(probePath.c_str()) == 0;
        return wroteData && closed && removed;
    }

    return false;
}

bool CopyFileIfMissing(const std::filesystem::path& source, const std::filesystem::path& destination,
                       bool& hadError, unsigned& copied)
{
    std::error_code ec;
    const auto sourceStatus = std::filesystem::symlink_status(source, ec);
    if (ec == std::errc::no_such_file_or_directory)
        return true;
    if (ec || !std::filesystem::is_regular_file(sourceStatus))
    {
        hadError = hadError || static_cast<bool>(ec);
        return !ec;
    }

    const bool destinationExists = std::filesystem::exists(destination, ec);
    if (ec)
    {
        hadError = true;
        return false;
    }
    if (destinationExists)
        return true;

    std::filesystem::create_directories(destination.parent_path(), ec);
    if (!ec)
    {
        const bool didCopy = std::filesystem::copy_file(
            source, destination, std::filesystem::copy_options::skip_existing, ec);
        if (!ec && didCopy)
            ++copied;
    }
    if (ec)
        hadError = true;
    return !ec;
}

bool CopyDirectoryFilesIfMissing(const std::filesystem::path& source, const std::filesystem::path& destination,
                                 bool& hadError, unsigned& copied)
{
    std::error_code ec;
    const auto sourceStatus = std::filesystem::symlink_status(source, ec);
    if (ec == std::errc::no_such_file_or_directory)
        return true;
    if (ec || !std::filesystem::is_directory(sourceStatus))
    {
        hadError = hadError || static_cast<bool>(ec);
        return !ec;
    }

    std::filesystem::recursive_directory_iterator it(
        source, std::filesystem::directory_options::skip_permission_denied, ec);
    const std::filesystem::recursive_directory_iterator end;
    if (ec)
    {
        hadError = true;
        return false;
    }

    for (; it != end; it.increment(ec))
    {
        if (ec)
        {
            hadError = true;
            ec.clear();
            continue;
        }

        const auto status = it->symlink_status(ec);
        if (ec)
        {
            hadError = true;
            ec.clear();
            continue;
        }
        // Only copy normal directories and files. Do not follow or migrate symlinks.
        if (!std::filesystem::is_directory(status) && !std::filesystem::is_regular_file(status))
            continue;

        const auto relativePath = it->path().lexically_relative(source);
        const auto targetPath = destination / relativePath;
        if (std::filesystem::is_directory(status))
        {
            std::filesystem::create_directories(targetPath, ec);
            if (ec)
            {
                hadError = true;
                ec.clear();
            }
            continue;
        }

        const bool targetExists = std::filesystem::exists(targetPath, ec);
        if (ec)
        {
            hadError = true;
            ec.clear();
            continue;
        }
        if (targetExists)
            continue;

        std::filesystem::create_directories(targetPath.parent_path(), ec);
        if (!ec)
        {
            const bool didCopy = std::filesystem::copy_file(
                it->path(), targetPath, std::filesystem::copy_options::skip_existing, ec);
            if (!ec && didCopy)
                ++copied;
        }
        if (ec)
        {
            hadError = true;
            ec.clear();
        }
    }

    if (ec)
        hadError = true;
    return !hadError;
}

unsigned MigrateLegacyUserData(const std::filesystem::path& destination)
{
    const auto markerPath = destination / ".legacy-user-data-migrated-v1";
    std::error_code ec;
    if (std::filesystem::exists(markerPath, ec))
        return 0;

    // These are the writable roots used by earlier PS5 builds. Copy only user
    // settings and save data; do not recursively copy the executable/resources.
    static const std::array<std::filesystem::path, 5> legacyRoots = {
        "/app0/user",
        "/download0/PPSA99902/user",
        "/app0",
        "/temp0/UnleashedRecomp",
        "/tmp/UnleashedRecomp",
    };
    static const std::array<const char*, 3> legacyFiles = {
        "config.toml",
        "cpkredir.ini",
        ".ach_notif_restored",
    };
    static const std::array<const char*, 2> legacyDirectories = {
        "save",
        "mlsave",
    };

    bool hadError = false;
    unsigned copied = 0;
    for (const auto& root : legacyRoots)
    {
        if (root == destination)
            continue;

        for (const char* fileName : legacyFiles)
        {
            const auto target = destination / fileName;
            const bool existed = std::filesystem::exists(target, ec);
            if (ec)
            {
                hadError = true;
                ec.clear();
                continue;
            }
            if (!existed)
                CopyFileIfMissing(root / fileName, target, hadError, copied);
        }

        for (const char* directoryName : legacyDirectories)
        {
            const auto source = root / directoryName;
            const auto target = destination / directoryName;
            std::error_code existsEc;
            const bool sourceExists = std::filesystem::exists(source, existsEc);
            if (existsEc)
            {
                hadError = true;
                continue;
            }
            if (!sourceExists)
                continue;

            CopyDirectoryFilesIfMissing(source, target, hadError, copied);
        }
    }

    if (!hadError)
    {
        const int marker = open(markerPath.string().c_str(), O_WRONLY | O_CREAT | O_EXCL, 0600);
        if (marker >= 0)
        {
            constexpr char markerContents[] = "Copied legacy config/save data without overwriting existing files.\n";
            (void)write(marker, markerContents, sizeof(markerContents) - 1);
            (void)close(marker);
        }
    }

    return copied;
}
} // namespace
#endif

bool CheckPortable()
{
#if defined(__PROSPERO__)
    return access("/app0/portable.txt", F_OK) == 0;
#else
    std::error_code ec;
    return std::filesystem::exists(g_executableRoot / "portable.txt", ec);
#endif
}

std::filesystem::path BuildUserPath()
{
    if (CheckPortable())
        return g_executableRoot;

    std::filesystem::path userPath;

#if defined(_WIN32)
    PWSTR knownPath = NULL;
    if (SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, NULL, &knownPath) == S_OK)
        userPath = std::filesystem::path{ knownPath } / USER_DIRECTORY;

    CoTaskMemFree(knownPath);
#elif defined(__PROSPERO__)
    const std::filesystem::path dataPath = std::filesystem::path("/data") / USER_DIRECTORY;
    userPath = EnsureWritableDirectory(dataPath) ? dataPath : g_executableRoot;
#elif defined(__linux__) || defined(__APPLE__)
    const char* homeDir = getenv("HOME");
#if defined(__linux__)
    if (homeDir == nullptr)
    {
        homeDir = getpwuid(getuid())->pw_dir;
    }
#endif

    if (homeDir != nullptr)
    {
        // Prefer to store in the .config directory if it exists. Use the home directory otherwise.
        std::filesystem::path homePath = homeDir;
#if defined(__linux__)
        std::filesystem::path configPath = homePath / ".config";
#else
        std::filesystem::path configPath = homePath / "Library" / "Application Support";
#endif
        if (std::filesystem::exists(configPath))
            userPath = configPath / USER_DIRECTORY;
        else
            userPath = homePath / ("." USER_DIRECTORY);
    }
#else
    static_assert(false, "GetUserPath() not implemented for this platform.");
#endif

    return userPath;
}

void InitializeUserPath()
{
#if defined(__PROSPERO__)
    if (CheckPortable())
    {
        g_userPath = g_executableRoot;
        return;
    }

    const std::filesystem::path dataPath = std::filesystem::path("/data") / USER_DIRECTORY;
    if (!EnsureWritableDirectory(dataPath))
    {
        const elevation::Status status = elevation::request(elevation::Capability::filesystem);
        if (status == elevation::Status::ok && EnsureWritableDirectory(dataPath))
        {
            g_userPath = dataPath;
            const unsigned migrated = MigrateLegacyUserData(dataPath);
            std::fprintf(stderr, "[UnleashedRecomp] User data path: %s (Lapy: %s; migrated: %u)\n",
                         g_userPath.string().c_str(), elevation::path(), migrated);
            return;
        }

        g_userPath = g_executableRoot;
        std::fprintf(stderr, "[UnleashedRecomp] /data unavailable (Lapy status %u, path %s); using executable root %s\n",
                     static_cast<unsigned>(status), elevation::path(), g_userPath.string().c_str());
        return;
    }

    g_userPath = dataPath;
    const unsigned migrated = MigrateLegacyUserData(dataPath);
    std::fprintf(stderr, "[UnleashedRecomp] User data path: %s (migrated: %u)\n",
                 g_userPath.string().c_str(), migrated);
#else
    g_userPath = BuildUserPath();
#endif
}

const std::filesystem::path& GetUserPath()
{
    return g_userPath;
}
