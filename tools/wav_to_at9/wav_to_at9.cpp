// wav_to_at9.cpp - Standalone audio (.wav, .mp3, .ogg, .flac) to PS5 ATRAC9 (snd0.at9) encoder
// Follows Sony's PS5 hardware ATRAC9 profile and blackbearreloaded/ps5-at9-converter:
//   - 48000 Hz, 2 channels (Stereo), looped WAVE_FORMAT_EXTENSIBLE (.at9)
//   - Automatic bitrate selection (192 / 168 / 144 / 96 kbps) to fit within 2 MiB
//   - High-quality Kaiser-windowed sinc resampling to 48000 Hz
//   - Psychoacoustic Bark-band masking & 4-frame superframe dynamic bit allocation
//   - Sony hardware decoder compliance:
//       * Superframe unused trailing bytes padded with 0x01 (API error 514, detail 577 if 0x00)
//       * 1 <= GradientStartUnit < GradientEndUnit <= 31 and GradientStartValue != GradientEndValue
//       * Coarse precisions <= 15 (PrecisionsFine == 0)
//       * Non-wrapping scale-factor coding in 0..31 across all coding modes
//       * Codebook minimum-magnitude rules enforced for units 0..11

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <numeric>
#include <string>
#include <vector>

#define STB_VORBIS_HEADER_ONLY
#include "../../thirdparty/stb/stb_vorbis.c"
#define DR_MP3_IMPLEMENTATION
#include "../../thirdparty/stb/dr_mp3.h"
#define DR_FLAC_IMPLEMENTATION
#include "../../thirdparty/stb/dr_flac.h"
#undef STB_VORBIS_HEADER_ONLY
#include "../../thirdparty/stb/stb_vorbis.c"

extern "C" {
#include "../../thirdparty/libatrac9/src/bit_allocation.h"
#include "../../thirdparty/libatrac9/src/huffCodes.h"
#include "../../thirdparty/libatrac9/src/structures.h"
#include "../../thirdparty/libatrac9/src/tables.h"
}

namespace {

constexpr uint32_t TARGET_SAMPLE_RATE = 48000;
constexpr uint32_t SAMPLE_RATE_INDEX  = 7;   // 48000 Hz
constexpr uint32_t CHANNEL_CONFIG_IDX = 2;   // Stereo (1 STEREO block)
constexpr uint32_t SUPERFRAME_INDEX   = 2;   // 4 frames per superframe
constexpr int      FRAMES_PER_SF      = 4;
constexpr int      FRAME_SAMPLES      = 256;
constexpr int      SUPERFRAME_SAMPLES = FRAME_SAMPLES * FRAMES_PER_SF; // 1024
constexpr int      ENCODER_DELAY      = 256; // One MDCT overlap window of lead-in
constexpr uint32_t MAX_AT9_FILE_BYTES = 2 * 1024 * 1024; // 2 MiB PS5 snd0.at9 limit

constexpr int MIN_BAND  = 3;  // MinBandCount[0] = 3 in ATRAC9
constexpr int MAX_UNITS = 30; // Max quant units

//------------------------------------------------------------------------------
// Bitstream Writer (MSB-first)
//------------------------------------------------------------------------------
class BitWriter {
public:
    void write(uint32_t val, int bits) {
        if (bits <= 0) return;
        for (int i = bits - 1; i >= 0; --i) {
            const uint8_t bit = (val >> i) & 1u;
            if ((bitPos_ & 7) == 0) {
                buf_.push_back(0);
            }
            buf_.back() |= uint8_t(bit << (7 - (bitPos_ & 7)));
            ++bitPos_;
        }
    }

    void align() {
        const int rem = bitPos_ & 7;
        if (rem != 0) {
            write(0, 8 - rem);
        }
    }

