/*
 * PS5 isolated linked-driver import canary.
 * Copyright (C) 2026 Mihawk-99
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Host-link only: these bodies exist so the probe can be linked, not run.
 */
#include <stdint.h>

int32_t sceAgcDriverSubmitDcb(void *description)
{
    (void)description;
    return -1;
}

int32_t sceAgcSuspendPoint(void)
{
    return -1;
}
