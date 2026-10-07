/*
 * ProsperoEden - protocol adapter for the upstream PS5-Lapy-JB-Daemon client.
 * Copyright (C) 2026 BlackBearReloaded
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include <bit>
#include <cstddef>
#include <cstdint>
#include <climits>

#include "elevation.hpp"

// Keep the upstream C header byte-for-byte intact so the packaged helper's
// protocol hash can be checked against the exact header used by this client.
#define _Static_assert static_assert
#include "lapy_elevation_protocol.h"
#undef _Static_assert

namespace elevation::wire
{
inline constexpr int io_timeout_us = 5'000'000;

enum class Kind : std::uint32_t
{
    request = LAPY_ELEVATION_REQUEST,
    prepare = LAPY_ELEVATION_PREPARE,
    prepared = LAPY_ELEVATION_PREPARED,
    response = LAPY_ELEVATION_RESPONSE,
};

using Message = lapy_elevation_message;
static_assert(sizeof(Message) == 24);
static_assert(std::endian::native == std::endian::little);

constexpr Message make_request(std::uint32_t pid, Capability capability) noexcept
{
    return Message{
        LAPY_ELEVATION_MAGIC,
        LAPY_ELEVATION_VERSION,
        static_cast<std::uint16_t>(sizeof(Message)),
        LAPY_ELEVATION_REQUEST,
        static_cast<std::uint32_t>(capability),
        pid,
        LAPY_ELEVATION_OK,
    };
}

constexpr Status to_status(std::uint32_t value) noexcept
{
    return value <= LAPY_ELEVATION_PROTOCOL_ERROR
        ? static_cast<Status>(value)
        : Status::protocol_error;
}

constexpr Status validate(const Message& message) noexcept
{
    if (message.magic != LAPY_ELEVATION_MAGIC || message.size != sizeof(Message))
        return Status::invalid_request;
    if (message.version != LAPY_ELEVATION_VERSION)
        return Status::unsupported_version;
    if (message.kind < LAPY_ELEVATION_REQUEST || message.kind > LAPY_ELEVATION_RESPONSE ||
        message.pid <= 1 || message.pid > INT32_MAX ||
        message.status > LAPY_ELEVATION_PROTOCOL_ERROR)
        return Status::invalid_request;
    if (message.capability != LAPY_ELEVATION_FILESYSTEM)
        return Status::unsupported_capability;
    return Status::ok;
}

constexpr bool matches(const Message& message, const Message& request, Kind kind) noexcept
{
    return validate(message) == Status::ok &&
           message.kind == static_cast<std::uint32_t>(kind) &&
           message.pid == request.pid &&
           message.capability == request.capability;
}

template <typename Byte, typename Operation>
bool transfer(Byte* bytes, std::size_t size, Operation operation) noexcept
{
    while (size != 0)
    {
        const auto count = operation(bytes, size);
        if (count <= 0 || static_cast<std::size_t>(count) > size)
            return false;
        bytes += count;
        size -= static_cast<std::size_t>(count);
    }
    return true;
}
} // namespace elevation::wire
