#pragma once
#include <common.h>

#include <sample.h>
#include <units.h>
#include <algorithm>
#include <limits>
#include <stdexcept>
#include <SDL2/SDL_rwops.h>

namespace chromatracker::file {

// heavily based on:
// https://github.com/OpenMPT/openmpt/blob/master/soundlib/ITCompression.cpp
// https://wiki.multimedia.cx/index.php/Impulse_Tracker#IT214_sample_compression
// https://github.com/nicolasgramlich/AndEngineMODPlayerExtension/blob/master/jni/loaders/itsex.c

// Algorithm parameters for 16-Bit samples
struct IT16BitParams
{
	using sample_t = int16_t;
    static constexpr sample_t maxVal = INT16_MAX;
	static constexpr int8_t fetchA = 4;
	static constexpr int8_t lowerB = -8;
	static constexpr int8_t upperB = 7;
	static constexpr int8_t defaultWidth = 17;
};

// Algorithm parameters for 8-Bit samples
struct IT8BitParams
{
	using sample_t = int8_t;
    static constexpr sample_t maxVal = INT8_MAX;
	static constexpr int8_t fetchA = 3;
	static constexpr int8_t lowerB = -4;
	static constexpr int8_t upperB = 3;
	static constexpr int8_t defaultWidth = 9;
};

// decompress IT samples
template<typename Params>
class ITDecompress
{
private:
    static const int BLOCK_SIZE = 0x8000;

    SDL_RWops *stream;
    Sample *sample;
    const frames numFrames;
    const int numChannels;
    const bool it215;

    unique_ptr<uint8_t[]> block;
    int bitPos;

    int blockLength;
    unsigned int mem1, mem2; // integrator memory

public:
    ITDecompress(SDL_RWops *stream, Sample *sample,
                 frames numFrames, int numChannels, bool it215)
        : stream(stream)
        , sample(sample)
        , numFrames(numFrames)
        , numChannels(numChannels)
        , it215(it215)
    {}

private:
    uint32_t readBits(int numBits)
    {
        uint32_t value = 0;
        uint32_t bitCount = 0;
        while (bitCount < numBits) {
            uint32_t bitOffset = bitPos % 8;
            uint32_t readBits = std::min(8 - bitOffset, numBits - bitCount);
            uint32_t bits = block[bitPos / 8] >> bitOffset;
            value |= bits << bitCount;
            bitPos += readBits;
            bitCount += readBits;
        }
        value &= (1 << numBits) - 1;
        return value;
    }

    void write(int v, int topBit, vector<float> &wave)
    {
        if (v & topBit)
            v -= topBit << 1; // make negative
        mem1 += v;
        mem2 += mem1;
        Params::sample_t val = (int)(it215 ? mem2 : mem1);
        wave.push_back((float)val / Params::maxVal);
        blockLength--;
        if (wave.back() > 1.1 || wave.back() < -1.1) {
            throw std::runtime_error("it's broken :(");
        }
    }

    void changeWidth(int *curWidth, int width)
    {
        width++;
        if (width >= *curWidth)
            width++;
        *curWidth = width;
    }

    void decompressBlock(vector<float> &wave)
    {
        blockLength = std::min(numFrames - wave.size(),
                               BLOCK_SIZE / sizeof(Params::sample_t));

        int width = Params::defaultWidth;
        while (blockLength > 0) {
            if (width > Params::defaultWidth) {
                throw std::runtime_error("Invalid bit width");
            }

            int v = readBits(width);
            int topBit = 1 << (width - 1);
            if (width <= 6) {
                // Mode A: 1 to 6 bits
                if (v == topBit)
                    changeWidth(&width, readBits(Params::fetchA));
                else
                    write(v, topBit, wave);
            } else if (width < Params::defaultWidth) {
                // Mode B: 7 to 8 / 16 bits
                if (v >= topBit + Params::lowerB && v <= topBit + Params::upperB)
                    changeWidth(&width, v - (topBit + Params::lowerB));
                else
                    write(v, topBit, wave);
            } else {
                // Mode C: 9 / 17 bits
                if (v & topBit)
                    width = (v & ~topBit) + 1;
                else
                    write((v & ~topBit), 0, wave);
            }
        }
    }

public:
    void decompress()
    {
        for (int c = 0; c < numChannels; c++) {
            vector<float> &wave = sample->channels[c];
            while (wave.size() < numFrames) {
                uint16_t compressedSize = SDL_ReadLE16(stream);
                if (!compressedSize)
                    continue;
                block = unique_ptr<uint8_t[]>(new uint8_t[compressedSize]);
                SDL_RWread(stream, block.get(), 1, compressedSize);
                bitPos = 0;

                mem1 = mem2 = 0;
                decompressBlock(wave);
            }
        }
    }
};

} // namespace
