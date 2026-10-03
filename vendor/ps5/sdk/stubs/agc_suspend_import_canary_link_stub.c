/*
 * PS5 isolated linked-AGC import canary.
 * Copyright (C) 2026 Mihawk-99
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Host-link only: this body exists so the probe can be linked, not run.
 */
#include <stdint.h>

int32_t sceAgcSuspendPoint(void)
{
    return -1;
}

/* Phase C2: the instance count a draw runs, which libSceAgc provides; the
 * console's instancing probe measures the packet it writes. */
uint32_t *sceAgcDcbSetNumInstances(void *command, uint32_t count)
{
    (void)command;
    (void)count;
    return 0;
}
