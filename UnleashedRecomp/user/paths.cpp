#include "paths.h"
#include <os/process.h>
#include <fstream>
#if defined(__PROSPERO__)
#include <cstdio>
#include <sys/stat.h>
#include <unistd.h>
#endif

std::filesystem::path g_executableRoot = os::process::GetExecutableRoot();
std::filesystem::path g_userPath = BuildUserPath();

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
    static const char* const s_writableRoots[] = {
        "/app0/user",
        "/data/UnleashedRecomp",
        "/temp0/UnleashedRecomp",
        "/tmp/UnleashedRecomp",
        "/download0/PPSA99902/user",
    };
    for (const char* candidateStr : s_writableRoots)
    {
        mkdir(candidateStr, 0777);
        std::string probe = std::string(candidateStr) + "/.write_test";
        if (FILE* fp = std::fopen(probe.c_str(), "wb"))
        {
            std::fclose(fp);
            unlink(probe.c_str());
            userPath = std::filesystem::path(candidateStr);
            break;
        }
    }
    if (userPath.empty())
        userPath = std::filesystem::path("/app0");
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

const std::filesystem::path& GetUserPath()
{
    return g_userPath;
}
