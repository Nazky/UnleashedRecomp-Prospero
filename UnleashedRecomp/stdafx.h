#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif

#if defined(_WIN32)
#include <windows.h>
#include <ShlObj_core.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;
#elif defined(__linux__) || defined(__PROSPERO__)
#include <unistd.h>
#include <pwd.h>
#endif

#ifdef UNLEASHED_RECOMP_D3D12
#include <dxcapi.h>
#endif

#include <algorithm>
#include <mutex>
#include <filesystem>
#include <fstream>
#include <vector>
#include <string>
#include <cassert>
#include <chrono>
#include <span>
#include <xbox.h>
#include <xxhash.h>
#include <ankerl/unordered_dense.h>
#include <ddspp.h>
#include <ppc/ppc_recomp_shared.h>
#include <toml++/toml.hpp>
#include <zstd.h>
#include <stb_image.h>
#include <blockingconcurrentqueue.h>
#include <SDL.h>
#include <SDL_mixer.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <implot.h>
#include <backends/imgui_impl_sdl2.h>
#include <o1heap.h>
#include <cstddef>
#include <smolv.h>
#include <set>
#include <fmt/core.h>
#include <list>
#include <semaphore>
#include <numeric>
#include <charconv>

#include "framework.h"
#include "mutex.h"

#ifndef _WIN32
#include <sys/mman.h>
#endif

#if defined(__PROSPERO__)
#undef NULL
#define NULL __null

#if !defined(__cpp_lib_atomic_ref)
namespace std
{
    template<typename T>
    struct atomic_ref
    {
        static_assert(sizeof(std::atomic<T>) == sizeof(T), "atomic<T> size mismatch");
        std::atomic<T>* m_ptr;

        explicit atomic_ref(T& obj) noexcept
            : m_ptr(reinterpret_cast<std::atomic<T>*>(&obj))
        {
        }

        T operator=(T desired) const noexcept
        {
            m_ptr->store(desired);
            return desired;
        }

        operator T() const noexcept
        {
            return m_ptr->load();
        }

        void store(T desired, std::memory_order order = std::memory_order_seq_cst) const noexcept
        {
            m_ptr->store(desired, order);
        }

        T load(std::memory_order order = std::memory_order_seq_cst) const noexcept
        {
            return m_ptr->load(order);
        }

        T exchange(T desired, std::memory_order order = std::memory_order_seq_cst) const noexcept
        {
            return m_ptr->exchange(desired, order);
        }

        bool compare_exchange_weak(T& expected, T desired,
                                   std::memory_order order = std::memory_order_seq_cst) const noexcept
        {
            return m_ptr->compare_exchange_weak(expected, desired, order);
        }

        bool compare_exchange_strong(T& expected, T desired,
                                     std::memory_order order = std::memory_order_seq_cst) const noexcept
        {
            return m_ptr->compare_exchange_strong(expected, desired, order);
        }

        void wait(T old, std::memory_order order = std::memory_order_seq_cst) const noexcept
        {
            m_ptr->wait(old, order);
        }

        void notify_one() const noexcept
        {
            m_ptr->notify_one();
        }

        void notify_all() const noexcept
        {
            m_ptr->notify_all();
        }
    };
}
#endif
#endif