    int bitLength() const { return bitPos_; }
    const std::vector<uint8_t>& bytes() const { return buf_; }

private:
    std::vector<uint8_t> buf_;
    int bitPos_ = 0;
};

static inline void writeBytes(std::vector<uint8_t>& out, const void* data, size_t len) {
    const auto* p = static_cast<const uint8_t*>(data);
    out.insert(out.end(), p, p + len);
}

static inline void writeU16LE(std::vector<uint8_t>& out, uint16_t v) {
    uint8_t b[2] = { uint8_t(v & 0xFF), uint8_t((v >> 8) & 0xFF) };
    writeBytes(out, b, 2);
}

static inline void writeU32LE(std::vector<uint8_t>& out, uint32_t v) {
    uint8_t b[4] = {
        uint8_t(v & 0xFF),
        uint8_t((v >> 8) & 0xFF),
        uint8_t((v >> 16) & 0xFF),
        uint8_t((v >> 24) & 0xFF)
    };
    writeBytes(out, b, 4);
}

//------------------------------------------------------------------------------
// Audio File Decoding (.wav, .flac, .ogg, .mp3)
//------------------------------------------------------------------------------
static inline uint16_t readU16LE(const uint8_t* p) {
    return uint16_t(p[0]) | (uint16_t(p[1]) << 8);
}

static inline uint32_t readU32LE(const uint8_t* p) {
    return uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
}

static bool loadWavFromMemory(const uint8_t* data, size_t size,
                              std::vector<std::vector<double>>& outChannels,
                              uint32_t& outSampleRate) {
    if (size < 44 || std::memcmp(data, "RIFF", 4) != 0 || std::memcmp(data + 8, "WAVE", 4) != 0) {
        return false;
    }

    uint16_t formatTag = 0;
    uint16_t channels = 0;
    uint32_t sampleRate = 0;
    uint16_t bitsPerSample = 0;
    const uint8_t* pcmData = nullptr;
    size_t pcmSize = 0;

    size_t offset = 12;
    while (offset + 8 <= size) {
        const uint8_t* chunkHdr = data + offset;
        const uint32_t chunkSize = readU32LE(chunkHdr + 4);
        const size_t payloadOffset = offset + 8;
        if (payloadOffset + chunkSize > size) break;

        if (std::memcmp(chunkHdr, "fmt ", 4) == 0 && chunkSize >= 16) {
            formatTag = readU16LE(data + payloadOffset + 0);
            channels = readU16LE(data + payloadOffset + 2);
            sampleRate = readU32LE(data + payloadOffset + 4);
            bitsPerSample = readU16LE(data + payloadOffset + 14);
            if (formatTag == 0xFFFE && chunkSize >= 40) {
                formatTag = readU16LE(data + payloadOffset + 24);
            }
        } else if (std::memcmp(chunkHdr, "data", 4) == 0) {
            pcmData = data + payloadOffset;
            pcmSize = chunkSize;
        }
        offset = payloadOffset + chunkSize + (chunkSize & 1u);
    }

    if (!pcmData || pcmSize == 0 || channels == 0 || sampleRate == 0) {
        return false;
    }
    if (formatTag != 1 && formatTag != 3) {
        return false;
    }

    const size_t bytesPerSample = bitsPerSample / 8;
    if (bytesPerSample == 0) return false;
    const size_t frameStride = bytesPerSample * channels;
    const size_t numFrames = pcmSize / frameStride;

    outSampleRate = sampleRate;
    outChannels.assign(channels, std::vector<double>(numFrames, 0.0));

    for (size_t i = 0; i < numFrames; ++i) {
        for (uint16_t c = 0; c < channels; ++c) {
            const uint8_t* sp = pcmData + i * frameStride + c * bytesPerSample;
            double v = 0.0;
            if (formatTag == 1) {
                if (bitsPerSample == 8) {
                    v = (int(sp[0]) - 128) / 128.0;
                } else if (bitsPerSample == 16) {
                    const int16_t s = int16_t(readU16LE(sp));
                    v = double(s) / 32768.0;
                } else if (bitsPerSample == 24) {
                    int32_t s = int32_t(sp[0]) | (int32_t(sp[1]) << 8) | (int32_t(sp[2]) << 16);
                    if (s & 0x800000) s |= ~0xFFFFFF;
                    v = double(s) / 8388608.0;
                } else if (bitsPerSample == 32) {
                    const int32_t s = int32_t(readU32LE(sp));
                    v = double(s) / 2147483648.0;
                }
            } else if (formatTag == 3) {
                if (bitsPerSample == 32) {
                    float f = 0.0f;
                    std::memcpy(&f, sp, sizeof(float));
                    v = double(f);
                } else if (bitsPerSample == 64) {
                    double d = 0.0;
                    std::memcpy(&d, sp, sizeof(double));
                    v = d;
                }
            }
            outChannels[c][i] = std::clamp(v, -1.0, 1.0);
        }
    }
    return true;
}

static bool loadAudioFile(const char* path,
                          std::vector<std::vector<double>>& outChannels,
                          uint32_t& outSampleRate) {
    FILE* f = std::fopen(path, "rb");
    if (!f) return false;
    std::fseek(f, 0, SEEK_END);
    const long fileLen = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    if (fileLen <= 0) {
        std::fclose(f);
        return false;
    }

    std::vector<uint8_t> buf(static_cast<size_t>(fileLen));
    if (std::fread(buf.data(), 1, buf.size(), f) != buf.size()) {
        std::fclose(f);
        return false;
    }
    std::fclose(f);

    // 1. RIFF/WAVE
    if (loadWavFromMemory(buf.data(), buf.size(), outChannels, outSampleRate)) {
        return true;
    }

    // 2. FLAC
    auto tryFlac = [&]() -> bool {
        unsigned int ch = 0, sr = 0;
        drflac_uint64 frames = 0;
        float* decoded = drflac_open_memory_and_read_pcm_frames_f32(buf.data(), buf.size(), &ch, &sr, &frames, nullptr);
        if (!decoded || frames == 0 || ch == 0 || sr == 0) {
            if (decoded) drflac_free(decoded, nullptr);
            return false;
        }
        outSampleRate = sr;
        outChannels.assign(ch, std::vector<double>(size_t(frames), 0.0));
        for (size_t i = 0; i < size_t(frames); ++i) {
            for (unsigned int c = 0; c < ch; ++c) {
                outChannels[c][i] = std::clamp(double(decoded[i * ch + c]), -1.0, 1.0);
            }
        }
        drflac_free(decoded, nullptr);
        return true;
    };

    // 3. Ogg Vorbis
    auto tryVorbis = [&]() -> bool {
        if (buf.size() < 4 || std::memcmp(buf.data(), "OggS", 4) != 0) {
            return false;
        }
        int ch = 0, sr = 0;
        short* decoded = nullptr;
        const int frames = stb_vorbis_decode_memory(buf.data(), int(buf.size()), &ch, &sr, &decoded);
        if (frames <= 0 || !decoded || ch <= 0 || sr <= 0) {
            if (decoded) std::free(decoded);
            return false;
        }
        outSampleRate = uint32_t(sr);
        outChannels.assign(size_t(ch), std::vector<double>(size_t(frames), 0.0));
        for (int i = 0; i < frames; ++i) {
            for (int c = 0; c < ch; ++c) {
                outChannels[size_t(c)][size_t(i)] = double(decoded[size_t(i) * ch + c]) / 32768.0;
            }
        }
        std::free(decoded);
        return true;
    };

    // 4. MP3 (skip any number of stacked leading ID3v2 tags before passing to dr_mp3)
    auto tryMp3 = [&]() -> bool {
        const uint8_t* mp3Ptr = buf.data();
        size_t mp3Size = buf.size();
        while (mp3Size >= 10 && std::memcmp(mp3Ptr, "ID3", 3) == 0) {
            const uint32_t tagSize =
                ((uint32_t(mp3Ptr[6]) & 0x7Fu) << 21) |
                ((uint32_t(mp3Ptr[7]) & 0x7Fu) << 14) |
                ((uint32_t(mp3Ptr[8]) & 0x7Fu) << 7)  |
                ((uint32_t(mp3Ptr[9]) & 0x7Fu) << 0);
            const size_t totalTag = 10 + size_t(tagSize) + ((mp3Ptr[5] & 0x10u) ? 10u : 0u);
            if (totalTag >= mp3Size) break;
            mp3Ptr += totalTag;
            mp3Size -= totalTag;
        }

        drmp3_config cfg{};
        drmp3_uint64 frames = 0;
        float* decoded = drmp3_open_memory_and_read_pcm_frames_f32(mp3Ptr, mp3Size, &cfg, &frames, nullptr);
        if (!decoded || frames == 0 || cfg.channels == 0 || cfg.sampleRate == 0) {
            if (decoded) drmp3_free(decoded, nullptr);
            return false;
        }
        outSampleRate = cfg.sampleRate;
        outChannels.assign(cfg.channels, std::vector<double>(size_t(frames), 0.0));
        for (size_t i = 0; i < size_t(frames); ++i) {
            for (uint32_t c = 0; c < cfg.channels; ++c) {
                outChannels[c][i] = std::clamp(double(decoded[i * cfg.channels + c]), -1.0, 1.0);
            }
        }
        drmp3_free(decoded, nullptr);
        return true;
    };

    if (buf.size() >= 4 && std::memcmp(buf.data(), "fLaC", 4) == 0 && tryFlac()) return true;
    if (buf.size() >= 4 && std::memcmp(buf.data(), "OggS", 4) == 0 && tryVorbis()) return true;
    if (tryMp3()) return true;
    if (tryFlac()) return true;
    return false;
}

//------------------------------------------------------------------------------
// DSP: Channel Mapping, Kaiser-Windowed Sinc Resampler, Fades & Peak Limiting
//------------------------------------------------------------------------------
static std::array<std::vector<double>, 2> toStereo(const std::vector<std::vector<double>>& in) {
    std::array<std::vector<double>, 2> out;
    if (in.empty()) return out;
    const size_t n = in[0].size();
    if (in.size() == 2) {
        out[0] = in[0];
        out[1] = in[1];
    } else if (in.size() == 1) {
        out[0] = in[0];
        out[1] = in[0];
    } else if (in.size() == 6) {
        const double k = 1.0 / std::sqrt(2.0);
        const double norm = 1.0 / (1.0 + 2.0 * k);
        out[0].resize(n);
        out[1].resize(n);
        for (size_t i = 0; i < n; ++i) {
            out[0][i] = (in[0][i] + k * in[2][i] + k * in[4][i]) * norm;
            out[1][i] = (in[1][i] + k * in[2][i] + k * in[5][i]) * norm;
        }
    } else {
        out[0] = in[0];
        out[1] = in[1];
    }
    return out;
}

static double besselI0(double x) {
    double sum = 1.0;
    double term = 1.0;
    const double halfX = x * 0.5;
    for (int k = 1; k < 40; ++k) {
        const double r = halfX / double(k);
        term *= r * r;
        sum += term;
        if (term < sum * 1e-15) break;
    }
    return sum;
}

static double sincNorm(double x) {
    if (std::abs(x) < 1e-12) return 1.0;
    const double pix = M_PI * x;
    return std::sin(pix) / pix;
}

static void resampleStereo(std::array<std::vector<double>, 2>& pcm, uint32_t rateIn, uint32_t rateOut) {
    if (rateIn == rateOut || pcm[0].empty()) return;

    const uint32_t g = std::gcd(rateIn, rateOut);
    const int up = int(rateOut / g);
    const int down = int(rateIn / g);

    const double nyquist = 0.5 * double(std::min(rateIn, rateOut));
    const double passHz = std::min(20000.0, 0.9 * nyquist);
    const double stopHz = nyquist;
    const double attenDb = 100.0;

    const double beta = 0.1102 * (attenDb - 8.7);
    const double width = (stopHz - passHz) / double(rateIn);
    const int taps = int(std::ceil((attenDb - 8.0) / (2.285 * 2.0 * M_PI * width)));
    const int half = std::max(8, taps / 2 + 1);
    const int numTaps = 2 * half;
    const double cutoff = 0.5 * (passHz + stopHz) / double(rateIn);
    const double invI0Beta = 1.0 / besselI0(beta);

    std::vector<double> h(size_t(up) * size_t(numTaps), 0.0);
    for (int phase = 0; phase < up; ++phase) {
        const double p = double(phase) / double(up);
        double rowSum = 0.0;
        double* row = &h[size_t(phase) * size_t(numTaps)];
        for (int idx = 0; idx < numTaps; ++idx) {
            const int k = -half + 1 + idx;
            const double t = double(k) - p;
            const double u = t / double(half);
            const double winArg = std::max(0.0, 1.0 - u * u);
            const double win = besselI0(beta * std::sqrt(winArg)) * invI0Beta;
            const double val = 2.0 * cutoff * sincNorm(2.0 * cutoff * t) * win;
            row[idx] = val;
            rowSum += val;
        }
        if (rowSum != 0.0) {
            const double invSum = 1.0 / rowSum;
            for (int idx = 0; idx < numTaps; ++idx) {
                row[idx] *= invSum;
            }
        }
    }

    const size_t nIn = pcm[0].size();
    const size_t nOut = size_t(std::ceil(double(nIn) * double(up) / double(down)));

    for (int ch = 0; ch < 2; ++ch) {
        std::vector<double> pad(nIn + size_t(2 * half + 2), 0.0);
        std::copy(pcm[ch].begin(), pcm[ch].end(), pad.begin() + half);

        std::vector<double> out(nOut, 0.0);
        for (size_t n = 0; n < nOut; ++n) {
            const uint64_t pos = uint64_t(n) * uint64_t(down);
            const size_t base = size_t(pos / uint64_t(up));
            const int phase = int(pos % uint64_t(up));
            const double* coeff = &h[size_t(phase) * size_t(numTaps)];
            const double* src = &pad[base + 1];
            double acc = 0.0;
            for (int idx = 0; idx < numTaps; ++idx) {
                acc += src[idx] * coeff[idx];
            }
            out[n] = acc;
        }
        pcm[ch] = std::move(out);
    }
}

static void applyFadesAndPeakLimit(std::array<std::vector<double>, 2>& pcm,
                                   uint32_t rate,
                                   double fadeInSec,
                                   double fadeOutSec) {
    const size_t n = pcm[0].size();
    if (n == 0) return;

    const size_t fi = std::min(n, size_t(std::llround(fadeInSec * double(rate))));
    const size_t fo = std::min(n, size_t(std::llround(fadeOutSec * double(rate))));

    for (size_t i = 0; i < fi; ++i) {
        const double g = 0.5 - 0.5 * std::cos(M_PI * (double(i) + 0.5) / double(fi));
        pcm[0][i] *= g;
        pcm[1][i] *= g;
    }
    for (size_t i = 0; i < fo; ++i) {
        const double g = 0.5 + 0.5 * std::cos(M_PI * (double(i) + 0.5) / double(fo));
        pcm[0][n - fo + i] *= g;
        pcm[1][n - fo + i] *= g;
    }

    // Ensure sample peak <= -1.0 dBFS (0.89125) to avoid clipping on 16-bit ATRAC9 reconstruction
    const double ceiling = std::pow(10.0, -1.0 / 20.0);
    double maxPeak = 0.0;
    for (size_t i = 0; i < n; ++i) {
        maxPeak = std::max(maxPeak, std::max(std::abs(pcm[0][i]), std::abs(pcm[1][i])));
    }
    if (maxPeak > ceiling) {
        const double scale = ceiling / maxPeak;
        for (size_t i = 0; i < n; ++i) {
            pcm[0][i] *= scale;
            pcm[1][i] *= scale;
        }
    }
}

//------------------------------------------------------------------------------
// ATRAC9 Psychoacoustic & Bit Allocation Engine (port of ps5-at9-converter)
//------------------------------------------------------------------------------
struct GradientSpec {
    int mode;
    int startUnit;
    int endUnit;
    int startValue;
    int endValue;
    int headerBits;
    std::array<int, 31> curve;
};

enum class SfKind {
    Clc,
    Delta,
    Distance,
    Offset
};

struct SfCodingChoice {
    int bits = 1 << 30;
    int mode = 0;
    SfKind kind = SfKind::Clc;
    int length = 0;
    int weightIdx = 0;
    int base = 0;
    int count = 0;
    std::array<int, 31> baseline{};
};

struct ChannelTable {
    std::array<int, 31> sf{};
    std::array<int, 30> mask{};
    std::array<int, 30> cbSet{};
    int q[15][256]{};
    int bitLengths[15][30]{};
    double errors[15][30]{};
    double zeroError[30]{};
    SfCodingChoice sfCoding;
};

struct FrameCandidate {
    bool valid = false;
    double cost = std::numeric_limits<double>::infinity();
    int gradIdx = 0;
    int boundary = 0;
    int byteSize = 0;
};

class Atrac9StereoEncoder {
public:
    Atrac9StereoEncoder() {
        initMdctBasis();
        initBarkModel();
        initGradients();
    }

