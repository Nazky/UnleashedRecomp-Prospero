#pragma once

#include <mod/mod_loader.h>

#define USER_DIRECTORY "UnleashedRecomp"

#ifndef GAME_INSTALL_DIRECTORY
#define GAME_INSTALL_DIRECTORY "."
#endif

extern std::filesystem::path g_executableRoot;

bool CheckPortable();
std::filesystem::path BuildUserPath();
void InitializeUserPath();
const std::filesystem::path& GetUserPath();

inline std::filesystem::path GetGamePath()
{
#ifdef __APPLE__
    // On macOS, there is the expectation that the app may be installed to
    // /Applications/, and the bundle should not be modified. Thus we need
    // to install game files to the user directory instead of next to the app.
    return GetUserPath();
#elif defined(__PROSPERO__)
    static const std::filesystem::path s_candidates[] = {
        "/app0/ressources",
        "/app0/resources",
        "/app0/PPSA99902/ressources",
        "/app0/PPSA99902/resources",
        "/app0/PPSA99902",
        "/app0",
        "/data/UnleashedRecomp/ressources",
        "/data/UnleashedRecomp/resources",
        "/data/UnleashedRecomp",
        "/download0/PPSA99902/ressources",
        "/download0/PPSA99902/resources",
        "/download0/PPSA99902",
    };
    for (const auto& candidate : s_candidates)
    {
        std::error_code ec;
        if (std::filesystem::exists(candidate / "game", ec))
            return candidate;
    }
    return "/app0/ressources";
#else
    return GAME_INSTALL_DIRECTORY;
#endif
}

inline std::filesystem::path GetSavePath(bool checkForMods)
{
    if (checkForMods && !ModLoader::s_saveFilePath.empty())
        return ModLoader::s_saveFilePath.parent_path();
    else
        return GetUserPath() / "save";
}

// Returned file name may not necessarily be
// equal to SYS-DATA as mods can assign anything.
inline std::filesystem::path GetSaveFilePath(bool checkForMods)
{
    if (checkForMods && !ModLoader::s_saveFilePath.empty())
        return ModLoader::s_saveFilePath;
    else
        return GetSavePath(false) / "SYS-DATA";
}
