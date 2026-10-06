#include <cstddef>
#include <cstdint>
#include "shader_cache.h"

ShaderCacheEntry g_shaderCacheEntries[] = {
    { 0, 0, 0, 0, 0, 0, nullptr }
};

const uint8_t g_compressedDxilCache[] = { 0x28, 0xb5, 0x2f, 0xfd, 0x20, 0x00, 0x01, 0x00, 0x00 };
const size_t g_dxilCacheCompressedSize = sizeof(g_compressedDxilCache);
const size_t g_dxilCacheDecompressedSize = 0;

const uint8_t g_compressedSpirvCache[] = { 0x28, 0xb5, 0x2f, 0xfd, 0x20, 0x00, 0x01, 0x00, 0x00 };
const size_t g_spirvCacheCompressedSize = sizeof(g_compressedSpirvCache);
const size_t g_spirvCacheDecompressedSize = 0;

const size_t g_shaderCacheEntryCount = 0;
