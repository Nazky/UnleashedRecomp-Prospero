/*
 * Compatibility shims for POSIX/GNU calls used by the pinned PacBrew
 * libcurl/OpenSSL archives that are missing or behave differently on PS5.
 *
 * These wrappers are linked locally into the title (not exported through the
 * app module). They intentionally use only the socket, pipe, and interface
 * APIs available from the PS5 platform layer.
 */
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <net/if.h>
#include <poll.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

/* Linux's recvmmsg-only flag is not present in the PS5 headers. PacBrew's
 * OpenSSL was built with this value and passes it to recvmmsg. */
#ifndef MSG_WAITFORONE
#define MSG_WAITFORONE 0x10000
#endif

/* Linux mmsghdr ABI: msghdr followed by the received/sent byte count. */
struct ps5_compat_mmsghdr {
    struct msghdr msg_hdr;
    unsigned int msg_len;
};

/* The PS5 runtime can reject F_SETFD on a socket with EINVAL. PacBrew's
 * libcurl treats setting FD_CLOEXEC as fatal during socket creation, even
 * though that flag is only about descriptor inheritance across exec. Keep
 * normal fcntl behavior, but tolerate that specific unsupported socket case. */
extern int __real_fcntl(int fd, int command, ...);

static int ps5_fcntl_command_has_no_argument(int command)
{
    if (command == F_GETFD || command == F_GETFL)
        return 1;
#ifdef F_GETOWN
    if (command == F_GETOWN)
        return 1;
#endif
#ifdef F_GETSIG
    if (command == F_GETSIG)
        return 1;
#endif
#ifdef F_GETLEASE
    if (command == F_GETLEASE)
        return 1;
#endif
#ifdef F_GETPIPE_SZ
    if (command == F_GETPIPE_SZ)
        return 1;
#endif
#ifdef F_GET_SEALS
    if (command == F_GET_SEALS)
        return 1;
#endif
#ifdef F_GETNOSIGPIPE
    if (command == F_GETNOSIGPIPE)
        return 1;
#endif
    return 0;
}

static int ps5_fcntl_command_has_flock_argument(int command)
{
#ifdef F_GETLK
    if (command == F_GETLK)
        return 1;
#endif
#ifdef F_SETLK
    if (command == F_SETLK)
        return 1;
#endif
#ifdef F_SETLKW
    if (command == F_SETLKW)
        return 1;
#endif
#ifdef F_SETLK_REMOTE
    if (command == F_SETLK_REMOTE)
        return 1;
#endif
    return 0;
}

#if defined(F_OGETLK) || defined(F_OSETLK) || defined(F_OSETLKW)
static int ps5_fcntl_command_has_old_flock_argument(int command)
{
#ifdef F_OGETLK
    if (command == F_OGETLK)
        return 1;
#endif
#ifdef F_OSETLK
    if (command == F_OSETLK)
        return 1;
#endif
#ifdef F_OSETLKW
    if (command == F_OSETLKW)
        return 1;
#endif
    return 0;
}
#endif

static int ps5_is_socket_descriptor(int fd)
{
    int socket_type = 0;
    socklen_t option_size = (socklen_t)sizeof(socket_type);
    return getsockopt(fd, SOL_SOCKET, SO_TYPE, &socket_type, &option_size) == 0;
}

