/* Minimal no-op ps5log/1 compatibility header for the optional Lapy helper.
 * The upstream helper's logging is not required for its protocol; no log data
 * is emitted or persisted by this stub.
 */
#ifndef UNLEASHED_LAPY_PS5LOG_STUB_H
#define UNLEASHED_LAPY_PS5LOG_STUB_H

#define PS5LOG_MARK 0
#define PS5LOG_ERR  1

static inline int ps5log_init_default(const char* tag, const char* component)
{
    (void)tag;
    (void)component;
    return 0;
}

static inline void ps5log_printf(int level, const char* format, ...)
{
    (void)level;
    (void)format;
}

static inline void ps5log_close(const char* reason)
{
    (void)reason;
}

#endif