    std::vector<uint8_t> encodeStream(const std::array<std::vector<double>, 2>& pcmIn,
                                      int frameBytes,
                                      int maxBand) {
        const int superframeBytes = frameBytes * FRAMES_PER_SF;
        const size_t totalSamples = pcmIn[0].size();
        const size_t sfCount = (totalSamples + size_t(FRAME_SAMPLES) + size_t(SUPERFRAME_SAMPLES) - 1) / size_t(SUPERFRAME_SAMPLES);
        const size_t totalFrames = sfCount * size_t(FRAMES_PER_SF);
        const size_t paddedLen = (totalFrames + 1) * size_t(FRAME_SAMPLES);

        // Scale normalized [-1, 1] float PCM to 16-bit scale [-32768, 32768]
        // Lead-in of FRAME_SAMPLES (256) zeros at start matches ENCODER_DELAY = 256
        std::array<std::vector<double>, 2> padded;
        padded[0].assign(paddedLen, 0.0);
        padded[1].assign(paddedLen, 0.0);
        for (size_t i = 0; i < totalSamples; ++i) {
            padded[0][size_t(FRAME_SAMPLES) + i] = pcmIn[0][i] * 32768.0;
            padded[1][size_t(FRAME_SAMPLES) + i] = pcmIn[1][i] * 32768.0;
        }

        std::vector<uint8_t> output;
        output.reserve(sfCount * size_t(superframeBytes));

        for (size_t sfIdx = 0; sfIdx < sfCount; ++sfIdx) {
            double specs[FRAMES_PER_SF][2][FRAME_SAMPLES]{};
            double masks[FRAMES_PER_SF][2][30]{};

            for (int f = 0; f < FRAMES_PER_SF; ++f) {
                const size_t frameRow = sfIdx * size_t(FRAMES_PER_SF) + size_t(f);
                const size_t base = frameRow * size_t(FRAME_SAMPLES);
                runMdctBlock(&padded[0][base], &padded[1][base], specs[f]);
                for (int ch = 0; ch < 2; ++ch) {
                    computeMaskingThreshold(specs[f][ch], masks[f][ch]);
                }
            }

            encodeSuperframe(specs, masks, superframeBytes, maxBand, output);
        }

        return output;
    }

private:
    // basis_[n][k] for n in 0..511, k in 0..255
    double basis_[2 * FRAME_SAMPLES][FRAME_SAMPLES]{};

    std::array<double, MAX_UNITS> barkUnit_{};
    double barkSpread_[MAX_UNITS][MAX_UNITS]{};
    std::array<double, MAX_UNITS> athEnergy_{};

    std::vector<GradientSpec> gradients_;

    void initMdctBasis() {
        std::array<double, 2 * FRAME_SAMPLES> window{};
        for (int n = 0; n < FRAME_SAMPLES; ++n) {
            const double w = (std::sin(((double(n) + 0.5) / double(FRAME_SAMPLES) - 0.5) * M_PI) + 1.0) * 0.5;
            window[size_t(n)] = w;
            window[size_t(2 * FRAME_SAMPLES - 1 - n)] = w;
        }
        const double scale = 2.0 / double(FRAME_SAMPLES);
        for (int n = 0; n < 2 * FRAME_SAMPLES; ++n) {
            const double tn = double(n) + 0.5 + double(FRAME_SAMPLES) * 0.5;
            const double wn = window[size_t(n)] * scale;
            for (int k = 0; k < FRAME_SAMPLES; ++k) {
                const double tk = double(k) + 0.5;
                basis_[n][k] = std::cos(M_PI / double(FRAME_SAMPLES) * tk * tn) * wn;
            }
        }
    }

    void runMdctBlock(const double* blockL, const double* blockR, double outSpec[2][FRAME_SAMPLES]) const {
        const double* blocks[2] = { blockL, blockR };
        for (int ch = 0; ch < 2; ++ch) {
            const double* x = blocks[ch];
            double* y = outSpec[ch];
            std::fill(y, y + FRAME_SAMPLES, 0.0);
            for (int n = 0; n < 2 * FRAME_SAMPLES; ++n) {
                const double xn = x[n];
                if (xn == 0.0) continue;
                const double* bRow = basis_[n];
                for (int k = 0; k < FRAME_SAMPLES; ++k) {
                    y[k] += xn * bRow[k];
                }
            }
        }
    }

    static double athDb(double f) {
        const double k = std::max(f, 20.0) / 1000.0;
        return 3.64 * std::pow(k, -0.8) - 6.5 * std::exp(-0.6 * (k - 3.3) * (k - 3.3)) + 1e-3 * std::pow(k, 4.0);
    }

    void initBarkModel() {
        const double binHz = double(TARGET_SAMPLE_RATE) / 2.0 / double(FRAME_SAMPLES);
        for (int u = 0; u < MAX_UNITS; ++u) {
            const double lo = double(QuantUnitToCoeffIndex[u]) * binHz;
            const double hi = double(QuantUnitToCoeffIndex[u + 1]) * binHz;
            const double mid = 0.5 * (lo + hi);
            barkUnit_[size_t(u)] = 13.0 * std::atan(0.00076 * mid) +
                                   3.5 * std::atan((mid / 7500.0) * (mid / 7500.0));

            double minAth = 1e30;
            for (int s = 0; s < 9; ++s) {
                const double freq = lo + (hi - lo) * double(s) / 8.0;
                minAth = std::min(minAth, athDb(freq) - 110.0);
            }
            athEnergy_[size_t(u)] = (32768.0 * 32768.0) * std::pow(10.0, minAth / 10.0);
        }

        for (int i = 0; i < MAX_UNITS; ++i) {
            for (int j = 0; j < MAX_UNITS; ++j) {
                const double dz = barkUnit_[size_t(i)] - barkUnit_[size_t(j)];
                const double atten = (dz < 0.0) ? (27.0 * -dz) : (12.0 * dz);
                barkSpread_[i][j] = std::pow(10.0, -atten / 10.0);
            }
        }
    }

