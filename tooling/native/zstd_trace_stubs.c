/*
 * Optional Zstandard tracing hooks are weak references in the PacBrew zstd
 * archive. The PS5 module converter requires every dynamic undefined symbol
 * to map to an SDK import, so provide disabled implementations locally.
 */

typedef unsigned long long ZSTD_TraceCtx;

struct ZSTD_CCtx_s;
struct ZSTD_DCtx_s;
typedef struct ZSTD_Trace ZSTD_Trace;

#if defined(__GNUC__)
#define ZSTD_TRACE_HIDDEN __attribute__((visibility("hidden")))
#else
#define ZSTD_TRACE_HIDDEN
#endif

ZSTD_TRACE_HIDDEN ZSTD_TraceCtx ZSTD_trace_compress_begin(const struct ZSTD_CCtx_s *cctx)
{
    (void)cctx;
    return 0;
}

ZSTD_TRACE_HIDDEN void ZSTD_trace_compress_end(ZSTD_TraceCtx ctx, const ZSTD_Trace *trace)
{
    (void)ctx;
    (void)trace;
}

ZSTD_TRACE_HIDDEN ZSTD_TraceCtx ZSTD_trace_decompress_begin(const struct ZSTD_DCtx_s *dctx)
{
    (void)dctx;
    return 0;
}

ZSTD_TRACE_HIDDEN void ZSTD_trace_decompress_end(ZSTD_TraceCtx ctx, const ZSTD_Trace *trace)
{
    (void)ctx;
    (void)trace;
}
