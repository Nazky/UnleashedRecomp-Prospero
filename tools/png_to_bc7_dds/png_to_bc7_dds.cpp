// UnleashedRecomp-Prospero - Native Linux PNG/JPG to PS5 3840x2160 BC7_UNORM DX10 DDS converter.
// Converts any PNG/JPG/BMP image (or existing BC7 DDS) into the exact 3840x2160 BC7_UNORM DX10
// 2D DDS format required by PS5 sce_sys/pic0.dds and sce_sys/pic1.dds.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb_image_resize2.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

namespace {

constexpr uint32_t TARGET_W = 3840;
constexpr uint32_t TARGET_H = 2160;
constexpr uint32_t BLOCKS_X = TARGET_W / 4;
constexpr uint32_t BLOCKS_Y = TARGET_H / 4;
constexpr size_t PAYLOAD_BYTES = size_t(BLOCKS_X) * size_t(BLOCKS_Y) * 16;
constexpr size_t DDS_HEADER_BYTES = 148;
constexpr size_t EXPECTED_DDS_BYTES = DDS_HEADER_BYTES + PAYLOAD_BYTES;

struct BitWriter128 {
    uint64_t lo = 0;
    uint64_t hi = 0;
    uint32_t pos = 0;

    void put(uint32_t val, uint32_t bits) {
        const uint64_t mask = (bits == 64) ? ~0ULL : ((1ULL << bits) - 1ULL);
        const uint64_t v = uint64_t(val) & mask;
        if (pos < 64) {
            lo |= (v << pos);
            if (pos + bits > 64) {
                hi |= (v >> (64 - pos));
            }
        } else {
            hi |= (v << (pos - 64));
        }
        pos += bits;
    }
};

// BC7 4-bit interpolation weights (0..64)
constexpr int kWeights4[16] = {
    0, 4, 9, 13, 17, 21, 26, 30,
    34, 38, 43, 47, 51, 55, 60, 64
};

void encodeBlockBC7Mode6(const uint8_t rgba[16][4], uint8_t out[16]) {
    uint8_t minC[4] = { 255, 255, 255, 255 };
    uint8_t maxC[4] = { 0, 0, 0, 0 };
    for (int i = 0; i < 16; ++i) {
        for (int c = 0; c < 4; ++c) {
            minC[c] = std::min(minC[c], rgba[i][c]);
            maxC[c] = std::max(maxC[c], rgba[i][c]);
        }
    }

    // Choose principal axis direction via bounding box diagonal
    int dir[4] = {
        int(maxC[0]) - int(minC[0]),
        int(maxC[1]) - int(minC[1]),
        int(maxC[2]) - int(minC[2]),
        int(maxC[3]) - int(minC[3])
    };

    uint8_t e0[4] = { minC[0], minC[1], minC[2], minC[3] };
    uint8_t e1[4] = { maxC[0], maxC[1], maxC[2], maxC[3] };

    // Project pixels to find the two extreme corners along the dominant axis
    int minProj = INT32_MAX;
    int maxProj = INT32_MIN;
    int minIdx = 0;
    int maxIdx = 0;
    for (int i = 0; i < 16; ++i) {
        int p = int(rgba[i][0]) * dir[0] + int(rgba[i][1]) * dir[1] +
                int(rgba[i][2]) * dir[2] + int(rgba[i][3]) * dir[3];
        if (p < minProj) { minProj = p; minIdx = i; }
        if (p > maxProj) { maxProj = p; maxIdx = i; }
    }
    if (maxProj > minProj) {
        for (int c = 0; c < 4; ++c) {
            e0[c] = rgba[minIdx][c];
            e1[c] = rgba[maxIdx][c];
        }
    }

    // Choose P0, P1 (shared LSB across R,G,B,A for each endpoint) by majority bit0
    uint32_t p0 = ((e0[0] & 1) + (e0[1] & 1) + (e0[2] & 1) + (e0[3] & 1)) >= 2 ? 1u : 0u;
    uint32_t p1 = ((e1[0] & 1) + (e1[1] & 1) + (e1[2] & 1) + (e1[3] & 1)) >= 2 ? 1u : 0u;

    uint8_t q0[4], q1[4], rec0[4], rec1[4];
    for (int c = 0; c < 4; ++c) {
        q0[c] = uint8_t(e0[c] >> 1);
        q1[c] = uint8_t(e1[c] >> 1);
        rec0[c] = uint8_t((q0[c] << 1) | p0);
        rec1[c] = uint8_t((q1[c] << 1) | p1);
    }

    uint8_t indices[16];
    for (int i = 0; i < 16; ++i) {
        int bestIdx = 0;
        int bestErr = INT32_MAX;
        for (int idx = 0; idx < 16; ++idx) {
            const int w = kWeights4[idx];
            int err = 0;
            for (int c = 0; c < 4; ++c) {
                const int interp = ((64 - w) * int(rec0[c]) + w * int(rec1[c]) + 32) >> 6;
                const int d = int(rgba[i][c]) - interp;
                err += d * d;
            }
            if (err < bestErr) {
                bestErr = err;
                bestIdx = idx;
            }
        }
        indices[i] = uint8_t(bestIdx);
    }

    // Anchor index 0 in Mode 6 uses 3 bits (MSB must be 0 -> indices[0] < 8)
    if (indices[0] >= 8) {
        for (int c = 0; c < 4; ++c) {
            std::swap(q0[c], q1[c]);
        }
        std::swap(p0, p1);
        for (int i = 0; i < 16; ++i) {
            indices[i] = uint8_t(15 - indices[i]);
        }
    }

    BitWriter128 bw;
    bw.put(0x40u, 7); // Mode 6: bit 6 set (0b1000000)
    bw.put(q0[0], 7);
    bw.put(q1[0], 7);
    bw.put(q0[1], 7);
    bw.put(q1[1], 7);
    bw.put(q0[2], 7);
    bw.put(q1[2], 7);
    bw.put(q0[3], 7);
    bw.put(q1[3], 7);
    bw.put(p0, 1);
    bw.put(p1, 1);
    bw.put(indices[0], 3);
    for (int i = 1; i < 16; ++i) {
        bw.put(indices[i], 4);
    }

    std::memcpy(out, &bw.lo, 8);
    std::memcpy(out + 8, &bw.hi, 8);
}

void writeDdsHeader(uint8_t hdr[DDS_HEADER_BYTES]) {
    std::memset(hdr, 0, DDS_HEADER_BYTES);
    auto put32 = [&](size_t off, uint32_t v) {
        std::memcpy(hdr + off, &v, sizeof(uint32_t));
    };
    std::memcpy(hdr + 0, "DDS ", 4);
    put32(4, 124);                        // dwSize
    put32(8, 0x00081007u);                // DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH | DDSD_PIXELFORMAT | DDSD_LINEARSIZE
    put32(12, TARGET_H);                  // dwHeight = 2160
    put32(16, TARGET_W);                  // dwWidth = 3840
    put32(20, uint32_t(PAYLOAD_BYTES));   // dwPitchOrLinearSize
    put32(24, 0);                         // dwDepth
    put32(28, 1);                         // dwMipMapCount = 1
    put32(76, 32);                        // ddspf.dwSize = 32
    put32(80, 0x00000004u);               // DDPF_FOURCC
    std::memcpy(hdr + 84, "DX10", 4);     // ddspf.dwFourCC = "DX10"
    put32(108, 0x00001000u);              // DDSCAPS_TEXTURE
    // DDS_HEADER_DXT10 at offset 128:
    put32(128, 98);                       // DXGI_FORMAT_BC7_UNORM = 98
    put32(132, 3);                        // D3D10_RESOURCE_DIMENSION_TEXTURE2D = 3
    put32(136, 0);                        // miscFlag = 0
    put32(140, 1);                        // arraySize = 1
    put32(144, 0);                        // miscFlags2 = 0
}

} // namespace