    void computeMaskingThreshold(const double spec[FRAME_SAMPLES], double outMask[30]) const {
        double energy[MAX_UNITS]{};
        double tonal[MAX_UNITS]{};
        for (int u = 0; u < MAX_UNITS; ++u) {
            const int start = QuantUnitToCoeffIndex[u];
            const int end = QuantUnitToCoeffIndex[u + 1];
            const int count = end - start;
            double sumSq = 0.0;
            double sumLog = 0.0;
            for (int k = start; k < end; ++k) {
                const double p = spec[k] * spec[k] + 1e-12;
                sumSq += spec[k] * spec[k];
                sumLog += std::log(p);
            }
            energy[u] = sumSq;
            const double gm = sumLog / double(count);
            const double am = std::log((sumSq / double(count)) + 1e-12);
            const double sfmDb = (10.0 / std::log(10.0)) * (gm - am);
            tonal[u] = std::clamp(sfmDb / -25.0, 0.0, 1.0);
        }

        for (int i = 0; i < MAX_UNITS; ++i) {
            double spreadSum = 0.0;
            for (int j = 0; j < MAX_UNITS; ++j) {
                spreadSum += energy[j] * barkSpread_[i][j];
            }
            const double offset = tonal[i] * std::min(14.5 + barkUnit_[size_t(i)], 25.0) +
                                  (1.0 - tonal[i]) * 5.5;
            const double masked = spreadSum * std::pow(10.0, -offset / 10.0);
            outMask[i] = std::max(masked, athEnergy_[size_t(i)]);
        }
    }

    static std::array<int, 31> buildGradientCurve(int mode, int startUnit, int endUnit, int startValue, int endValue) {
        Block blk{};
        blk.QuantizationUnitCount = 30;
        blk.GradientMode = mode;
        blk.GradientStartUnit = startUnit;
        blk.GradientEndUnit = endUnit;
        blk.GradientStartValue = startValue;
        blk.GradientEndValue = endValue;
        CreateGradient(&blk);
        std::array<int, 31> grad{};
        for (int i = 0; i < 31; ++i) {
            grad[size_t(i)] = blk.Gradient[i];
        }
        return grad;
    }

    void initGradients() {
        GenerateGradientCurves();
        // Sony hardware rules:
        //   1 <= startUnit < endUnit <= 31
        //   startValue != endValue (0..30 vs 31)
        auto addGrad = [&](int mode, int su, int sv) {
            GradientSpec g{};
            g.mode = mode;
            g.startUnit = su;
            g.endUnit = 31;
            g.startValue = sv;
            g.endValue = 31;
            g.headerBits = (mode == 0) ? (2 + 6 + 6 + 5 + 5 + 4) : (2 + 5 + 5 + 4);
            g.curve = buildGradientCurve(mode, su, 31, sv, 31);
            gradients_.push_back(g);
        };

        for (int m : { 1, 2, 3 }) {
            for (int su = 1; su <= 28; ++su) {
                for (int sv = 0; sv <= 26; ++sv) {
                    addGrad(m, su, sv);
                }
            }
        }
        for (int sv = 0; sv <= 30; ++sv) {
            addGrad(1, 1, sv);
            for (int su : { 8, 12, 16, 20, 24, 26 }) {
                addGrad(0, su, sv);
            }
        }
    }

    static std::array<int, 31> computeScaleFactors(const double spec[FRAME_SAMPLES], int unitCount) {
        std::array<int, 31> sf{};
        for (int u = 0; u < unitCount; ++u) {
            const int start = QuantUnitToCoeffIndex[u];
            const int end = QuantUnitToCoeffIndex[u + 1];
            double peak = 0.0;
            for (int k = start; k < end; ++k) {
                peak = std::max(peak, std::abs(spec[k]));
            }
            if (peak > 0.0) {
                const int s = int(std::ceil(std::log2(std::max(peak, 1e-300)))) + 15;
                sf[size_t(u)] = std::clamp(s, 0, 31);
            } else {
                sf[size_t(u)] = 0;
            }
        }
        return sf;
    }

    static std::array<int, 30> computePrecisionMask(const std::array<int, 31>& sf, int unitCount) {
        std::array<int, 30> mask{};
        for (int i = 1; i < unitCount; ++i) {
            const int delta = sf[size_t(i)] - sf[size_t(i - 1)];
            if (delta > 1) {
                mask[size_t(i)] += std::min(delta - 1, 5);
            } else if (delta < -1) {
                mask[size_t(i - 1)] += std::min(-delta - 1, 5);
            }
        }
        return mask;
    }

    static std::array<int, 30> computeCodebookSets(const std::array<int, 31>& sf, int codedUnits) {
        std::array<int, 30> sets{};
        if (codedUnits <= 1) return sets;

        int s[31]{};
        for (int i = 0; i < codedUnits; ++i) s[i] = sf[size_t(i)];
        s[codedUnits] = sf[size_t(codedUnits - 1)];

        int avg = 0;
        if (codedUnits > 12) {
            int sum12 = 0;
            for (int i = 0; i < 12; ++i) sum12 += s[i];
            avg = (sum12 + 6) / 12;
        }
        for (int i = 8; i < codedUnits; ++i) {
            const int lo = std::min(s[i - 1], s[i + 1]);
            if (s[i] - lo >= 3 || 2 * s[i] - s[i - 1] - s[i + 1] >= 3) {
                sets[size_t(i)] = 1;
            }
        }
        for (int i = 12; i < codedUnits; ++i) {
            if (!sets[size_t(i)]) {
                const int lo = std::min(s[i - 1], s[i + 1]);
                const int sub = (QuantUnitToCoeffCount[i] == 16) ? 1 : 0;
                if (s[i] - lo >= 2 && s[i] >= avg - sub) {
                    sets[size_t(i)] = 1;
                }
            }
        }
        return sets;
    }

    // Enforce Sony codebook minimum-magnitude rules for units 0..11
    static void fixMinimumCodebookRules(int q[15][256], const double xn[256]) {
        // Units 0..7 (coeffs 0..15): pairs at precisions p in {1, 2, 3} (word lengths 2..4)
        // must have max(|a|, |b|) >= 1 << (p - 1)
        for (int p : { 1, 2, 3 }) {
            const int need = 1 << (p - 1);
            int* row = q[p - 1];
            for (int u = 0; u < 8; ++u) {
                const int i0 = 2 * u;
                const int i1 = 2 * u + 1;
                if (std::max(std::abs(row[i0]), std::abs(row[i1])) < need) {
                    const int pick = (std::abs(xn[i1]) > std::abs(xn[i0])) ? i1 : i0;
                    row[pick] = (xn[pick] >= 0.0) ? need : -need;
                }
            }
        }

        // Units 8..11 (coeffs 16..31): 4-tuples at precision p == 1 (word length 2)
        // cannot be all zero
        int* row0 = q[0];
        for (int u = 0; u < 4; ++u) {
            const int base = 16 + 4 * u;
            if (row0[base] == 0 && row0[base + 1] == 0 && row0[base + 2] == 0 && row0[base + 3] == 0) {
                int bestJ = 0;
                double bestAbs = std::abs(xn[base]);
                for (int j = 1; j < 4; ++j) {
                    const double a = std::abs(xn[base + j]);
                    if (a > bestAbs) {
                        bestAbs = a;
                        bestJ = j;
                    }
                }
                row0[base + bestJ] = (xn[base + bestJ] >= 0.0) ? 1 : -1;
            }
        }
    }

    static SfCodingChoice sfClc(const std::array<int, 31>& sf, int units) {
        int lo = sf[0], hi = sf[0];
        for (int i = 1; i < units; ++i) {
            lo = std::min(lo, sf[size_t(i)]);
            hi = std::max(hi, sf[size_t(i)]);
        }
        SfCodingChoice c{};
        c.mode = 1;
        c.kind = SfKind::Clc;
        for (int length : { 2, 3, 4 }) {
            if (hi - lo < (1 << length)) {
                c.bits = 2 + 2 + 5 + units * length;
                c.length = length;
                c.base = lo;
                return c;
            }
        }
        c.bits = 2 + 2 + units * 5;
        c.length = 5;
        c.base = 0;
        return c;
    }

