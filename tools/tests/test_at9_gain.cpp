#define main wav_to_at9_converter_main
#include "../wav_to_at9/wav_to_at9.cpp"
#undef main

#include <cmath>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <memory>

extern "C" {
#include "../../thirdparty/libatrac9/src/libatrac9.h"
#include "../../thirdparty/libatrac9/src/structures.h"
}

static bool loadAt9FromMemory(const uint8_t* data, size_t size,
                              std::vector<std::vector<double>>& outChannels,
                              uint32_t& outSampleRate) {
    static constexpr uint8_t atrac9Guid[16] = {
        0xd2, 0x42, 0xe1, 0x47, 0xba, 0x36, 0x8d, 0x4d,
        0x88, 0xfc, 0x61, 0x65, 0x4f, 0x8c, 0x83, 0x6c
    };

    if (size < 12 || std::memcmp(data, "RIFF", 4) != 0 || std::memcmp(data + 8, "WAVE", 4) != 0)
        return false;

    const size_t riffEnd = size_t(readU32LE(data + 4)) + 8;
    if (riffEnd > size || riffEnd < 12)
        return false;

    const uint8_t* format = nullptr;
    size_t formatSize = 0;
    const uint8_t* fact = nullptr;
    size_t factSize = 0;
    const uint8_t* sampler = nullptr;
    size_t samplerSize = 0;
    const uint8_t* encoded = nullptr;
    size_t encodedSize = 0;

    for (size_t offset = 12; offset + 8 <= riffEnd;) {
        const uint8_t* chunkHeader = data + offset;
        const uint32_t chunkSize = readU32LE(chunkHeader + 4);
        const size_t payloadOffset = offset + 8;
        if (size_t(chunkSize) > riffEnd - payloadOffset)
            return false;

        const uint8_t* payload = data + payloadOffset;
        if (std::memcmp(chunkHeader, "fmt ", 4) == 0) {
            format = payload;
            formatSize = chunkSize;
        } else if (std::memcmp(chunkHeader, "fact", 4) == 0) {
            fact = payload;
            factSize = chunkSize;
        } else if (std::memcmp(chunkHeader, "smpl", 4) == 0) {
            sampler = payload;
            samplerSize = chunkSize;
        } else if (std::memcmp(chunkHeader, "data", 4) == 0) {
            encoded = payload;
            encodedSize = chunkSize;
        }

        const size_t nextOffset = payloadOffset + size_t(chunkSize) + (chunkSize & 1u);
        if (nextOffset > riffEnd)
            return false;
        offset = nextOffset;
    }

    // Ordinary PCM WAVE files also reach this probe; leave them for loadWavFromMemory.
    if (!format || formatSize < 48 || readU16LE(format) != 0xFFFE ||
        std::memcmp(format + 24, atrac9Guid, sizeof(atrac9Guid)) != 0)
        return false;

    if (!fact || factSize < 4 || !encoded || encodedSize == 0)
        return false;

    const uint16_t headerChannels = readU16LE(format + 2);
    const uint32_t headerSampleRate = readU32LE(format + 4);
    const uint16_t headerBlockAlign = readU16LE(format + 12);
    const uint32_t audibleSampleCount = readU32LE(fact);
    if (headerChannels == 0 || headerChannels > MAX_CHANNEL_COUNT || headerSampleRate == 0 || audibleSampleCount == 0)
        return false;

    uint32_t loopStart = 0;
    if (sampler && samplerSize >= 60 && readU32LE(sampler + 28) > 0)
        loopStart = readU32LE(sampler + 44);
    else if (factSize >= 8)
        loopStart = readU32LE(fact + 4);

    std::unique_ptr<void, decltype(&Atrac9ReleaseHandle)> decoder(
        Atrac9GetHandle(), &Atrac9ReleaseHandle);
    if (!decoder)
        return false;

    const int initResult = Atrac9InitDecoder(decoder.get(), const_cast<uint8_t*>(format + 44));
    if (initResult != 0) {
        std::fprintf(stderr, "Error: ATRAC9 decoder initialization failed (0x%08x).\n",
                     static_cast<unsigned int>(initResult));
        return false;
    }

    Atrac9CodecInfo codec{};
    if (Atrac9GetCodecInfo(decoder.get(), &codec) != 0 ||
        codec.channels != headerChannels || codec.samplingRate != int(headerSampleRate) ||
        codec.superframeSize <= 0 || codec.framesInSuperframe <= 0 || codec.frameSamples <= 0 ||
        codec.superframeSize != headerBlockAlign || encodedSize % size_t(codec.superframeSize) != 0)
        return false;

    const size_t superframeCount = encodedSize / size_t(codec.superframeSize);
    const size_t framesPerSuperframe = size_t(codec.framesInSuperframe);
    const size_t samplesPerFrame = size_t(codec.frameSamples);
    if (superframeCount > std::numeric_limits<size_t>::max() / framesPerSuperframe / samplesPerFrame)
        return false;
    const size_t decodedSampleCount = superframeCount * framesPerSuperframe * samplesPerFrame;
    if (size_t(loopStart) > decodedSampleCount ||
        size_t(audibleSampleCount) > decodedSampleCount - size_t(loopStart))
        return false;

    outChannels.assign(size_t(codec.channels), std::vector<double>(audibleSampleCount));
    std::vector<short> framePcm(samplesPerFrame * size_t(codec.channels));
    size_t decodedFrameStart = 0;
    size_t destinationFrame = 0;

    for (size_t sf = 0; sf < superframeCount; ++sf) {
        const size_t superframeStart = sf * size_t(codec.superframeSize);
        const size_t superframeEnd = superframeStart + size_t(codec.superframeSize);
        size_t cursor = superframeStart;

        for (size_t frame = 0; frame < framesPerSuperframe; ++frame) {
            if (cursor >= superframeEnd)
                return false;

            int bytesUsed = 0;
            const int decodeResult = Atrac9Decode(decoder.get(), encoded + cursor, framePcm.data(), &bytesUsed);
            if (decodeResult != 0 || bytesUsed <= 0 || size_t(bytesUsed) > superframeEnd - cursor) {
                std::fprintf(stderr, "Error: ATRAC9 frame decode failed (status 0x%08x, bytes %d).\n",
                             static_cast<unsigned int>(decodeResult), bytesUsed);
                return false;
            }
            cursor += size_t(bytesUsed);

            for (size_t sample = 0; sample < samplesPerFrame; ++sample) {
                const size_t streamSample = decodedFrameStart + sample;
                if (streamSample < size_t(loopStart) ||
                    streamSample >= size_t(loopStart) + size_t(audibleSampleCount))
                    continue;

                const size_t targetSample = destinationFrame++;
                for (int channel = 0; channel < codec.channels; ++channel) {
                    outChannels[size_t(channel)][targetSample] =
                        double(framePcm[sample * size_t(codec.channels) + size_t(channel)]) / 32768.0;
                }
            }
            decodedFrameStart += samplesPerFrame;
        }
    }

    if (destinationFrame != size_t(audibleSampleCount))
        return false;

    outSampleRate = uint32_t(codec.samplingRate);
    return true;
}


