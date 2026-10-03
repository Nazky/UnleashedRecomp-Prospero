#!/usr/bin/env bash
# PS5 Vulkan - link recipe for titles that link RADV.
# Copyright (C) 2026 Mihawk-99
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Sourced by tools/build-radv-title.sh (and, later, by every title that links
# the RADV archive), so each links it the same way:
# - the archive my Mesa fork builds with -Dradv-winsys=ps5 (RADV, its compiler
#   and Mesa's runtime in one), linked whole;
# - the payload SDK's libc++, libc++abi and libunwind for ACO's C++, and
#   Clang's builtins for __emutls_get_address, as tools/psbc-link.sh has them;
# - the SDK's platform layer, whose ps5_ functions stand in for the libc
#   functions no system module exports, bound to libc's names here: a title
#   that defined libc's names itself would export them, which the title
#   converter refuses (platform/include/ps5platform/libc.h in the SDK fork).
#
# radv_link_recipe ROOT SDK_ROOT ARCHIVE sets radv_linker_script,
# radv_link_inputs and radv_link_flags. It returns 2 when an input is missing.

radv_link_recipe() {
    local root=$1 sdk_root=$2 archive=$3
    local compiler=${PS5_CLANG:-$(command -v clang || command -v clang-18 || true)}
    [[ -n $compiler ]] || { echo "clang was not found; set PS5_CLANG" >&2; return 2; }
    local builtins
    builtins="$("$compiler" --print-resource-dir)/lib/linux/libclang_rt.builtins-x86_64.a"
    if [[ ! -f "$builtins" ]]; then
        builtins="$(find "$("$compiler" --print-resource-dir)/lib" -name "libclang_rt.builtins*x86_64*.a" -o -path "*x86_64*/libclang_rt.builtins.a" 2>/dev/null | head -n 1 || true)"
    fi
    if [[ -z "$builtins" || ! -f "$builtins" ]]; then
        builtins="$root/vendor/ps5/sdk/libclang_rt.builtins-x86_64.a"
    fi
    if [[ ! -f "$builtins" && -f "$sdk_root/target/lib/libc.a" ]]; then
        mkdir -p "$root/build/builtins-tmp"
        cp -f "$(gcc -print-libgcc-file-name)" "$root/build/libclang_rt.builtins-fallback.a"
        (cd "$root/build/builtins-tmp" && ar d "$root/build/libclang_rt.builtins-fallback.a" generic-morestack-thread.o 2>/dev/null || true)
        rm -rf "$root/build/builtins-tmp"
        builtins="$root/build/libclang_rt.builtins-fallback.a"
    fi
    local platform="$sdk_root/target/lib/libps5platform.a"

    mkdir -p "$root/build"
    cat > "$root/build/ps5-emutls-cxa.c" << 'EOF_EMUTLS'
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* Delay emutls deallocation and drain C++ thread_local destructors first
 * while emutls storage is still live on the exiting thread. */
#define EMUTLS_SKIP_DESTRUCTOR_ROUNDS 2

struct thread_destructor {
    void (*destructor)(void *);
    void *object;
    struct thread_destructor *next;
};

static pthread_key_t destructors_key;
static pthread_once_t destructors_once = PTHREAD_ONCE_INIT;
static bool destructors_ready;

static void
run_destructors(void *list)
{
    struct thread_destructor *entry = (struct thread_destructor *)list;
    while (entry) {
        struct thread_destructor *const next = entry->next;
        entry->destructor(entry->object);
        free(entry);
        struct thread_destructor *const added =
            (struct thread_destructor *)pthread_getspecific(destructors_key);
        if (added) {
            pthread_setspecific(destructors_key, NULL);
            run_destructors(added);
        }
        entry = next;
    }
}

static void
drain_thread_destructors(void)
{
    if (!destructors_ready)
        return;
    struct thread_destructor *const list =
        (struct thread_destructor *)pthread_getspecific(destructors_key);
    if (list) {
        pthread_setspecific(destructors_key, NULL);
        run_destructors(list);
    }
}

static void
setup_destructors(void)
{
    destructors_ready =
        pthread_key_create(&destructors_key, run_destructors) == 0 &&
        atexit(drain_thread_destructors) == 0;
}

__attribute__((visibility("hidden"))) int
ps5___cxa_thread_atexit_impl(void (*destructor)(void *), void *object, void *dso)
{
    (void)dso;
    pthread_once(&destructors_once, setup_destructors);
    if (!destructors_ready)
        return -1;
    struct thread_destructor *const entry =
        (struct thread_destructor *)malloc(sizeof(*entry));
    if (!entry)
        return -1;
    entry->destructor = destructor;
    entry->object = object;
    entry->next = (struct thread_destructor *)pthread_getspecific(destructors_key);
    if (pthread_setspecific(destructors_key, entry) != 0) {
        free(entry);
        return -1;
    }
    return 0;
}

typedef struct emutls_address_array {
    uintptr_t skip_destructor_rounds;
    uintptr_t size;
    void *data[];
} emutls_address_array;

typedef unsigned int gcc_word __attribute__((mode(word)));
typedef unsigned int gcc_pointer __attribute__((mode(pointer)));

typedef struct __emutls_control {
    gcc_word size;
    gcc_word align;
    union {
        uintptr_t index;
        void *address;
    } object;
    void *value;
} __emutls_control;

static pthread_mutex_t emutls_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_key_t emutls_pthread_key;
static size_t emutls_num_object = 0;

static __inline void *
emutls_memalign_alloc(size_t align, size_t size)
{
    if (align < sizeof(void *))
        align = sizeof(void *);
    const size_t extra = align - 1 + sizeof(void *);
    char *const object = (char *)malloc(extra + size);
    if (!object)
        abort();
    void *const base =
        (void *)(((uintptr_t)(object + extra)) & ~(uintptr_t)(align - 1));
    ((void **)base)[-1] = object;
    return base;
}

static __inline void
emutls_memalign_free(void *base)
{
    free(((void **)base)[-1]);
}

static __inline void
emutls_setspecific(emutls_address_array *value)
{
    pthread_setspecific(emutls_pthread_key, (void *)value);
}

static __inline emutls_address_array *
emutls_getspecific(void)
{
    return (emutls_address_array *)pthread_getspecific(emutls_pthread_key);
}

static void
emutls_shutdown(emutls_address_array *array)
{
    if (array) {
        for (uintptr_t i = 0; i < array->size; ++i) {
            if (array->data[i]) {
                void *const ptr = array->data[i];
                array->data[i] = NULL;
                emutls_memalign_free(ptr);
            }
        }
    }
}

static void
emutls_key_destructor(void *ptr)
{
    emutls_address_array *array = (emutls_address_array *)ptr;
    if (destructors_ready && pthread_getspecific(destructors_key) != NULL) {
        emutls_setspecific(array);
        drain_thread_destructors();
    }
    if (array->skip_destructor_rounds > 0) {
        array->skip_destructor_rounds--;
        emutls_setspecific(array);
    } else {
        emutls_setspecific(NULL);
        emutls_shutdown(array);
        free(ptr);
    }
}

static void
emutls_init(void)
{
    if (pthread_key_create(&emutls_pthread_key, emutls_key_destructor) != 0)
        abort();
}

static __inline void
emutls_init_once(void)
{
    static pthread_once_t once = PTHREAD_ONCE_INIT;
    pthread_once(&once, emutls_init);
}

static __inline void *
emutls_allocate_object(__emutls_control *control)
{
    size_t size = control->size;
    size_t align = control->align;
    if (align < sizeof(void *))
        align = sizeof(void *);
    if ((align & (align - 1)) != 0)
        abort();
    void *base = emutls_memalign_alloc(align, size);
    if (control->value)
        memcpy(base, control->value, size);
    else
        memset(base, 0, size);
    return base;
}

static __inline uintptr_t
emutls_get_index(__emutls_control *control)
{
    uintptr_t index = __atomic_load_n(&control->object.index, __ATOMIC_ACQUIRE);
    if (!index) {
        emutls_init_once();
        pthread_mutex_lock(&emutls_mutex);
        index = control->object.index;
        if (!index) {
            index = ++emutls_num_object;
            __atomic_store_n(&control->object.index, index, __ATOMIC_RELEASE);
        }
        pthread_mutex_unlock(&emutls_mutex);
    }
    return index;
}

static __inline void
emutls_check_array_set_size(emutls_address_array *array, uintptr_t size)
{
    if (array == NULL)
        abort();
    array->size = size;
    emutls_setspecific(array);
}

static __inline uintptr_t
emutls_new_data_array_size(uintptr_t index)
{
    uintptr_t header_words = sizeof(emutls_address_array) / sizeof(void *);
    return ((index + header_words + 15) & ~((uintptr_t)15)) - header_words;
}

static __inline uintptr_t
emutls_asize(uintptr_t N)
{
    return N * sizeof(void *) + sizeof(emutls_address_array);
}

static __inline emutls_address_array *
emutls_get_address_array(uintptr_t index)
{
    emutls_address_array *array = emutls_getspecific();
    if (array == NULL) {
        uintptr_t new_size = emutls_new_data_array_size(index);
        array = (emutls_address_array *)malloc(emutls_asize(new_size));
        if (array) {
            memset(array->data, 0, new_size * sizeof(void *));
            array->skip_destructor_rounds = EMUTLS_SKIP_DESTRUCTOR_ROUNDS;
        }
        emutls_check_array_set_size(array, new_size);
    } else if (index > array->size) {
        uintptr_t orig_size = array->size;
        uintptr_t new_size = emutls_new_data_array_size(index);
        array = (emutls_address_array *)realloc(array, emutls_asize(new_size));
        if (array)
            memset(array->data + orig_size, 0,
                   (new_size - orig_size) * sizeof(void *));
        emutls_check_array_set_size(array, new_size);
    }
    return array;
}

__attribute__((visibility("hidden"))) void *
__emutls_get_address(__emutls_control *control)
{
    uintptr_t index = emutls_get_index(control);
    emutls_address_array *array = emutls_get_address_array(index--);
    if (array->data[index] == NULL)
        array->data[index] = emutls_allocate_object(control);
    return array->data[index];
}
EOF_EMUTLS
    "$compiler" --target=x86_64-sie-ps5 -fPIC -fno-plt -fno-stack-protector \
        -isysroot "$sdk_root" -isystem "$sdk_root/target/include" \
        -O2 -c "$root/build/ps5-emutls-cxa.c" -o "$root/build/ps5-emutls-cxa.o"

    radv_linker_script=(-T "$root/tooling/psbc/ps5-pie-unwind.ld" -L "$root/tooling/native")
    # Whole: Mesa's dispatch tables name every entry point through a weak
    # reference, and a weak reference pulls no archive member in, so an entry
    # point whose object nothing else needs would be NULL (the first console
    # run of PPSA99014 called one inside wsi_device_init).
    radv_link_inputs=(-L "$sdk_root/target/lib" "$root/build/ps5-emutls-cxa.o"
        --whole-archive "$archive" --no-whole-archive
        --start-group "$sdk_root/target/lib/libc++.a" "$sdk_root/target/lib/libc++abi.a"
        "$sdk_root/target/lib/libunwind.a" "$builtins" "$platform" --end-group)
    # Threads that ask for no stack get the main thread's 2 MiB in direct
    # memory (the platform's thread wraps), for RADV's, the CTS's and libc++'s
    # alike; join and detach free the stacks.
    radv_link_flags=(--no-dynamic-linker --wrap=pthread_create --wrap=pthread_join --wrap=pthread_detach)
    # The platform's open_memstream publishes its buffer at fflush and fclose
    # (src/memstream.c in the SDK fork's platform layer): Mesa's u_memstream,
    # RADV's recorded shader IR among its users.
    radv_link_flags+=(--wrap=fclose --wrap=fflush)
    local name
    # Every allocation the title makes goes to the platform's heap in direct
    # memory (ps5platform/heap.h): libc's private heap ran out under the CTS's
    # first shader build.
    for name in malloc calloc realloc free posix_memalign aligned_alloc memalign \
            malloc_usable_size reallocf reallocarray getline getdelim; do
        radv_link_flags+=("--wrap=$name")
    done
    for name in qsort_r mkstemps openlog popen pclose open_memstream __xuname __assert \
            __memset_chk regcomp regexec regfree regerror localtime_r newlocale freelocale \
            strtod_l strtof_l dladdr utimensat localeconv_l strtoll_l strtoull_l strtold_l \
            snprintf_l sscanf_l asprintf_l strcoll_l strxfrm_l strftime_l wcscoll_l wcsxfrm_l \
            btowc_l wctob_l iswctype_l mbrlen_l mbrtowc_l mbsrtowcs_l mbsnrtowcs_l wcrtomb_l \
            wcsnrtombs_l mbtowc_l ___mb_cur_max_l ___runetype_l ___tolower_l ___toupper_l \
            __runes_for_locale catopen catgets catclose backtrace backtrace_symbols_fd \
            __cxa_thread_atexit_impl ffs timegm; do
        radv_link_flags+=("--defsym=$name=ps5_$name")
    done
    radv_link_flags+=("--defsym=___mb_cur_max=ps5____mb_cur_max_l")
    # The rest of the platform's libc (ps5platform/libc.h): functions no system
    # module exports, which a title's import leaves pointing at nothing (libc++'s
    # random_device called arc4random through NULL:
    # dEQP-VK.pipeline.*.creation_cache_control), and those exported but
    # refused to a title or faulting in it. The directory functions go
    # together: a DIR from ps5_opendir is the platform's own.
    for name in arc4random arc4random_buf arc4random_uniform gmtime_r statvfs fstatvfs \
            futimens clock_nanosleep getaddrinfo freeaddrinfo if_nameindex if_freenameindex \
            opendir fdopendir readdir rewinddir dirfd closedir nl_langinfo nl_langinfo_l getpwuid_r \
            posix_fallocate access \
            openat unlinkat fchmodat fstatat mkdirat renameat memfd_create; do
        radv_link_flags+=("--defsym=$name=ps5_$name")
    done
    # A bound name the SDK's stub libraries also define would be exported from
    # the title to override theirs, and the title converter refuses exports:
    # every bound name stays local.
    local map="$root/build/radv-platform-local.map"
    mkdir -p "$root/build"
    {
        printf '{\n    local:\n'
        local flag
        for flag in "${radv_link_flags[@]}"; do
            [[ $flag == --defsym=* ]] || continue
            flag=${flag#--defsym=}
            printf '        %s;\n' "${flag%%=*}"
        done
        printf '};\n'
    } > "$map.tmp"
    mv "$map.tmp" "$map"
    radv_link_flags+=(--version-script "$map")

    local file
    for file in "$archive" "$platform" "$sdk_root/target/lib/libc++.a" \
            "$sdk_root/target/lib/libc++abi.a" "$sdk_root/target/lib/libunwind.a" "$builtins"; do
        [[ -f $file ]] || { echo "missing $file" >&2; return 2; }
    done
}