    static SfCodingChoice sfDeltaOffset(const std::array<int, 31>& sf, int units, int mode) {
        SfCodingChoice best{};
        best.mode = mode;
        best.kind = SfKind::Delta;

        for (int w = 0; w < 8; ++w) {
            const uint8_t* weights = ScaleFactorWeights[w];
            int t[31]{};
            int minT = 1000;
            for (int i = 0; i < units; ++i) {
                t[i] = sf[size_t(i)] + int(weights[i]);
                minT = std::min(minT, t[i]);
            }
            const int base = std::clamp(minT, 0, 31);
            int u[31]{};
            int minU = 1000, maxU = -1000;
            for (int i = 0; i < units; ++i) {
                u[i] = t[i] - base;
                minU = std::min(minU, u[i]);
                maxU = std::max(maxU, u[i]);
            }
            if (minU < 0) continue;

            for (int length : { 3, 4, 5, 6 }) {
                if (maxU >= (1 << length)) continue;
                const HuffmanCodebook* book = &HuffmanScaleFactorsUnsigned[length];
                const int mask = (1 << length) - 1;
                int bits = 2 + 3 + 5 + 2 + length;
                bool ok = true;
                for (int i = 1; i < units; ++i) {
                    const int delta = (u[i] - u[i - 1]) & mask;
                    const uint8_t b = book->Bits[delta];
                    if (b == 0) {
                        ok = false;
                        break;
                    }
                    bits += int(b);
                }
                if (ok && bits < best.bits) {
                    best.bits = bits;
                    best.weightIdx = w;
                    best.base = base;
                    best.length = length;
                }
            }
        }
        return best;
    }

    static SfCodingChoice sfDistance(const std::array<int, 31>& sf,
                                     int units,
                                     const std::array<int, 31>& baseline,
                                     int baselineLen,
                                     int mode) {
        SfCodingChoice best{};
        best.mode = mode;
        best.kind = SfKind::Distance;
        const int count = std::min(units, baselineLen);

        int lo = 0, hi = 0;
        for (int i = 0; i < count; ++i) {
            const int d = sf[size_t(i)] - baseline[size_t(i)];
            if (i == 0 || d < lo) lo = d;
            if (i == 0 || d > hi) hi = d;
        }

        for (int length : { 2, 3, 4, 5 }) {
            const int half = 1 << (length - 1);
            if (lo < -half || hi >= half) continue; // Strictly no wrap-around!
            const HuffmanCodebook* book = &HuffmanScaleFactorsSigned[length];
            const int mask = (1 << length) - 1;
            int bits = 2 + 2 + (units - count) * 5;
            bool ok = true;
            for (int i = 0; i < count; ++i) {
                const int sym = (sf[size_t(i)] - baseline[size_t(i)]) & mask;
                const uint8_t b = book->Bits[sym];
                if (b == 0) {
                    ok = false;
                    break;
                }
                bits += int(b);
            }
            if (ok && bits < best.bits) {
                best.bits = bits;
                best.length = length;
                best.baseline = baseline;
                best.count = count;
            }
        }
        return best;
    }

    static SfCodingChoice sfDeltaBaseline(const std::array<int, 31>& sf,
                                          int units,
                                          const std::array<int, 31>& baseline,
                                          int baselineLen,
                                          int mode) {
        SfCodingChoice best{};
        best.mode = mode;
        best.kind = SfKind::Offset;
        const int count = std::min(units, baselineLen);
        if (count < 1) return best;

        int r[31]{};
        int minR = 1000;
        for (int i = 0; i < count; ++i) {
            r[i] = sf[size_t(i)] - baseline[size_t(i)];
            minR = std::min(minR, r[i]);
        }
        const int base = std::clamp(minR, -16, 15);
        int u[31]{};
        int minU = 1000, maxU = -1000;
        for (int i = 0; i < count; ++i) {
            u[i] = r[i] - base;
            minU = std::min(minU, u[i]);
            maxU = std::max(maxU, u[i]);
        }
        if (minU < 0) return best;

        for (int length : { 1, 2, 3, 4 }) {
            if (maxU >= (1 << length)) continue;
            const HuffmanCodebook* book = &HuffmanScaleFactorsUnsigned[length];
            const int mask = (1 << length) - 1;
            int bits = 2 + 5 + 2 + length + (units - count) * 5;
            bool ok = true;
            for (int i = 1; i < count; ++i) {
                const int delta = (u[i] - u[i - 1]) & mask;
                const uint8_t b = book->Bits[delta];
                if (b == 0) {
                    ok = false;
                    break;
                }
                bits += int(b);
            }
            if (ok && bits < best.bits) {
                best.bits = bits;
                best.length = length;
                best.base = base;
                best.baseline = baseline;
                best.count = count;
            }
        }
        return best;
    }

    static SfCodingChoice chooseSfCoding(const std::array<int, 31>& sf,
                                         int units,
                                         int ch,
                                         bool firstInSuperframe,
                                         const std::array<int, 31>& sfPrev,
                                         int prevUnits,
                                         const std::array<int, 31>& sfCh0) {
        std::vector<SfCodingChoice> opts;
        if (ch == 0) {
            opts.push_back(sfDeltaOffset(sf, units, 0));
            opts.push_back(sfClc(sf, units));
            if (!firstInSuperframe) {
                opts.push_back(sfDistance(sf, units, sfPrev, prevUnits, 2));
                opts.push_back(sfDeltaBaseline(sf, units, sfPrev, prevUnits, 3));
            }
        } else {
            opts.push_back(sfDeltaOffset(sf, units, 0));
            opts.push_back(sfDistance(sf, units, sfCh0, units, 1));
            opts.push_back(sfDeltaBaseline(sf, units, sfCh0, units, 2));
            if (!firstInSuperframe) {
                opts.push_back(sfDistance(sf, units, sfPrev, prevUnits, 3));
            }
        }
        auto it = std::min_element(opts.begin(), opts.end(),
            [](const SfCodingChoice& a, const SfCodingChoice& b) {
                return a.bits < b.bits;
            });
        return *it;
    }

    static void writeScaleFactors(BitWriter& bw,
                                  const std::array<int, 31>& sf,
                                  int units,
                                  const SfCodingChoice& sc) {
        bw.write(uint32_t(sc.mode), 2);
        if (sc.kind == SfKind::Clc) {
            bw.write(uint32_t(sc.length - 2), 2);
            if (sc.length < 5) {
                bw.write(uint32_t(sc.base), 5);
            }
            for (int i = 0; i < units; ++i) {
                bw.write(uint32_t(sf[size_t(i)] - sc.base), sc.length);
            }
        } else if (sc.kind == SfKind::Delta) {
            const uint8_t* weights = ScaleFactorWeights[sc.weightIdx];
            int u[31]{};
            for (int i = 0; i < units; ++i) {
                u[i] = sf[size_t(i)] + int(weights[i]) - sc.base;
            }
            bw.write(uint32_t(sc.weightIdx), 3);
            bw.write(uint32_t(sc.base), 5);
            bw.write(uint32_t(sc.length - 3), 2);
            bw.write(uint32_t(u[0]), sc.length);
            const HuffmanCodebook* book = &HuffmanScaleFactorsUnsigned[sc.length];
            const int mask = (1 << sc.length) - 1;
            for (int i = 1; i < units; ++i) {
                const int delta = (u[i] - u[i - 1]) & mask;
                bw.write(book->Codes[delta], book->Bits[delta]);
            }
        } else if (sc.kind == SfKind::Distance) {
            bw.write(uint32_t(sc.length - 2), 2);
            const HuffmanCodebook* book = &HuffmanScaleFactorsSigned[sc.length];
            const int mask = (1 << sc.length) - 1;
            for (int i = 0; i < sc.count; ++i) {
                const int sym = (sf[size_t(i)] - sc.baseline[size_t(i)]) & mask;
                bw.write(book->Codes[sym], book->Bits[sym]);
            }
            for (int i = sc.count; i < units; ++i) {
                bw.write(uint32_t(sf[size_t(i)]), 5);
            }
        } else { // SfKind::Offset
            bw.write(uint32_t(sc.base + 16), 5);
            bw.write(uint32_t(sc.length - 1), 2);
            int u[31]{};
            for (int i = 0; i < sc.count; ++i) {
                u[i] = sf[size_t(i)] - sc.baseline[size_t(i)] - sc.base;
            }
            bw.write(uint32_t(u[0]), sc.length);
            const HuffmanCodebook* book = &HuffmanScaleFactorsUnsigned[sc.length];
            const int mask = (1 << sc.length) - 1;
            for (int i = 1; i < sc.count; ++i) {
                const int delta = (u[i] - u[i - 1]) & mask;
                bw.write(book->Codes[delta], book->Bits[delta]);
            }
            for (int i = sc.count; i < units; ++i) {
                bw.write(uint32_t(sf[size_t(i)]), 5);
            }
        }
    }

