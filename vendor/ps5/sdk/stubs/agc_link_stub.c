/*
 * PS5 native diagnostic harness - AGC link stub.
 * Copyright (C) 2026 Mihawk-99
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Host-link declarations for system libSceAgc; never packaged or executed.
 */

#include <stdint.h>

int32_t sceAgcInit(void *state, uint32_t size) { (void)state; (void)size; return -1; }
void *sceAgcGetRegisterDefaults(void) { return 0; }
uint32_t *sceAgcDcbSetCxRegistersIndirect(void *command, const void *registers, uint32_t count) { (void)command; (void)registers; (void)count; return 0; }
uint32_t *sceAgcDcbDrawIndexAuto(void *command, uint32_t count, uint64_t modifier) { (void)command; (void)count; (void)modifier; return 0; }
uint32_t *sceAgcDcbSetNumInstances(void *command, uint32_t count) { (void)command; (void)count; return 0; }
