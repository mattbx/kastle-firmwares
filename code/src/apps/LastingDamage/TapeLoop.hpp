/*
MIT License

Copyright (c) 2026 kastle-firmwares contributors

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#pragma once

// TapeLoop - the heart of Lasting Damage: a short stereo loop recorded into
// RAM that wears out a little every time the head passes over it.
//
// Portable on purpose: standard headers only, no Pico SDK, no floats, no
// allocation. The Kastle app and the desktop harness (lab/harness) compile
// this same file, so what the harness checks is what the device runs.
//
// The idea in one paragraph: a real tape loop sheds oxide as it plays. Here
// the "wear head" sits on the playback head, and every sample it passes is
// written back slightly damaged: duller (a gentle lowpass along the tape),
// a touch quieter, now and then hit by a "flake" (a short dip in level), and
// re-rounded to 16 bits with a little noise (dither). Because the damage is
// written back into the same buffer, it accumulates. After enough passes the
// loop is a ghost in a bed of hiss. Wear at zero skips the write-back
// entirely, so the loop then repeats bit for bit.
//
// Number formats (fixed point, because the RP2040 has no floating point unit):
// - Q15: an int where 32768 means 1.0. Like storing money as cents.
// - phase: 16.15 fixed point, the integer part is the frame index.
// - Q8 sample: a 16-bit sample shifted up 8 bits, so the wear maths keeps
//   8 extra bits of precision before rounding back to 16 bits.

#include <cstddef>
#include <cstdint>

// The firmware defines this as always_inline before including, so the
// per-sample functions are folded into the app's AudioLoop, which Bastl's
// FASTCODE places in RAM (code read from flash can stall the audio). Putting
// these header functions in .fastcode directly fails: inline functions live
// in their own sections and GCC reports a section type conflict. On the
// desktop it is empty.
#ifndef LASTING_DAMAGE_FAST
#define LASTING_DAMAGE_FAST
#endif

namespace lasting_damage
{

class TapeLoop
{
public:
    enum class State : uint8_t
    {
        EMPTY,     ///< Nothing recorded: the wet output is the input.
        RECORDING, ///< Writing the input into the buffer.
        CLOSING,   ///< Recording a short post-roll to bake the seam.
        LOOPING,   ///< Playing, and wearing, the loop.
    };

    static constexpr int32_t kUnity = 1 << 15;      ///< 1.0 in Q15.
    static constexpr int32_t kMaxSpeed = 4 * kUnity; ///< 4x either way.
    static constexpr uint32_t kMaxFrames = 65535;    ///< Phase format limit.

    /**
     * @param buffer Interleaved stereo frames (L, R, L, R...), owned by the host.
     * @param capacity_frames Frames the buffer holds (clamped to kMaxFrames).
     * @param seam_frames Crossfade length at the loop seam and for the output
     *        fades (about 10 ms is right).
     * @param seed Seeds the flakes and the dither, so a run is repeatable.
     */
    void Init(int16_t *buffer, uint32_t capacity_frames, uint32_t seam_frames, uint32_t seed)
    {
        buf_ = buffer;
        cap_ = capacity_frames > kMaxFrames ? kMaxFrames : capacity_frames;
        seam_ = seam_frames < 1 ? 1 : seam_frames;
        if (cap_ < 3 * seam_ + 1)
        {
            seam_ = cap_ / 4 > 0 ? cap_ / 4 : 1; // tiny buffers: keep the maths valid
        }
        // Round up, so the fade always completes within seam_ frames.
        mix_step_ = (kUnity + static_cast<int32_t>(seam_) - 1) / static_cast<int32_t>(seam_);
        rng_ = seed != 0 ? seed : 0x9E3779B9u; // xorshift must not start at 0
        state_ = State::EMPTY;
        len_ = 0;
        rec_pos_ = 0;
        phase_ = 0;
        passes_ = 0;
        wear_idx_ = 0;
        loop_mix_ = 0;
        speed_target_ = kUnity;
        speed_smooth_ = kUnity << 8;
        SetWear(0);
        flake_left_ = 0;
        reset_prev_ = true;
    }

    /// Start recording from the top. If a loop is playing it fades out.
    void Record()
    {
        state_ = State::RECORDING;
        rec_pos_ = 0;
    }

    /// Stop recording. The loop starts after a short post-roll (seam_frames).
    void Close()
    {
        if (state_ != State::RECORDING)
        {
            return;
        }
        uint32_t min_len = 2 * seam_;
        len_ = rec_pos_ > min_len ? rec_pos_ : min_len;
        state_ = State::CLOSING;
    }

    /// Forget the loop: back to passthrough.
    void Clear()
    {
        state_ = State::EMPTY;
    }

    /// Playback speed in Q15: 32768 = 1x forward, negative = reverse.
    void SetSpeed(int32_t speed_q15)
    {
        if (speed_q15 > kMaxSpeed)
        {
            speed_q15 = kMaxSpeed;
        }
        else if (speed_q15 < -kMaxSpeed)
        {
            speed_q15 = -kMaxSpeed;
        }
        speed_target_ = speed_q15;
    }

    /// Jump straight to a speed, skipping the smoothing (tests, and Init).
    void ForceSpeed(int32_t speed_q15)
    {
        SetSpeed(speed_q15);
        speed_smooth_ = speed_target_ << 8;
    }

    /**
     * Wear amount in Q15: 0 = a perfect loop, 32767 = mostly gone in about
     * 8 passes. Values under 256 count as zero so pot noise at the bottom of
     * the knob can't start a slow wear.
     *
     * Our first guesses, to tune by ear (all scale with wear squared, so the
     * bottom half of the knob is gentle):
     * - keep (level kept per pass): 1.0 down to 0.75
     * - dull (one-pole coefficient along the tape): 1.0 (none) down to 0.4
     * - flakes: up to one per 2000 frames passed, 0.3 to 0.9 deep
     */
    void SetWear(int32_t wear_q15)
    {
        if (wear_q15 < 0)
        {
            wear_q15 = 0;
        }
        if (wear_q15 > kUnity - 1)
        {
            wear_q15 = kUnity - 1;
        }
        int32_t w2 = (wear_q15 * wear_q15) >> 15;
        SetWearParts(wear_q15 >= 256,
                     65536 - ((w2 * 16384) >> 15),
                     kUnity - ((w2 * 19661) >> 15),
                     static_cast<uint32_t>((static_cast<uint64_t>(w2) * 2147483u) >> 15),
                     9830 + ((wear_q15 * 19661) >> 15));
    }

    /**
     * The four kinds of wear set one by one. SetWear() maps the knob onto
     * these; the tests call this directly so each kind can be checked alone.
     * @param on false skips the write-back entirely (bit-exact loop).
     * @param keep_q16 Level kept per pass, 65536 = 1.0.
     * @param dull_q15 One-pole coefficient along the tape, 32768 = no dulling.
     * @param flake_rate Chance per frame passed that a flake starts, out of 2^32.
     * @param flake_depth_q15 How deep a flake dips, 32768 = to silence.
     */
    void SetWearParts(bool on, int32_t keep_q16, int32_t dull_q15, uint32_t flake_rate, int32_t flake_depth_q15)
    {
        if (on && !wear_on_)
        {
            reset_prev_ = true; // start the dulling filter from the tape, not stale state
        }
        wear_on_ = on;
        keep_q16_ = keep_q16;
        dull_q15_ = dull_q15;
        flake_rate_ = flake_rate;
        flake_depth_q15_ = flake_depth_q15;
    }

    /**
     * One stereo frame. Writes the wet signal: the loop when looping, the
     * input otherwise, with a seam_frames fade between the two.
     */
    LASTING_DAMAGE_FAST void Process(int16_t in_l, int16_t in_r, int16_t *out_l, int16_t *out_r)
    {
        int32_t loop_l = 0;
        int32_t loop_r = 0;

        switch (state_)
        {
        case State::EMPTY:
            break;

        case State::RECORDING:
            if (len_ > 0 && loop_mix_ > 0)
            {
                ReadAndAdvance(&loop_l, &loop_r, false); // old loop fading out
            }
            WriteFrame(rec_pos_, in_l, in_r);
            rec_pos_++;
            if (rec_pos_ >= cap_ - seam_)
            {
                Close(); // buffer full: close the loop automatically
            }
            break;

        case State::CLOSING:
            WriteFrame(rec_pos_, in_l, in_r);
            rec_pos_++;
            if (rec_pos_ >= len_ + seam_)
            {
                BakeSeam();
                state_ = State::LOOPING;
                phase_ = 0;
                wear_idx_ = 0;
                passes_ = 0;
                reset_prev_ = true;
            }
            break;

        case State::LOOPING:
            ReadAndAdvance(&loop_l, &loop_r, true);
            break;
        }

        // Fade between input (monitor) and loop.
        int32_t target = (state_ == State::LOOPING) ? kUnity : 0;
        if (loop_mix_ < target)
        {
            loop_mix_ = (loop_mix_ + mix_step_ > target) ? target : loop_mix_ + mix_step_;
        }
        else if (loop_mix_ > target)
        {
            loop_mix_ = (loop_mix_ - mix_step_ < target) ? target : loop_mix_ - mix_step_;
        }
        // (loop - in) is at most 65535 and loop_mix_ at most 32768, so the
        // product fits in 32 bits. At 0 and 32768 this is exactly in / loop.
        *out_l = static_cast<int16_t>(in_l + (((loop_l - in_l) * loop_mix_) >> 15));
        *out_r = static_cast<int16_t>(in_r + (((loop_r - in_r) * loop_mix_) >> 15));
    }

    State GetState() const { return state_; }
    uint32_t GetLength() const { return len_; }
    uint32_t GetPasses() const { return passes_; }
    uint32_t GetRecordPosition() const { return rec_pos_; }
    uint32_t GetCapacity() const { return cap_; }
    uint32_t GetSeam() const { return seam_; }
    int32_t GetPhase() const { return phase_; }