    static ChannelTable buildChannelTable(const double spec[FRAME_SAMPLES],
                                          const double maskWeight[30],
                                          int unitCount,
                                          int ch,
                                          bool firstInSuperframe,
                                          const std::array<int, 31>& sfPrev,
                                          int prevUnits,
                                          const std::array<int, 31>& sfCh0) {
        ChannelTable tbl{};
        tbl.sf = computeScaleFactors(spec, unitCount);
        tbl.mask = computePrecisionMask(tbl.sf, unitCount);
        tbl.cbSet = computeCodebookSets(tbl.sf, unitCount);
        tbl.sfCoding = chooseSfCoding(tbl.sf, unitCount, ch, firstInSuperframe, sfPrev, prevUnits, sfCh0);

        const int coeffEnd = QuantUnitToCoeffIndex[unitCount];
        double xn[FRAME_SAMPLES]{};
        for (int u = 0; u < unitCount; ++u) {
            const int start = QuantUnitToCoeffIndex[u];
            const int end = QuantUnitToCoeffIndex[u + 1];
            const double invScale = 1.0 / SpectrumScale[tbl.sf[size_t(u)]];
            for (int k = start; k < end; ++k) {
                xn[k] = spec[k] * invScale;
            }
        }

        for (int p = 1; p <= 15; ++p) {
            const double invStep = 1.0 / QuantizerStepSize[p];
            const int maxQ = (1 << p) - 1;
            int* qRow = tbl.q[p - 1];
            for (int k = 0; k < coeffEnd; ++k) {
                const int v = int(std::nearbyint(xn[k] * invStep));
                qRow[k] = std::clamp(v, -maxQ, maxQ);
            }
        }

        fixMinimumCodebookRules(tbl.q, xn);

        for (int u = 0; u < MAX_UNITS; ++u) {
            const int start = QuantUnitToCoeffIndex[u];
            const int end = QuantUnitToCoeffIndex[u + 1];
            double zErr = 0.0;
            for (int k = start; k < end; ++k) {
                zErr += spec[k] * spec[k];
            }
            tbl.zeroError[u] = zErr / maskWeight[u];
        }

        for (int u = 0; u < unitCount; ++u) {
            const int start = QuantUnitToCoeffIndex[u];
            const int end = QuantUnitToCoeffIndex[u + 1];
            const int count = end - start;
            const int cbIdx = QuantUnitToCodebookIndex[u];
            const int cbSet = tbl.cbSet[size_t(u)];
            const double scale = SpectrumScale[tbl.sf[size_t(u)]];
            const double invMask = 1.0 / maskWeight[u];

            // p = 1..6 -> word lengths wl = 2..7 (Huffman spectrum codebooks)
            for (int p = 1; p <= 6; ++p) {
                const int wl = p + 1;
                const HuffmanCodebook* book = &HuffmanSpectrum[cbSet][wl][cbIdx];
                const int group = 1 << book->ValueCountPower;
                const int bits = book->ValueBits;
                const int mask = (1 << bits) - 1;

                int totalBits = 0;
                bool valid = true;
                for (int k = start; k < end; k += group) {
                    uint32_t sym = 0;
                    for (int j = 0; j < group; ++j) {
                        sym |= uint32_t(tbl.q[p - 1][k + j] & mask) << (j * bits);
                    }
                    const uint8_t b = book->Bits[sym];
                    if (b == 0) {
                        valid = false;
                        break;
                    }
                    totalBits += int(b);
                }

                if (!valid) {
                    tbl.bitLengths[p - 1][u] = 100000;
                    tbl.errors[p - 1][u] = 1e30;
                } else {
                    tbl.bitLengths[p - 1][u] = totalBits;
                    const double stepScale = QuantizerStepSize[p] * scale;
                    double err = 0.0;
                    for (int k = start; k < end; ++k) {
                        const double dq = double(tbl.q[p - 1][k]) * stepScale;
                        const double diff = spec[k] - dq;
                        err += diff * diff;
                    }
                    tbl.errors[p - 1][u] = err * invMask;
                }
            }

            // p = 7..15 -> word lengths wl = 8..16 (raw signed wl-bit two's-complement integers)
            for (int p = 7; p <= 15; ++p) {
                const int wl = p + 1;
                tbl.bitLengths[p - 1][u] = wl * count;
                const double stepScale = QuantizerStepSize[p] * scale;
                double err = 0.0;
                for (int k = start; k < end; ++k) {
                    const double dq = double(tbl.q[p - 1][k]) * stepScale;
                    const double diff = spec[k] - dq;
                    err += diff * diff;
                }
                tbl.errors[p - 1][u] = err * invMask;
            }
        }

        return tbl;
    }

    static void computeCoarsePrecisions(const ChannelTable& chTbl,
                                        const GradientSpec& grad,
                                        int boundary,
                                        int unitCount,
                                        int outPrec[30],
                                        bool& outValid) {
        outValid = true;
        for (int u = 0; u < unitCount; ++u) {
            int p = 0;
            if (grad.mode != 0) {
                p = chTbl.sf[size_t(u)] + chTbl.mask[size_t(u)] - grad.curve[size_t(u)];
                if (p > 0) {
                    if (grad.mode == 1) p = p / 2;
                    else if (grad.mode == 2) p = (3 * p) / 8;
                    else p = p / 4;
                }
            } else {
                p = chTbl.sf[size_t(u)] - grad.curve[size_t(u)];
            }
            if (p < 1) p = 1;
            if (u < boundary) p += 1;
            if (p > 15) {
                outValid = false; // Never emit PrecisionsFine > 0 on PS5
                return;
            }
            outPrec[u] = p;
        }
    }

    std::vector<FrameCandidate> buildFrameCandidates(const ChannelTable chTbl[2],
                                                     int unitCount,
                                                     bool firstInSuperframe,
                                                     int maxFrameBytes) const {
        const int maxBoundary = std::min(15, unitCount);
        // Fixed frame bits (excluding gradient header, scale factors, and spectrum):
        //   first (1) + reuse (1) + [if first: band(4) + stereoBand(4) + bex(1)] + primary(1) + jointSigns(1) + hasExt(1)
        const int fixedBits = 2 + (firstInSuperframe ? 9 : 0) + 2 + 1 +
                              chTbl[0].sfCoding.bits + chTbl[1].sfCoding.bits;

        std::vector<FrameCandidate> byBytes(size_t(maxFrameBytes + 1));

        for (size_t gIdx = 0; gIdx < gradients_.size(); ++gIdx) {
            const GradientSpec& g = gradients_[gIdx];
            if (g.startUnit >= g.endUnit || g.endUnit > 31) continue;

            for (int boundary = 0; boundary <= maxBoundary; ++boundary) {
                int prec[2][30]{};
                bool valid0 = false, valid1 = false;
                computeCoarsePrecisions(chTbl[0], g, boundary, unitCount, prec[0], valid0);
                if (!valid0) continue;
                computeCoarsePrecisions(chTbl[1], g, boundary, unitCount, prec[1], valid1);
                if (!valid1) continue;

                int specBits = 0;
                double cost = 0.0;
                bool ok = true;
                for (int c = 0; c < 2; ++c) {
                    for (int u = 0; u < unitCount; ++u) {
                        const int pIdx = prec[c][u] - 1;
                        const int b = chTbl[c].bitLengths[pIdx][u];
                        if (b >= 100000) {
                            ok = false;
                            break;
                        }
                        specBits += b;
                        cost += chTbl[c].errors[pIdx][u];
                    }
                    if (!ok) break;
                }
                if (!ok) continue;

                const int totalBits = fixedBits + g.headerBits + specBits;
                const int byteSize = (totalBits + 7) >> 3;
                if (byteSize >= 1 && byteSize <= maxFrameBytes) {
                    if (cost < byBytes[size_t(byteSize)].cost) {
                        FrameCandidate fc{};
                        fc.valid = true;
                        fc.cost = cost;
                        fc.gradIdx = int(gIdx);
                        fc.boundary = boundary;
                        fc.byteSize = byteSize;
                        byBytes[size_t(byteSize)] = fc;
                    }
                }
            }
        }
        return byBytes;
    }