__attribute__((visibility("hidden"))) int
__wrap_fcntl(int fd, int command, ...)
{
    const int entry_errno = errno;
    int integer_argument = 0;
    int result;

    if (ps5_fcntl_command_has_no_argument(command)) {
        /* A harmless zero also covers libc implementations that inspect the
         * ignored third argument for a no-argument command. */
        result = __real_fcntl(fd, command, 0);
    }
    else {
        va_list arguments;
        va_start(arguments, command);
        if (ps5_fcntl_command_has_flock_argument(command)) {
            struct flock *pointer_argument = va_arg(arguments, struct flock *);
            va_end(arguments);
            result = __real_fcntl(fd, command, pointer_argument);
        }
#if defined(F_OGETLK) || defined(F_OSETLK) || defined(F_OSETLKW)
        else if (ps5_fcntl_command_has_old_flock_argument(command)) {
            struct __oflock *pointer_argument = va_arg(arguments, struct __oflock *);
            va_end(arguments);
            result = __real_fcntl(fd, command, pointer_argument);
        }
#endif
        else {
            integer_argument = va_arg(arguments, int);
            va_end(arguments);
            result = __real_fcntl(fd, command, integer_argument);
        }
    }

    if (result >= 0)
        return result;

    const int fcntl_errno = errno;
    if (command == F_SETFD && integer_argument == FD_CLOEXEC &&
        fcntl_errno == EINVAL && ps5_is_socket_descriptor(fd)) {
        errno = entry_errno;
        return 0;
    }

    errno = fcntl_errno;
    return result;
}

__attribute__((visibility("hidden"))) int
ps5_compat_sendmmsg(int socket_fd, void *message_vector, unsigned int count, int flags)
{
    if (count == 0)
        return 0;
    if (!message_vector) {
        errno = EFAULT;
        return -1;
    }

    struct ps5_compat_mmsghdr *messages =
        (struct ps5_compat_mmsghdr *)message_vector;
    unsigned int sent = 0;
    for (; sent < count; ++sent) {
        ssize_t result = sendmsg(socket_fd, &messages[sent].msg_hdr, flags);
        if (result < 0)
            return sent ? (int)sent : -1;
        messages[sent].msg_len = (unsigned int)result;
    }
    return (int)sent;
}

static int ps5_remaining_poll_ms(const struct timespec *deadline)
{
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0)
        return -1;

    int64_t seconds = (int64_t)deadline->tv_sec - (int64_t)now.tv_sec;
    int64_t nanoseconds = (int64_t)deadline->tv_nsec - (int64_t)now.tv_nsec;
    if (nanoseconds < 0) {
        --seconds;
        nanoseconds += 1000000000;
    }
    if (seconds < 0 || (seconds == 0 && nanoseconds <= 0))
        return 0;
    if (seconds > INT_MAX / 1000)
        return INT_MAX;

    int64_t milliseconds = seconds * 1000 + (nanoseconds + 999999) / 1000000;
    return milliseconds > INT_MAX ? INT_MAX : (int)milliseconds;
}

static void ps5_update_timeout(struct timespec *timeout, const struct timespec *deadline)
{
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0)
        return;

    int64_t seconds = (int64_t)deadline->tv_sec - (int64_t)now.tv_sec;
    int64_t nanoseconds = (int64_t)deadline->tv_nsec - (int64_t)now.tv_nsec;
    if (nanoseconds < 0) {
        --seconds;
        nanoseconds += 1000000000;
    }
    if (seconds < 0 || (seconds == 0 && nanoseconds < 0)) {
        timeout->tv_sec = 0;
        timeout->tv_nsec = 0;
        return;
    }
    timeout->tv_sec = (time_t)seconds;
    timeout->tv_nsec = (long)nanoseconds;
}