static bool LoadAt9File(const char* path,
                        std::vector<std::vector<double>>& outChannels,
                        uint32_t& outSampleRate)
{
    std::ifstream file(path, std::ios::binary);
    if (!file)
        return false;

    const std::vector<char> raw((std::istreambuf_iterator<char>(file)),
                                std::istreambuf_iterator<char>());
    const std::vector<uint8_t> bytes(raw.begin(), raw.end());
    return !bytes.empty() && loadAt9FromMemory(bytes.data(), bytes.size(), outChannels, outSampleRate);
}

static double CalculateRms(const std::vector<std::vector<double>>& channels)
{
    long double sumSquares = 0.0L;
    size_t sampleCount = 0;
    for (const auto& channel : channels)
    {
        for (const double sample : channel)
        {
            sumSquares += static_cast<long double>(sample) * sample;
            ++sampleCount;
        }
    }
    return sampleCount == 0 ? 0.0 : std::sqrt(static_cast<double>(sumSquares / sampleCount));
}

int main(int argc, char** argv)
{
    if (argc != 3)
        return 2;

    std::vector<std::vector<double>> before;
    std::vector<std::vector<double>> after;
    uint32_t beforeRate = 0;
    uint32_t afterRate = 0;
    if (!loadAudioFile(argv[1], before, beforeRate) ||
        !LoadAt9File(argv[2], after, afterRate) ||
        before.empty() || after.empty() || beforeRate != afterRate ||
        before.front().size() != after.front().size())
    {
        std::fprintf(stderr, "AT9 gain test: could not decode matching WAV/AT9 streams.\n");
        return 1;
    }

    const double beforeRms = CalculateRms(before);
    const double afterRms = CalculateRms(after);
    const double ratio = beforeRms > 0.0 ? afterRms / beforeRms : 0.0;
    constexpr double expectedGain = 1.0; // 0 dB unity gain
    constexpr double tolerance = 0.30;
    std::printf("AT9 gain test: before RMS %.6f, after RMS %.6f, ratio %.6f (target %.6f)\n",
                beforeRms, afterRms, ratio, expectedGain);
    if (ratio < expectedGain * (1.0 - tolerance) || ratio > expectedGain * (1.0 + tolerance))
    {
        std::fprintf(stderr, "AT9 gain test: expected the configured 0 dB (unity) gain, got %.6f.\n", ratio);
        return 1;
    }
    return 0;
}