    void writeFrame(BitWriter& bw,
                    const ChannelTable chTbl[2],
                    int band,
                    bool firstInSuperframe,
                    int gradIdx,
                    int boundary) const {
        const int unitCount = BandToQuantUnitCount[band];
        const GradientSpec& g = gradients_[size_t(gradIdx)];

        bw.write(firstInSuperframe ? 0u : 1u, 1); // !FirstInSuperframe
        bw.write(firstInSuperframe ? 0u : 1u, 1); // ReuseBandParams
        if (firstInSuperframe) {
            bw.write(uint32_t(band - MIN_BAND), 4);
            bw.write(uint32_t(band - MIN_BAND), 4); // stereoBand == band
            bw.write(0u, 1);                        // bandExtensionEnabled == 0
        }

        bw.write(uint32_t(g.mode), 2);
        if (g.mode != 0) {
            bw.write(uint32_t(g.startUnit), 5);
            bw.write(uint32_t(g.startValue), 5);
        } else {
            bw.write(uint32_t(g.startUnit), 6);
            bw.write(uint32_t(g.endUnit - 1), 6);
            bw.write(uint32_t(g.startValue), 5);
            bw.write(uint32_t(g.endValue), 5);
        }
        bw.write(uint32_t(boundary), 4);

        bw.write(0u, 1); // PrimaryChannelIndex == 0
        bw.write(0u, 1); // HasJointStereoSigns == 0
        bw.write(0u, 1); // HasExtensionData == 0

        for (int c = 0; c < 2; ++c) {
            int prec[30]{};
            bool valid = false;
            computeCoarsePrecisions(chTbl[c], g, boundary, unitCount, prec, valid);

            writeScaleFactors(bw, chTbl[c].sf, unitCount, chTbl[c].sfCoding);

            for (int u = 0; u < unitCount; ++u) {
                const int p = prec[u];
                const int wl = p + 1;
                const int start = QuantUnitToCoeffIndex[u];
                const int end = QuantUnitToCoeffIndex[u + 1];
                const int* vals = &chTbl[c].q[p - 1][start];
                const int count = end - start;

                if (wl <= 7) {
                    const HuffmanCodebook* book =
                        &HuffmanSpectrum[chTbl[c].cbSet[size_t(u)]][wl][QuantUnitToCodebookIndex[u]];
                    const int group = 1 << book->ValueCountPower;
                    const int bits = book->ValueBits;
                    const int mask = (1 << bits) - 1;

                    for (int k = 0; k < count; k += group) {
                        uint32_t sym = 0;
                        for (int j = 0; j < group; ++j) {
                            sym |= uint32_t(vals[k + j] & mask) << (j * bits);
                        }
                        bw.write(book->Codes[sym], book->Bits[sym]);
                    }
                } else {
                    const uint32_t mask = (1u << wl) - 1u;
                    for (int k = 0; k < count; ++k) {
                        bw.write(uint32_t(vals[k]) & mask, wl);
                    }
                }
            }
        }

        bw.align();
    }

    static int selectActiveBand(const double specs[FRAMES_PER_SF][2][FRAME_SAMPLES],
                                const double masks[FRAMES_PER_SF][2][30],
                                int maxBand) {
        int topUnit = 0;
        for (int u = 0; u < MAX_UNITS; ++u) {
            const int s = QuantUnitToCoeffIndex[u];
            const int e = QuantUnitToCoeffIndex[u + 1];
            bool audible = false;
            for (int f = 0; f < FRAMES_PER_SF && !audible; ++f) {
                for (int c = 0; c < 2 && !audible; ++c) {
                    double energy = 0.0;
                    for (int k = s; k < e; ++k) {
                        energy += specs[f][c][k] * specs[f][c][k];
                    }
                    if (energy > masks[f][c][u]) {
                        audible = true;
                    }
                }
            }
            if (audible) {
                topUnit = u + 1;
            }
        }

        int needBand = 3;
        for (int b = 3; b <= maxBand; ++b) {
            if (BandToQuantUnitCount[b] >= topUnit) {
                needBand = b;
                break;
            }
            needBand = b;
        }
        return std::clamp(needBand, 3, maxBand);
    }

    void encodeSuperframe(const double specs[FRAMES_PER_SF][2][FRAME_SAMPLES],
                          const double masks[FRAMES_PER_SF][2][30],
                          int superframeBytes,
                          int maxBand,
                          std::vector<uint8_t>& outStream) const {
        int band = selectActiveBand(specs, masks, maxBand);

        while (band >= MIN_BAND) {
            const int unitCount = BandToQuantUnitCount[band];
            ChannelTable tables[FRAMES_PER_SF][2]{};
            std::vector<FrameCandidate> cands[FRAMES_PER_SF];
            std::array<int, 31> sfPrev[2]{};

            bool possible = true;
            for (int f = 0; f < FRAMES_PER_SF; ++f) {
                const bool first = (f == 0);
                tables[f][0] = buildChannelTable(specs[f][0], masks[f][0], unitCount, 0, first, sfPrev[0], unitCount, tables[f][0].sf);
                tables[f][1] = buildChannelTable(specs[f][1], masks[f][1], unitCount, 1, first, sfPrev[1], unitCount, tables[f][0].sf);
                sfPrev[0] = tables[f][0].sf;
                sfPrev[1] = tables[f][1].sf;

                cands[f] = buildFrameCandidates(tables[f], unitCount, first, superframeBytes);
                bool hasValid = false;
                for (const auto& fc : cands[f]) {
                    if (fc.valid) {
                        hasValid = true;
                        break;
                    }
                }
                if (!hasValid) {
                    possible = false;
                    break;
                }
            }

            if (possible) {
                const double INF = std::numeric_limits<double>::infinity();
                std::vector<double> dp(size_t(superframeBytes + 1), INF);
                int back[FRAMES_PER_SF][513]{};
                dp[0] = 0.0;

                for (int f = 0; f < FRAMES_PER_SF; ++f) {
                    std::vector<double> nextDp(size_t(superframeBytes + 1), INF);
                    for (int b = 1; b <= superframeBytes; ++b) {
                        if (!cands[f][size_t(b)].valid) continue;
                        const double cCost = cands[f][size_t(b)].cost;
                        for (int prevB = 0; prevB + b <= superframeBytes; ++prevB) {
                            if (dp[size_t(prevB)] == INF) continue;
                            const double v = dp[size_t(prevB)] + cCost;
                            if (v < nextDp[size_t(prevB + b)]) {
                                nextDp[size_t(prevB + b)] = v;
                                back[f][prevB + b] = b;
                            }
                        }
                    }
                    dp = std::move(nextDp);
                }

                int bestTotalB = -1;
                double minCost = INF;
                for (int b = 0; b <= superframeBytes; ++b) {
                    if (dp[size_t(b)] < minCost) {
                        minCost = dp[size_t(b)];
                        bestTotalB = b;
                    }
                }

                if (bestTotalB >= 0 && std::isfinite(minCost)) {
                    FrameCandidate picked[FRAMES_PER_SF]{};
                    int curB = bestTotalB;
                    for (int f = FRAMES_PER_SF - 1; f >= 0; --f) {
                        const int stepB = back[f][curB];
                        picked[f] = cands[f][size_t(stepB)];
                        curB -= stepB;
                    }

                    const size_t sfStart = outStream.size();
                    for (int f = 0; f < FRAMES_PER_SF; ++f) {
                        BitWriter bw;
                        writeFrame(bw, tables[f], band, f == 0, picked[f].gradIdx, picked[f].boundary);
                        outStream.insert(outStream.end(), bw.bytes().begin(), bw.bytes().end());
                    }

                    const size_t written = outStream.size() - sfStart;
                    if (written < size_t(superframeBytes)) {
                        // CRITICAL for Sony PS5 hardware ATRAC9 decoder:
                        // All unused bytes after Frame 3 in a superframe MUST be 0x01 (NOT 0x00)!
                        outStream.insert(outStream.end(), size_t(superframeBytes) - written, 0x01u);
                    }
                    return;
                }
            }

            --band;
        }
    }
};

//------------------------------------------------------------------------------
// RIFF/WAVE (.at9) Container Writer
//------------------------------------------------------------------------------
static void buildConfigData(int frameBytes, uint8_t outCfg[4]) {
    const uint32_t word =
        (0xFEu << 24) |
        (SAMPLE_RATE_INDEX << 20) |
        (CHANNEL_CONFIG_IDX << 17) |
        (0u << 16) |
        (uint32_t(frameBytes - 1) << 5) |
        (SUPERFRAME_INDEX << 3);
    outCfg[0] = uint8_t((word >> 24) & 0xFFu);
    outCfg[1] = uint8_t((word >> 16) & 0xFFu);
    outCfg[2] = uint8_t((word >> 8) & 0xFFu);
    outCfg[3] = uint8_t(word & 0xFFu);
}

static bool writeAt9File(const char* outPath,
                         const std::vector<uint8_t>& superframes,
                         uint32_t totalSamples,
                         int frameBytes) {
    const uint32_t superframeBytes = uint32_t(frameBytes * FRAMES_PER_SF);
    const uint32_t avgBytesPerSec =
        (TARGET_SAMPLE_RATE * superframeBytes + SUPERFRAME_SAMPLES / 2) / SUPERFRAME_SAMPLES;

    uint8_t configData[4]{};
    buildConfigData(frameBytes, configData);

    static const uint8_t atrac9Guid[16] = {
        0xd2, 0x42, 0xe1, 0x47, 0xba, 0x36, 0x8d, 0x4d,
        0x88, 0xfc, 0x61, 0x65, 0x4f, 0x8c, 0x83, 0x6c
    };

    std::vector<uint8_t> chunks;

    // 1. "fmt " chunk (52 bytes)
    writeBytes(chunks, "fmt ", 4);
    writeU32LE(chunks, 52);
    writeU16LE(chunks, 0xFFFE);                       // WAVE_FORMAT_EXTENSIBLE
    writeU16LE(chunks, 2);                            // nChannels = 2
    writeU32LE(chunks, TARGET_SAMPLE_RATE);           // nSamplesPerSec = 48000
    writeU32LE(chunks, avgBytesPerSec);               // nAvgBytesPerSec
    writeU16LE(chunks, uint16_t(superframeBytes));    // nBlockAlign = superframeBytes
    writeU16LE(chunks, 0);                            // wBitsPerSample = 0
    writeU16LE(chunks, 34);                           // cbSize = 34 (0x22)
    writeU16LE(chunks, uint16_t(SUPERFRAME_SAMPLES)); // samplesPerBlock = 1024
    writeU32LE(chunks, 0x00000003u);                  // dwChannelMask = SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT
    writeBytes(chunks, atrac9Guid, 16);               // SubFormat = KSDATAFORMAT_SUBTYPE_ATRAC9
    writeU32LE(chunks, 1);                            // versionInfo = 1
    writeBytes(chunks, configData, 4);                // configData
    writeU32LE(chunks, 0);                            // reserved = 0

    // 2. "fact" chunk (12 bytes)
    writeBytes(chunks, "fact", 4);
    writeU32LE(chunks, 12);
    writeU32LE(chunks, totalSamples);
    writeU32LE(chunks, ENCODER_DELAY);                // input & overlap delay = 256
    writeU32LE(chunks, ENCODER_DELAY);                // encoder delay = 256

    // 3. "smpl" chunk (60 bytes, looped)
    const uint32_t loopStart = ENCODER_DELAY;
    const uint32_t loopEnd = ENCODER_DELAY + totalSamples - 1;
    writeBytes(chunks, "smpl", 4);
    writeU32LE(chunks, 60);
    writeU32LE(chunks, 0);                            // manufacturer
    writeU32LE(chunks, 0);                            // product
    writeU32LE(chunks, uint32_t(1000000000ull / TARGET_SAMPLE_RATE)); // samplePeriod = 20833 ns
    writeU32LE(chunks, 60);                           // midiUnityNote
    writeU32LE(chunks, 0);                            // midiPitchFraction
    writeU32LE(chunks, 0);                            // smpteFormat
    writeU32LE(chunks, 0);                            // smpteOffset
    writeU32LE(chunks, 1);                            // numSampleLoops = 1
    writeU32LE(chunks, 0);                            // samplerData = 0
    writeU32LE(chunks, 0);                            // cuePointId
    writeU32LE(chunks, 0);                            // type = 0 (loop forward)
    writeU32LE(chunks, loopStart);                    // start
    writeU32LE(chunks, loopEnd);                      // end
    writeU32LE(chunks, 0);                            // fraction
    writeU32LE(chunks, 0);                            // playCount = 0 (infinite)

    // 4. "data" chunk
    writeBytes(chunks, "data", 4);
    writeU32LE(chunks, uint32_t(superframes.size()));
    writeBytes(chunks, superframes.data(), superframes.size());

    std::vector<uint8_t> riff;
    riff.reserve(12 + chunks.size());
    writeBytes(riff, "RIFF", 4);
    writeU32LE(riff, uint32_t(4 + chunks.size()));
    writeBytes(riff, "WAVE", 4);
    writeBytes(riff, chunks.data(), chunks.size());

    FILE* out = std::fopen(outPath, "wb");
    if (!out) return false;
    const size_t written = std::fwrite(riff.data(), 1, riff.size(), out);
    std::fclose(out);
    return written == riff.size();
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 3) {
        std::fprintf(stderr, "Usage: %s <input.(wav|mp3|ogg|flac)> <output.at9> [max_seconds]\n", argv[0]);
        return 1;
    }

