// Known-answer checks for Lasting Damage's TapeLoop, run on the desktop.
// Each check states its expected number up front, from the maths, and a wrong
// implementation gives a visibly wrong number.
// MIT License, Copyright (c) 2026 kastle-firmwares contributors (see TapeLoop.hpp).

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

#include "apps/LastingDamage/TapeLoop.hpp"

using lasting_damage::TapeLoop;

namespace
{

int g_failures = 0;

void Check(bool ok, const char *name, const char *detail)
{
    std::printf("%s  %-46s %s\n", ok ? "PASS" : "FAIL", name, detail);
    if (!ok)
    {
        g_failures++;
    }
}

constexpr uint32_t kRate = 44000;
constexpr uint32_t kSeam = 440;        // 10 ms
constexpr uint32_t kCapacity = 48000;  // what the Kastle app uses

struct Rig
{
    std::vector<int16_t> buf = std::vector<int16_t>(2 * kCapacity, 0);
    TapeLoop loop;
    explicit Rig(uint32_t seed = 1) { loop.Init(buf.data(), kCapacity, kSeam, seed); }

    void Frame(int16_t l, int16_t r, int16_t *ol = nullptr, int16_t *or_ = nullptr)
    {
        int16_t a, b;
        loop.Process(l, r, &a, &b);
        if (ol) *ol = a;
        if (or_) *or_ = b;
    }

    // Records exactly len frames of in(), closes, feeds the post-roll, then
    // runs until the output fade has finished. Afterwards the play head is
    // kSeam frames into the loop.
    template <typename F>
    void RecordLoop(uint32_t len, F in)
    {
        loop.Record();
        for (uint32_t n = 0; n < len; n++)
        {
            int16_t s = in(n);
            Frame(s, s);
        }
        loop.Close();
        for (uint32_t n = len; loop.GetState() != TapeLoop::State::LOOPING; n++)
        {
            int16_t s = in(n);
            Frame(s, s);
        }
        for (uint32_t n = 0; n < kSeam; n++)
        {
            Frame(0, 0);
        }
    }

    int16_t At(uint32_t frame, uint32_t ch = 0) const { return buf[2 * frame + ch]; }
};

int16_t TestSignal(uint32_t n)
{
    // Two sines and a slow sweep: busy enough that a misplaced sample shows.
    double t = static_cast<double>(n) / kRate;
    double v = 0.4 * std::sin(2 * M_PI * 220.0 * t) + 0.3 * std::sin(2 * M_PI * 1337.0 * t + 1.0) +
               0.2 * std::sin(2 * M_PI * (50.0 + 400.0 * t) * t);
    return static_cast<int16_t>(std::lround(v * 30000.0));
}

double Mean(const Rig &r, uint32_t from, uint32_t to)
{
    double s = 0;
    for (uint32_t i = from; i < to; i++) s += r.At(i);
    return s / (to - from);
}

double Rms(const Rig &r, uint32_t from, uint32_t to)
{
    double s = 0;
    for (uint32_t i = from; i < to; i++) s += static_cast<double>(r.At(i)) * r.At(i);
    return std::sqrt(s / (to - from));
}

// Runs whole passes of the loop (at whatever speed is set) with zero input.
void RunPasses(Rig &r, uint32_t passes)
{
    uint32_t target = r.loop.GetPasses() + passes;
    while (r.loop.GetPasses() < target) r.Frame(0, 0);
}

} // namespace

