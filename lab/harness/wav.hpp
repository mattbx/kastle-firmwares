// Minimal 16-bit PCM WAV read and write for the desktop harness.
// MIT License, Copyright (c) 2026 kastle-firmwares contributors (see TapeLoop.hpp).
#pragma once

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace harness
{

inline bool Rd(void *dst, size_t size, size_t count, FILE *f)
{
    return std::fread(dst, size, count, f) == count;
}

struct Wav
{
    uint32_t rate = 44000;
    std::vector<int16_t> frames; // interleaved stereo
    size_t Frames() const { return frames.size() / 2; }
};

// Reads 16-bit PCM, mono or stereo. Mono is copied to both channels.
inline bool ReadWav(const std::string &path, Wav *out)
{
    FILE *f = std::fopen(path.c_str(), "rb");
    if (!f)
    {
        return false;
    }
    char id[4];
    uint32_t size = 0;
    if (std::fread(id, 1, 4, f) != 4 || std::memcmp(id, "RIFF", 4) != 0)
    {
        std::fclose(f);
        return false;
    }
    if (!Rd(&size, 4, 1, f) || !Rd(id, 1, 4, f)) // size, then WAVE
    {
        std::fclose(f);
        return false;
    }
    uint16_t channels = 0, bits = 0, format = 0;
    std::vector<int16_t> raw;
    while (std::fread(id, 1, 4, f) == 4 && std::fread(&size, 4, 1, f) == 1)
    {
        if (std::memcmp(id, "fmt ", 4) == 0)
        {
            std::vector<uint8_t> fmt(size);
            if (size < 16 || !Rd(fmt.data(), 1, size, f)) break;
            std::memcpy(&format, &fmt[0], 2);
            std::memcpy(&channels, &fmt[2], 2);
            std::memcpy(&out->rate, &fmt[4], 4);
            std::memcpy(&bits, &fmt[14], 2);
        }
        else if (std::memcmp(id, "data", 4) == 0)
        {
            raw.resize(size / 2);
            if (!Rd(raw.data(), 2, raw.size(), f)) break;
        }
        else
        {
            std::fseek(f, static_cast<long>(size + (size & 1)), SEEK_CUR);
        }
    }
    std::fclose(f);
    if (format != 1 || bits != 16 || (channels != 1 && channels != 2))
    {
        return false;
    }
    out->frames.clear();
    if (channels == 2)
    {
        out->frames = raw;
    }
    else
    {
        for (int16_t s : raw)
        {
            out->frames.push_back(s);
            out->frames.push_back(s);
        }
    }
    return true;
}

inline bool WriteWav(const std::string &path, const Wav &w)
{
    FILE *f = std::fopen(path.c_str(), "wb");
    if (!f)
    {
        return false;
    }
    uint32_t data = static_cast<uint32_t>(w.frames.size() * 2);
    uint32_t riff = 36 + data;
    uint16_t format = 1, channels = 2, bits = 16, align = 4;
    uint32_t fmt_size = 16, byte_rate = w.rate * 4;
    std::fwrite("RIFF", 1, 4, f);
    std::fwrite(&riff, 4, 1, f);
    std::fwrite("WAVEfmt ", 1, 8, f);
    std::fwrite(&fmt_size, 4, 1, f);
    std::fwrite(&format, 2, 1, f);
    std::fwrite(&channels, 2, 1, f);
    std::fwrite(&w.rate, 4, 1, f);
    std::fwrite(&byte_rate, 4, 1, f);
    std::fwrite(&align, 2, 1, f);
    std::fwrite(&bits, 2, 1, f);
    std::fwrite("data", 1, 4, f);
    std::fwrite(&data, 4, 1, f);
    std::fwrite(w.frames.data(), 2, w.frames.size(), f);
    std::fclose(f);
    return true;
}

} // namespace harness
