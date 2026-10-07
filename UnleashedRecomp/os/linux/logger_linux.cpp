#include <os/logger.h>
#if defined(__PROSPERO__)
#include <cstdio>
#include <filesystem>
#include <fcntl.h>
#include <mutex>
#include <string>
#include <unistd.h>

const std::filesystem::path& GetUserPath();

static int s_klogFd = -1;
static std::mutex s_logMutex;

static FILE* OpenTraceLog()
{
    const std::filesystem::path logDirectory = GetUserPath() / "logs";
    std::error_code ec;
    std::filesystem::create_directories(logDirectory, ec);
    const std::filesystem::path logPath = logDirectory / "unleashed_trace.txt";
    return std::fopen(logPath.string().c_str(), "a");
}
#endif

void os::logger::Init()
{
#if defined(__PROSPERO__)
    std::lock_guard<std::mutex> lock(s_logMutex);
    if (s_klogFd < 0)
        s_klogFd = open("/dev/klog", O_WRONLY);
    if (FILE* f = OpenTraceLog())
    {
        std::fputs("=== UnleashedRecomp PS5 Startup ===\n", f);
        std::fflush(f);
        std::fclose(f);
    }
#endif
}

void os::logger::Log(const std::string_view str, ELogType type, const char* func)
{
    std::string line = func ? fmt::format("[{}] {}", func, str) : fmt::format("{}", str);
    fmt::println("{}", line);
#if defined(__PROSPERO__)
    std::lock_guard<std::mutex> lock(s_logMutex);
    std::string kline = fmt::format("<118>[UnleashedRecomp] {}\n", line);
    if (s_klogFd < 0)
        s_klogFd = open("/dev/klog", O_WRONLY);
    if (s_klogFd >= 0)
        write(s_klogFd, kline.data(), kline.size());
    if (FILE* f = OpenTraceLog())
    {
        std::fputs(line.c_str(), f);
        std::fputc('\n', f);
        std::fflush(f);
        std::fclose(f);
    }
#endif
}
