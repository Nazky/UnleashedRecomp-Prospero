/*
 * ps5-homebrew-dev-protocol - Title-aware close controller.
 * Copyright (C) 2026 BlackBearReloaded
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Closes the running big app only when its title matches the requested ID.
 * Exits 0 when the title was terminated and 1 otherwise.
 *
 * The sceSystemServiceKillApp(app_id, -1, 0, 0) fallback follows
 * sys_launch_title() in websrv's src/ps5/sys.c, Copyright (C) 2024
 * John Törnblom, GPL-3.0-or-later (https://github.com/ps5-payload-dev/websrv).
 */

#include <stdint.h>

#ifndef BOOTSTRAP_TITLE_ID
#error BOOTSTRAP_TITLE_ID must be defined
#endif

int sceLncUtilGetAppIdOfRunningBigApp(void);
int sceLncUtilGetAppTitleId(uint32_t app_id, char *title_id);
int sceLncUtilKillApp(uint32_t app_id);
int sceSystemServiceGetAppId(const char *);
int sceSystemServiceKillApp(int, int, int, int);

static int same_title(const char *left, const char *right)
{
    unsigned i;
    for (i = 0; left[i] || right[i]; ++i)
        if (left[i] != right[i])
            return 0;
    return 1;
}

int main(void)
{
    char title_id[16] = {0};
    int app_id = sceLncUtilGetAppIdOfRunningBigApp();

    if (app_id > 0 && sceLncUtilGetAppTitleId((uint32_t)app_id, title_id) == 0 &&
        same_title(title_id, BOOTSTRAP_TITLE_ID))
        return sceLncUtilKillApp((uint32_t)app_id) == 0 ? 0 : 1;

    app_id = sceSystemServiceGetAppId(BOOTSTRAP_TITLE_ID);
    return app_id > 0 && sceSystemServiceKillApp(app_id, -1, 0, 0) == 0
        ? 0 : 1;
}