private:
    static int16_t Clamp16(int32_t x)
    {
        return static_cast<int16_t>(x > 32767 ? 32767 : (x < -32768 ? -32768 : x));
    }

    // xorshift32: a tiny, fast, repeatable random number generator.
    uint32_t Random()
    {
        uint32_t x = rng_;
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        rng_ = x;
        return x;
    }

    void WriteFrame(uint32_t i, int16_t l, int16_t r)
    {
        buf_[2 * i] = l;
        buf_[2 * i + 1] = r;
    }

    // The post-roll (frames len_ .. len_+seam_-1) is what came straight after
    // the loop's last frame. Crossfading it into the loop's first frames makes
    // the end flow into the start, so the seam never clicks, on any pass.
    // Linear crossfade: DC in both halves stays exactly DC.
    void BakeSeam()
    {
        for (uint32_t j = 0; j < seam_; j++)
        {
            int32_t t = static_cast<int32_t>(((2 * j + 1) * static_cast<uint32_t>(kUnity)) / (2 * seam_));
            for (uint32_t ch = 0; ch < 2; ch++)
            {
                int32_t start = buf_[2 * j + ch];
                int32_t post = buf_[2 * (len_ + j) + ch];
                buf_[2 * j + ch] = Clamp16((start * t + post * (kUnity - t) + (1 << 14)) >> 15);
            }
        }
    }

    LASTING_DAMAGE_FAST void ReadAndAdvance(int32_t *l, int32_t *r, bool wear)
    {
        // Linear interpolation between this frame and the next (wrapping).
        int32_t idx = phase_ >> 15;
        int32_t frac = phase_ & 0x7FFF;
        int32_t nxt = idx + 1;
        if (nxt >= static_cast<int32_t>(len_))
        {
            nxt = 0;
        }
        const int16_t *a = &buf_[2 * idx];
        const int16_t *b = &buf_[2 * nxt];
        *l = a[0] + (((b[0] - a[0]) * frac) >> 15);
        *r = a[1] + (((b[1] - a[1]) * frac) >> 15);

        // Smooth speed changes (one-pole, about 12 ms) so TIME never zippers.
        // speed_smooth_ keeps 8 extra bits so small steps still arrive.
        speed_smooth_ += ((speed_target_ << 8) - speed_smooth_) >> 9;
        int32_t inc = speed_smooth_ >> 8;

        phase_ += inc;
        const int32_t lenq = static_cast<int32_t>(len_) << 15;
        while (phase_ >= lenq)
        {
            phase_ -= lenq;
            passes_++;
        }
        while (phase_ < 0)
        {
            phase_ += lenq;
            passes_++;
        }

        // The wear head follows the play head and damages every frame it
        // passes, once per pass, in either direction.
        int32_t cur = phase_ >> 15;
        // A flake that is under way finishes even if wear was just turned
        // off; stopping it half way would leave a step (a click) on the tape.
        if (!wear || (!wear_on_ && flake_left_ == 0))
        {
            wear_idx_ = cur;
            return;
        }
        int32_t guard = 0;
        if (inc >= 0)
        {
            while (wear_idx_ != cur && guard++ < 8)
            {
                wear_idx_ = (wear_idx_ + 1 >= static_cast<int32_t>(len_)) ? 0 : wear_idx_ + 1;
                Damage(wear_idx_);
            }
        }
        else
        {
            while (wear_idx_ != cur && guard++ < 8)
            {
                wear_idx_ = (wear_idx_ == 0) ? static_cast<int32_t>(len_) - 1 : wear_idx_ - 1;
                Damage(wear_idx_);
            }
        }
        wear_idx_ = cur;
    }

    // One frame of wear, written back in place.
    LASTING_DAMAGE_FAST void Damage(int32_t i)
    {
        // Flakes: now and then start a short dip in level (64-1087 frames,
        // 1.5-25 ms), shaped as a triangle so it never clicks.
        if (flake_left_ == 0 && wear_on_ && Random() < flake_rate_)
        {
            flake_len_ = 64 + (Random() >> 22);
            flake_left_ = flake_len_;
            flake_amt_ = flake_depth_q15_;
            flake_tri_ = 0;
            flake_step_ = static_cast<int32_t>((2u * kUnity) / flake_len_);
        }
        int32_t flake_gain = kUnity;
        if (flake_left_ > 0)
        {
            flake_gain = kUnity - ((flake_amt_ * flake_tri_) >> 15);
            if (flake_left_ > flake_len_ / 2)
            {
                flake_tri_ = (flake_tri_ + flake_step_ > kUnity) ? kUnity : flake_tri_ + flake_step_;
            }
            else
            {
                flake_tri_ = (flake_tri_ - flake_step_ < 0) ? 0 : flake_tri_ - flake_step_;
            }
            flake_left_--;
        }
        // Total gain for this frame in Q16 (65536 = 1.0). With wear off only
        // the finishing flake applies.
        const int32_t keep = wear_on_ ? keep_q16_ : 65536;
        const int32_t dull = wear_on_ ? dull_q15_ : kUnity;
        int32_t gain_q16 = static_cast<int32_t>((static_cast<int64_t>(keep) * flake_gain) >> 15);

        uint32_t dither = Random();
        const uint32_t base = 2u * static_cast<uint32_t>(i);
        for (uint32_t ch = 0; ch < 2; ch++)
        {
            int32_t x = static_cast<int32_t>(buf_[base + ch]) * 256; // Q8
            if (reset_prev_)
            {
                prev_[ch] = x;
            }
            // Dulling: a one-pole lowpass run along the tape. Each pass takes
            // off a little more top end. dull = 1.0 leaves it untouched.
            int32_t y = prev_[ch] + static_cast<int32_t>((static_cast<int64_t>(dull) * (x - prev_[ch])) >> 15);
            prev_[ch] = y;
            // Level loss and flakes. int64 because Q8 x Q16 needs 40 bits.
            y = static_cast<int32_t>((static_cast<int64_t>(y) * gain_q16) >> 16);
            // Back to 16 bits: TPDF dither (two random bytes subtracted gives a
            // triangle-shaped spread of +-1 step), then round (+128 is half a
            // step in Q8). Rounding instead of truncating matters here: a plain
            // shift always rounds down, so the loop would creep negative on
            // every pass. The dither keeps the decay going below the last bit
            // (no dead band), so the loop fades into a bed of hiss, not silence.
            uint32_t byte_a = (dither >> (16 * ch)) & 0xFF;
            uint32_t byte_b = (dither >> (16 * ch + 8)) & 0xFF;
            int32_t d = static_cast<int32_t>(byte_a) - static_cast<int32_t>(byte_b);
            buf_[base + ch] = Clamp16((y + d + 128) >> 8);
        }
        reset_prev_ = false;
    }

    int16_t *buf_ = nullptr;
    uint32_t cap_ = 0;
    uint32_t seam_ = 1;
    uint32_t len_ = 0;
    uint32_t rec_pos_ = 0;
    State state_ = State::EMPTY;

    int32_t phase_ = 0;        // 16.15 fixed point frame position
    int32_t speed_target_ = 0; // Q15
    int32_t speed_smooth_ = 0; // Q23 (Q15 with 8 extra bits)
    uint32_t passes_ = 0;

    int32_t loop_mix_ = 0; // Q15, 0 = input, 32768 = loop
    int32_t mix_step_ = 1;

    bool wear_on_ = false;
    bool reset_prev_ = true;
    int32_t keep_q16_ = 65536;
    int32_t dull_q15_ = kUnity;
    uint32_t flake_rate_ = 0;
    int32_t flake_depth_q15_ = 0;
    int32_t wear_idx_ = 0;
    int32_t prev_[2] = {0, 0}; // dulling filter state, Q8

    uint32_t flake_left_ = 0;
    uint32_t flake_len_ = 1;
    int32_t flake_amt_ = 0;
    int32_t flake_tri_ = 0;
    int32_t flake_step_ = 0;

    uint32_t rng_ = 1;
};

} // namespace lasting_damage