int main()
{
    char d[200];

    // 1. Null case: wear off, 1x. The loop must repeat the recording bit for
    //    bit, pass after pass (outside the baked seam).
    {
        Rig r;
        const uint32_t L = 30011; // odd, prime-ish, nothing lines up by luck
        r.RecordLoop(L, TestSignal);
        uint32_t mismatches = 0, compared = 0;
        uint32_t head = kSeam; // play head position after RecordLoop
        for (uint32_t n = 0; n < 5 * L; n++)
        {
            int16_t o;
            r.Frame(0, 0, &o);
            uint32_t idx = (head + n) % L;
            if (idx >= kSeam)
            {
                compared++;
                if (o != TestSignal(idx)) mismatches++;
            }
        }
        std::snprintf(d, sizeof d, "expected 0 mismatches, got %u of %u", mismatches, compared);
        Check(mismatches == 0, "null: wear off repeats bit for bit", d);
    }

    // 2. Seam bake on DC: a linear crossfade of DC with DC is exactly DC.
    {
        Rig r;
        const uint32_t L = 10000;
        r.RecordLoop(L, [](uint32_t) { return int16_t(1234); });
        uint32_t bad = 0;
        for (uint32_t i = 0; i < L; i++) bad += (r.At(i, 0) != 1234) + (r.At(i, 1) != 1234);
        std::snprintf(d, sizeof d, "expected every sample 1234, %u differ", bad);
        Check(bad == 0, "seam: DC stays exactly DC", d);
    }

    // 3. Seam continuity: a 440 Hz sine cut at an arbitrary length. The
    //    biggest step between samples must stay near the sine's own maximum
    //    slope, 2*pi*f*A/fs = 2*pi*440*10000/44000 = 628. An unbaked cut
    //    would step by up to 2A = 20000.
    {
        Rig r;
        const uint32_t L = 12345;
        auto sine = [](uint32_t n) {
            return static_cast<int16_t>(std::lround(10000.0 * std::sin(2 * M_PI * 440.0 * n / kRate)));
        };
        r.RecordLoop(L, sine);
        int32_t max_step = 0;
        int16_t prev;
        r.Frame(0, 0, &prev);
        for (uint32_t n = 0; n < 3 * L; n++)
        {
            int16_t o;
            r.Frame(0, 0, &o);
            int32_t step = std::abs(o - prev);
            if (step > max_step) max_step = step;
            prev = o;
        }
        std::snprintf(d, sizeof d, "expected max step <= 650 (sine slope 628), got %d", max_step);
        Check(max_step <= 650, "seam: no click at the loop point", d);
    }

    // 4. Speed 2x: the head reads every second frame, so with an even length
    //    the output repeats exactly every L/2 frames.
    {
        Rig r;
        const uint32_t L = 20000;
        r.RecordLoop(L, TestSignal);
        r.loop.ForceSpeed(2 * TapeLoop::kUnity);
        std::vector<int16_t> out(2 * L);
        for (auto &o : out) r.Frame(0, 0, &o);
        uint32_t bad = 0;
        for (uint32_t n = 0; n + L / 2 < out.size(); n++) bad += out[n] != out[n + L / 2];
        std::snprintf(d, sizeof d, "expected period L/2 = %u exactly, %u frames differ", L / 2, bad);
        Check(bad == 0, "speed: 2x halves the loop period", d);
    }

    // 5. Speed 0.5x: every second output frame sits exactly on a stored frame,
    //    and the ones between are the average of their neighbours (floored).
    {
        Rig r;
        const uint32_t L = 20000;
        r.RecordLoop(L, TestSignal);
        r.loop.ForceSpeed(TapeLoop::kUnity / 2);
        int32_t phase0 = r.loop.GetPhase(); // whole frame, as 1x left it
        uint32_t bad = 0;
        for (uint32_t n = 0; n < 4000; n++)
        {
            int16_t o;
            r.Frame(0, 0, &o);
            uint32_t idx = ((phase0 >> 15) + n / 2) % L;
            int32_t a = r.At(idx), b = r.At((idx + 1) % L);
            int32_t want = (n % 2 == 0) ? a : a + (((b - a) * 16384) >> 15);
            bad += o != want;
        }
        std::snprintf(d, sizeof d, "expected 0 interpolation errors, got %u", bad);
        Check(bad == 0, "speed: 0.5x interpolates halfway exactly", d);
    }

    // 6. Reverse: -1x reads the frames backwards.
    {
        Rig r;
        const uint32_t L = 20000;
        r.RecordLoop(L, TestSignal);
        r.loop.ForceSpeed(-TapeLoop::kUnity);
        uint32_t start = static_cast<uint32_t>(r.loop.GetPhase() >> 15);
        uint32_t bad = 0;
        for (uint32_t n = 0; n < 2 * L; n++)
        {
            int16_t o;
            r.Frame(0, 0, &o);
            bad += o != r.At((start + 2 * L - n) % L);
        }
        std::snprintf(d, sizeof d, "expected 0 out of order, got %u", bad);
        Check(bad == 0, "speed: -1x plays backwards", d);
    }

    // 7. Level loss: DC 16000 with keep 0.9 per pass (no dulling, no flakes).
    //    After 10 passes: 16000 * 0.9^10 = 5578.9. Dither is zero-mean, so
    //    the mean over the loop lands within a step of that.
    {
        Rig r;
        const uint32_t L = 20000;
        r.RecordLoop(L, [](uint32_t) { return int16_t(16000); });
        r.loop.SetWearParts(true, 58982, TapeLoop::kUnity, 0, 0); // 58982/65536 = 0.9
        RunPasses(r, 10);
        // Wear started with the head at frame kSeam, and the head has just
        // wrapped, so frames past kSeam have been passed exactly 10 times.
        double want = 16000.0 * std::pow(58982.0 / 65536.0, 10);
        double got = Mean(r, kSeam + 10, L);
        std::snprintf(d, sizeof d, "expected %.1f +- 1.5, got %.2f", want, got);
        Check(std::fabs(got - want) <= 1.5, "wear: level falls by keep^passes", d);
    }

    // 8. No negative creep: a silent loop worn for 200 passes at keep 0.999.
    //    Rounding plus zero-mean dither: the mean stays within half a step of
    //    0. (Truncating instead would drift by about -0.5 per pass.)
    {
        Rig r;
        const uint32_t L = 20000;
        r.RecordLoop(L, [](uint32_t) { return int16_t(0); });
        r.loop.SetWearParts(true, 65470, TapeLoop::kUnity, 0, 0); // 0.999
        RunPasses(r, 200);
        double got = Mean(r, 0, L);
        std::snprintf(d, sizeof d, "expected |mean| < 0.5, got %.3f", got);
        Check(std::fabs(got) < 0.5, "wear: rounding, no creep below zero", d);
    }

    // 9. Hiss floor: each pass adds dither (variance 1/6) and rounding error
    //    (1/12), 1/4 in total, while keep g shrinks what's there. The noise
    //    settles at rms = 0.5 / sqrt(1 - g^2). For g = 0.9: 1.147 steps.
    {
        Rig r;
        const uint32_t L = 40000;
        r.RecordLoop(L, [](uint32_t) { return int16_t(0); });
        r.loop.SetWearParts(true, 58982, TapeLoop::kUnity, 0, 0);
        RunPasses(r, 80);
        double g = 58982.0 / 65536.0;
        double want = 0.5 / std::sqrt(1 - g * g);
        double got = Rms(r, 0, L);
        std::snprintf(d, sizeof d, "expected %.3f +- 8%%, got %.3f", want, got);
        Check(std::fabs(got - want) <= 0.08 * want, "wear: loop decays into a known hiss floor", d);
    }

    // 10. Dulling: the fastest possible wiggle (+A, -A, ...) through one pass
    //     of a one-pole with coefficient a has gain a / (2 - a). For a = 0.5
    //     that's 1/3: 9000 becomes 3000.
    {
        Rig r;
        const uint32_t L = 20000; // even, so the pattern survives the seam bake
        r.RecordLoop(L, [](uint32_t n) { return int16_t(n % 2 ? -9000 : 9000); });
        r.loop.SetWearParts(true, 65536, TapeLoop::kUnity / 2, 0, 0);
        uint32_t head = static_cast<uint32_t>(r.loop.GetPhase() >> 15);
        RunPasses(r, 1);
        // Frames the head has passed once since wear started, away from the
        // filter's first-sample transient.
        uint32_t now = static_cast<uint32_t>(r.loop.GetPhase() >> 15);
        uint32_t worst = 0;
        for (uint32_t i = head + 50; i < L; i++)
        {
            uint32_t err = static_cast<uint32_t>(std::abs(std::abs(r.At(i)) - 3000));
            if (err > worst) worst = err;
        }
        std::snprintf(d, sizeof d, "expected |x| = 3000 +- 2, worst error %u (head %u->%u)", worst, head, now);
        Check(worst <= 2, "wear: dulling matches the one-pole maths", d);
    }

    // 11. Flakes never click: DC 10000, flakes at their deepest (0.9) and
    //     back to back (a new one may start as soon as the last ends). One
    //     pass lays them down; the next pass plays them with wear off. The
    //     steepest flake edge is depth*A / (len/2) = 0.9 * 10000 / 32 = 281
    //     per frame, plus a step or two of dither: no step may exceed 290.
    //     (Flakes from later passes can overlap earlier ones and be steeper;
    //     that's still a slope over milliseconds, not a click.)
    {
        Rig r;
        const uint32_t L = 20000;
        r.RecordLoop(L, [](uint32_t) { return int16_t(10000); });
        r.loop.SetWearParts(true, 65536, TapeLoop::kUnity, 1u << 26, 29491); // 1 in 64
        RunPasses(r, 1);
        r.loop.SetWearParts(false, 65536, TapeLoop::kUnity, 0, 0);
        int32_t max_step = 0, min_level = 32767;
        int16_t prev;
        r.Frame(0, 0, &prev);
        for (uint32_t n = 0; n < L; n++)
        {
            int16_t o;
            r.Frame(0, 0, &o);
            max_step = std::max(max_step, std::abs(o - prev));
            min_level = std::min<int32_t>(min_level, o);
            prev = o;
        }
        std::snprintf(d, sizeof d, "expected step <= 290 and some dips, got step %d, min %d", max_step, min_level);
        Check(max_step <= 290 && min_level < 5000, "wear: flakes dip smoothly", d);
    }

    // 12. Seeded means repeatable: the same seed gives the same worn loop,
    //     another seed a different one.
    {
        auto run = [](uint32_t seed) {
            Rig r(seed);
            r.RecordLoop(20000, TestSignal);
            r.loop.SetWear(26000);
            RunPasses(r, 5);
            return r.buf;
        };
        bool same = run(7) == run(7);
        bool differs = run(7) != run(8);
        std::snprintf(d, sizeof d, "same seed identical: %s, other seed differs: %s", same ? "yes" : "no", differs ? "yes" : "no");
        Check(same && differs, "seed: worn loop is repeatable", d);
    }

    // 13. Wear knob at zero (and pot noise below 256) is the null case.
    {
        Rig r;
        const uint32_t L = 15000;
        r.RecordLoop(L, TestSignal);
        std::vector<int16_t> before = r.buf;
        r.loop.SetWear(200);
        RunPasses(r, 3);
        std::snprintf(d, sizeof d, "expected buffer untouched: %s", r.buf == before ? "untouched" : "CHANGED");
        Check(r.buf == before, "wear: knob below 256 changes nothing", d);
    }

    // 14. Automatic close: recording past the end closes the loop at
    //     capacity - seam = 47560 frames.
    {
        Rig r;
        r.loop.Record();
        for (uint32_t n = 0; n < kCapacity + 10; n++) r.Frame(1, 1);
        std::snprintf(d, sizeof d, "expected LOOPING with length %u, got state %d length %u",
                      kCapacity - kSeam, static_cast<int>(r.loop.GetState()), r.loop.GetLength());
        Check(r.loop.GetState() == TapeLoop::State::LOOPING && r.loop.GetLength() == kCapacity - kSeam,
              "record: full buffer closes the loop", d);
    }

    std::printf("\n%s: %d failure(s)\n", g_failures ? "FAILED" : "ALL PASSED", g_failures);
    return g_failures ? 1 : 0;
}