    const char* inPath = argv[1];
    const char* outPath = argv[2];
    double maxSeconds = 174.7; // Max duration at 96 kbps that fits in 2 MiB
    if (argc >= 4) {
        const double v = std::atof(argv[3]);
        if (v > 0.0) maxSeconds = v;
    }

    std::vector<std::vector<double>> rawChannels;
    uint32_t inSampleRate = 0;
    if (!loadAudioFile(inPath, rawChannels, inSampleRate) || rawChannels.empty() || rawChannels[0].empty()) {
        std::fprintf(stderr, "Error: Failed to decode audio file '%s'\n", inPath);
        return 1;
    }

    // 1. Map to stereo and resample to 48000 Hz using Kaiser-windowed sinc filter
    std::array<std::vector<double>, 2> stereo = toStereo(rawChannels);
    rawChannels.clear();
    rawChannels.shrink_to_fit();

    resampleStereo(stereo, inSampleRate, TARGET_SAMPLE_RATE);

    // 2. Trim to requested max duration and maximum 2 MiB capacity
    constexpr uint32_t headerOverhead = 168;
    const uint32_t maxPayloadBytes = MAX_AT9_FILE_BYTES - headerOverhead;
    const size_t maxSamples96k =
        (size_t(maxPayloadBytes / 256) * size_t(SUPERFRAME_SAMPLES)) - size_t(FRAME_SAMPLES);
    const size_t userMaxSamples = size_t(maxSeconds * double(TARGET_SAMPLE_RATE));
    const size_t maxAllowedSamples = std::min(userMaxSamples, maxSamples96k);

    bool wasTrimmed = false;
    if (stereo[0].size() > maxAllowedSamples) {
        stereo[0].resize(maxAllowedSamples);
        stereo[1].resize(maxAllowedSamples);
        wasTrimmed = true;
    }

    const size_t totalSamples = stereo[0].size();
    if (totalSamples == 0) {
        std::fprintf(stderr, "Error: Input audio has 0 samples\n");
        return 1;
    }

    // 3. Auto-select highest bitrate (192, 168, 144, or 96 kbps) that fits in <= 2 MiB
    struct BitrateOption { int kbps; int frameBytes; int maxBand; };
    constexpr BitrateOption kBitrates[] = {
        { 192, 128, 16 },
        { 168, 112, 16 },
        { 144,  96, 15 },
        {  96,  64, 13 },
    };
    int chosenKbps = 96;
    int chosenFrameBytes = 64;
    int chosenMaxBand = 13;
    for (const auto& opt : kBitrates) {
        const size_t sfBytes = size_t(opt.frameBytes * FRAMES_PER_SF);
        const size_t sfCount =
            (totalSamples + size_t(FRAME_SAMPLES) + size_t(SUPERFRAME_SAMPLES) - 1) / size_t(SUPERFRAME_SAMPLES);
        if (sfCount * sfBytes <= size_t(maxPayloadBytes)) {
            chosenKbps = opt.kbps;
            chosenFrameBytes = opt.frameBytes;
            chosenMaxBand = opt.maxBand;
            break;
        }
    }

    // 4. Smooth cosine fade-in/out and -1.0 dBFS peak ceiling
    const double fadeInSec = 0.02;
    const double fadeOutSec = wasTrimmed ? 0.50 : 0.02;
    applyFadesAndPeakLimit(stereo, TARGET_SAMPLE_RATE, fadeInSec, fadeOutSec);

    // 5. Encode Sony-compliant ATRAC9 bitstream
    Atrac9StereoEncoder encoder;
    const std::vector<uint8_t> superframes = encoder.encodeStream(stereo, chosenFrameBytes, chosenMaxBand);

    if (!writeAt9File(outPath, superframes, uint32_t(totalSamples), chosenFrameBytes)) {
        std::fprintf(stderr, "Error: Failed to write output AT9 file '%s'\n", outPath);
        return 1;
    }

    const size_t numSf = superframes.size() / size_t(chosenFrameBytes * FRAMES_PER_SF);
    std::printf("Encoded %s -> %s (%u samples, %.2f s @ 48000 Hz stereo, %d kbps, %zu superframes, %zu bytes)\n",
                inPath, outPath, uint32_t(totalSamples),
                double(totalSamples) / double(TARGET_SAMPLE_RATE),
                chosenKbps, numSf, size_t(headerOverhead) + superframes.size());
    return 0;
}
