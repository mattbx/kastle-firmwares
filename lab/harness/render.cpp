// Renders Lasting Damage scenarios to WAV files for listening.
//
//   lasting_damage_render <input.wav> <output-dir>
//
// The input should be 16-bit, 44 kHz (the Kastle's rate). Each scenario plays
// half a second of input, records one second, closes the loop and lets it
// wear with the wet signal only (the app's dry/wet mix is not part of this).
// MIT License, Copyright (c) 2026 kastle-firmwares contributors (see TapeLoop.hpp).

#include <cstdint>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

#include "apps/LastingDamage/TapeLoop.hpp"
#include "wav.hpp"

using lasting_damage::TapeLoop;

namespace
{

constexpr uint32_t kRate = 44000;
constexpr uint32_t kCapacity = 48000;
constexpr uint32_t kSeam = 440;

int32_t Q15(double x) { return static_cast<int32_t>(x * 32768.0); }

struct Scenario
{
    const char *name;
    double seconds; // looping time after the loop closes
    // Called every frame while looping: t = seconds since the loop closed.
    std::function<void(TapeLoop &, double t)> control;
};

harness::Wav Run(const harness::Wav &in, const Scenario &s)
{
    std::vector<int16_t> buf(2 * kCapacity, 0);
    TapeLoop loop;
    loop.Init(buf.data(), kCapacity, kSeam, 12345);

    harness::Wav out;
    out.rate = kRate;
    const uint32_t pre = kRate / 2, rec = kRate;
    const uint32_t total = pre + rec + kSeam + static_cast<uint32_t>(s.seconds * kRate);
    size_t n_in = in.Frames();
    uint32_t looping_since = 0;
    for (uint32_t n = 0; n < total; n++)
    {
        if (n == pre) loop.Record();
        if (n == pre + rec) loop.Close();
        if (loop.GetState() == TapeLoop::State::LOOPING)
        {
            if (looping_since == 0) looping_since = n;
            s.control(loop, static_cast<double>(n - looping_since) / kRate);
        }
        size_t i = n % n_in;
        int16_t l, r;
        loop.Process(in.frames[2 * i], in.frames[2 * i + 1], &l, &r);
        out.frames.push_back(l);
        out.frames.push_back(r);
    }
    return out;
}

} // namespace

int main(int argc, char **argv)
{
    if (argc < 3)
    {
        std::fprintf(stderr, "usage: %s <input.wav> <output-dir>\n", argv[0]);
        return 2;
    }
    harness::Wav in;
    if (!harness::ReadWav(argv[1], &in) || in.Frames() == 0)
    {
        std::fprintf(stderr, "could not read %s (16-bit PCM WAV only)\n", argv[1]);
        return 1;
    }
    if (in.rate != kRate)
    {
        std::fprintf(stderr, "note: %s is %u Hz, played as if it were %u Hz\n", argv[1], in.rate, kRate);
    }
    const std::string dir = argv[2];

    const Scenario scenarios[] = {
        {"null", 8.0, [](TapeLoop &l, double) { l.SetWear(0); }},
        {"gentle", 60.0, [](TapeLoop &l, double) { l.SetWear(Q15(0.30)); }},
        {"medium", 45.0, [](TapeLoop &l, double) { l.SetWear(Q15(0.50)); }},
        {"heavy", 30.0, [](TapeLoop &l, double) { l.SetWear(Q15(0.80)); }},
        {"varispeed", 40.0, [](TapeLoop &l, double t) {
             l.SetWear(Q15(0.40));
             double speed = 1.0;
             if (t >= 10 && t < 20) speed = 1.0 - 0.5 * (t - 10) / 10; // glide to half speed
             else if (t >= 20 && t < 30) speed = -1.0;                 // reverse
             l.SetSpeed(Q15(speed));
         }},
    };

    for (const auto &s : scenarios)
    {
        harness::Wav out = Run(in, s);
        std::string path = dir + "/lasting-damage-" + s.name + ".wav";
        if (!harness::WriteWav(path, out))
        {
            std::fprintf(stderr, "could not write %s\n", path.c_str());
            return 1;
        }
        std::printf("wrote %s (%.1f s)\n", path.c_str(), static_cast<double>(out.Frames()) / kRate);
    }
    return 0;
}
