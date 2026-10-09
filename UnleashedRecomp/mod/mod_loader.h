#pragma once

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

struct ModConfigChoice
{
    std::string displayName;
    std::string value;
    std::string description;
};

struct ModConfigOption
{
    std::string sectionName;
    std::string groupDisplayName;
    std::string name;
    std::string displayName;
    std::string description;
    std::string type;
    std::string iniFile;
    std::string defaultValue;
    std::string value;
    std::vector<ModConfigChoice> choices;
};

struct UserModInfo
{
    std::string folderName;
    std::string title;
    std::string description;
    std::string author;
    std::vector<ModConfigOption> options;
};

struct ModLoader
{
    static inline bool s_isLogTypeConsole;

    static inline std::filesystem::path s_saveFilePath;
    static inline std::atomic<uint32_t> s_detectedModCount{};
    static inline std::atomic<uint32_t> s_enabledModCount{};
    static inline std::atomic<bool> s_showStartupStatus{};
    static inline std::atomic<bool> s_startupLoadingObserved{};

    static std::filesystem::path ResolvePath(std::string_view path);

    static std::vector<std::filesystem::path>* GetIncludeDirectories(size_t modIndex);

    static const std::vector<UserModInfo>& GetUserMods();
    static bool CommitUserMods(const std::vector<UserModInfo>& mods, std::string& errorMessage);

    static void Init();
    static void NotifyStartupLoadingStarted();
    static void NotifyStartupLoadingFinished();
};