__attribute__((visibility("hidden"))) int
ps5_compat_recvmmsg(int socket_fd, void *message_vector, unsigned int count,
                    int flags, struct timespec *timeout)
{
    if (count == 0)
        return 0;
    if (!message_vector) {
        errno = EFAULT;
        return -1;
    }

    struct timespec deadline = { 0, 0 };
    const int has_timeout = timeout != NULL;
    if (has_timeout) {
        if (timeout->tv_sec < 0 || timeout->tv_nsec < 0 || timeout->tv_nsec >= 1000000000L) {
            errno = EINVAL;
            return -1;
        }
        if (clock_gettime(CLOCK_MONOTONIC, &deadline) != 0)
            return -1;
        deadline.tv_sec += timeout->tv_sec;
        deadline.tv_nsec += timeout->tv_nsec;
        if (deadline.tv_nsec >= 1000000000L) {
            ++deadline.tv_sec;
            deadline.tv_nsec -= 1000000000L;
        }
    }

    struct ps5_compat_mmsghdr *messages =
        (struct ps5_compat_mmsghdr *)message_vector;
    const int wait_for_one = (flags & MSG_WAITFORONE) != 0;
    const int recv_base_flags = flags & ~MSG_WAITFORONE;
    unsigned int received = 0;

    for (; received < count; ++received) {
        int wait_ms = -1;
        if (has_timeout) {
            wait_ms = ps5_remaining_poll_ms(&deadline);
            if (wait_ms < 0) {
                if (received > 0)
                    break;
                return -1;
            }
        } else if (wait_for_one && received > 0) {
            wait_ms = 0;
        }

        if (has_timeout || (wait_for_one && received > 0)) {
            struct pollfd ready = { socket_fd, POLLIN, 0 };
            int poll_result = poll(&ready, 1, wait_ms);
            if (poll_result == 0)
                break;
            if (poll_result < 0) {
                if (received > 0)
                    break;
                return -1;
            }
        }

        int recv_flags = recv_base_flags;
        if (has_timeout || (wait_for_one && received > 0))
            recv_flags |= MSG_DONTWAIT;
        ssize_t result = recvmsg(socket_fd, &messages[received].msg_hdr, recv_flags);
        if (result < 0) {
            if (received > 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR))
                break;
            if (has_timeout && (errno == EAGAIN || errno == EWOULDBLOCK))
                break;
            if (has_timeout)
                ps5_update_timeout(timeout, &deadline);
            return -1;
        }
        messages[received].msg_len = (unsigned int)result;
    }

    if (has_timeout)
        ps5_update_timeout(timeout, &deadline);
    return (int)received;
}

static int ps5_set_pipe_flags(int fd, int flags)
{
    if (flags & O_CLOEXEC) {
        int descriptor_flags = fcntl(fd, F_GETFD);
        if (descriptor_flags < 0 || fcntl(fd, F_SETFD, descriptor_flags | FD_CLOEXEC) < 0)
            return -1;
    }
    if (flags & O_NONBLOCK) {
        int status_flags = fcntl(fd, F_GETFL);
        if (status_flags < 0 || fcntl(fd, F_SETFL, status_flags | O_NONBLOCK) < 0)
            return -1;
    }
    return 0;
}

__attribute__((visibility("hidden"))) int
ps5_compat_pipe2(int pipe_fds[2], int flags)
{
    const int supported_flags = O_CLOEXEC | O_NONBLOCK;
    if (!pipe_fds || (flags & ~supported_flags)) {
        errno = EINVAL;
        return -1;
    }
    if (pipe(pipe_fds) != 0)
        return -1;

    if (ps5_set_pipe_flags(pipe_fds[0], flags) != 0 ||
        ps5_set_pipe_flags(pipe_fds[1], flags) != 0) {
        int saved_errno = errno;
        close(pipe_fds[0]);
        close(pipe_fds[1]);
        errno = saved_errno;
        return -1;
    }
    return 0;
}

__attribute__((visibility("hidden"))) unsigned int
ps5_compat_if_nametoindex(const char *interface_name)
{
    if (!interface_name) {
        errno = EINVAL;
        return 0;
    }

    struct if_nameindex *interfaces = if_nameindex();
    if (!interfaces)
        return 0;

    unsigned int result = 0;
    for (struct if_nameindex *current = interfaces;
         current->if_index != 0 && current->if_name != NULL; ++current) {
        if (strcmp(current->if_name, interface_name) == 0) {
            result = current->if_index;
            break;
        }
    }
    if_freenameindex(interfaces);
    if (result == 0)
        errno = ENXIO;
    return result;
}
