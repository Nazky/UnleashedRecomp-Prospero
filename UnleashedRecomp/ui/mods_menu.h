#pragma once

#include <mod/mod_loader.h>

class ModsMenu
{
public:
    static inline bool s_isVisible = false;
    static inline bool s_isRestartRequired = false;

    static void Init();
    static void Draw();
    static void Open();
    static void Close();
    static bool CommitRestartSettings();
    static void RevertRestartSettings();
};