int main(int argc, char **argv) {
    if (argc != 3 && argc != 5) {
        std::fprintf(stderr, "Usage: %s <input.png|input.jpg|input.dds> <output.dds|output.png> [width height]\n", argv[0]);
        return 2;
    }

    const char *inPath = argv[1];
    const char *outPath = argv[2];
    const size_t outLen = std::strlen(outPath);
    const bool writePng = (outLen >= 4 && (std::strcmp(outPath + outLen - 4, ".png") == 0 ||
                                           std::strcmp(outPath + outLen - 4, ".PNG") == 0));

    if (writePng) {
        const int targetW = (argc == 5) ? std::atoi(argv[3]) : 512;
        const int targetH = (argc == 5) ? std::atoi(argv[4]) : 512;
        int w = 0, h = 0, comp = 0;
        stbi_uc *pixels = stbi_load(inPath, &w, &h, &comp, 4);
        if (!pixels) {
            std::fprintf(stderr, "png_to_bc7_dds: failed to load image '%s': %s\n", inPath, stbi_failure_reason());
            return 1;
        }
        std::vector<uint8_t> resized;
        const uint8_t *src = pixels;
        if (w != targetW || h != targetH) {
            resized.resize(size_t(targetW) * size_t(targetH) * 4);
            stbir_resize_uint8_srgb(
                pixels, w, h, w * 4,
                resized.data(), targetW, targetH, targetW * 4,
                STBIR_RGBA
            );
            src = resized.data();
        }
        int ok = stbi_write_png(outPath, targetW, targetH, 4, src, targetW * 4);
        stbi_image_free(pixels);
        if (!ok) {
            std::fprintf(stderr, "png_to_bc7_dds: failed to write PNG '%s'\n", outPath);
            return 1;
        }
        return 0;
    }

    // Check if input is already a valid 3840x2160 BC7 DX10 DDS
    if (FILE *f = std::fopen(inPath, "rb")) {
        uint8_t magic[4] = {};
        if (std::fread(magic, 1, 4, f) == 4 && std::memcmp(magic, "DDS ", 4) == 0) {
            std::fseek(f, 0, SEEK_END);
            long sz = std::ftell(f);
            std::fseek(f, 0, SEEK_SET);
            if (sz == long(EXPECTED_DDS_BYTES)) {
                std::vector<uint8_t> buf(EXPECTED_DDS_BYTES);
                if (std::fread(buf.data(), 1, EXPECTED_DDS_BYTES, f) == EXPECTED_DDS_BYTES) {
                    std::fclose(f);
                    FILE *out = std::fopen(outPath, "wb");
                    if (!out || std::fwrite(buf.data(), 1, EXPECTED_DDS_BYTES, out) != EXPECTED_DDS_BYTES) {
                        std::fprintf(stderr, "Failed to write %s\n", outPath);
                        if (out) std::fclose(out);
                        return 1;
                    }
                    std::fclose(out);
                    return 0;
                }
            }
        }
        std::fclose(f);
    }

    int w = 0, h = 0, comp = 0;
    stbi_uc *pixels = stbi_load(inPath, &w, &h, &comp, 4);
    if (!pixels) {
        std::fprintf(stderr, "png_to_bc7_dds: failed to load image '%s': %s\n", inPath, stbi_failure_reason());
        return 1;
    }

    std::vector<uint8_t> rgba3840;
    const uint8_t *src = pixels;
    if (uint32_t(w) != TARGET_W || uint32_t(h) != TARGET_H) {
        rgba3840.resize(size_t(TARGET_W) * size_t(TARGET_H) * 4);
        stbir_resize_uint8_srgb(
            pixels, w, h, w * 4,
            rgba3840.data(), int(TARGET_W), int(TARGET_H), int(TARGET_W * 4),
            STBIR_RGBA
        );
        src = rgba3840.data();
    }

    std::vector<uint8_t> dds(EXPECTED_DDS_BYTES);
    writeDdsHeader(dds.data());
    uint8_t *dstBlocks = dds.data() + DDS_HEADER_BYTES;

    #pragma omp parallel for schedule(static)
    for (int by = 0; by < int(BLOCKS_Y); ++by) {
        for (int bx = 0; bx < int(BLOCKS_X); ++bx) {
            uint8_t blockRgba[16][4];
            for (int py = 0; py < 4; ++py) {
                const uint8_t *row = src + (size_t(by * 4 + py) * TARGET_W + size_t(bx * 4)) * 4;
                for (int px = 0; px < 4; ++px) {
                    const int idx = py * 4 + px;
                    blockRgba[idx][0] = row[px * 4 + 0];
                    blockRgba[idx][1] = row[px * 4 + 1];
                    blockRgba[idx][2] = row[px * 4 + 2];
                    blockRgba[idx][3] = 255; // Opaque background
                }
            }
            uint8_t *outBlock = dstBlocks + (size_t(by) * BLOCKS_X + size_t(bx)) * 16;
            encodeBlockBC7Mode6(blockRgba, outBlock);
        }
    }

    stbi_image_free(pixels);

    FILE *out = std::fopen(outPath, "wb");
    if (!out || std::fwrite(dds.data(), 1, dds.size(), out) != dds.size()) {
        std::fprintf(stderr, "png_to_bc7_dds: failed to write '%s'\n", outPath);
        if (out) std::fclose(out);
        return 1;
    }
    std::fclose(out);
    return 0;
}
