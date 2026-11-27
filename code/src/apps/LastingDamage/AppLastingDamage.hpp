/*
MIT License

Copyright (c) 2026 kastle-firmwares contributors
Based on Bastl Instruments' AppTemplate and AppFxWizard:
Copyright (c) 2024 Vaclav Mach (Bastl Instruments)
Copyright (c) 2024 Marek Mach (Bastl Instruments)

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

#include <cstddef>
#include <cstdint>
#include <memory>
#include "common/controls/FancyPot.hpp"
#include "common/core/App.hpp"
#include "common/core/Hardware.hpp"
#include "common/core/Kastle2.hpp"
#include "common/dsp/filters/DjFilter.hpp"
#include "common/dsp/math/math_utils.hpp"
#include "common/dsp/math/qmath.hpp"
#include "common/dsp/utility/EdgeDetector.hpp"
#include "common/fastcode.hpp"

// Fold TapeLoop's per-sample code into AudioLoop, so it runs from RAM too.
#define LASTING_DAMAGE_FAST __attribute__((always_inline))
#include "apps/LastingDamage/TapeLoop.hpp"

namespace kastle2
{

/**
 * @class AppLastingDamage
 * @ingroup apps
 * @brief A disintegration loop: record about a second, and every pass wears it.
 *
 * Panel (the FX Wizard's printed labels):
 * - MODE button, short press: record / close the loop. TRIG IN does the same.
 * - MODE button, hold about a second: reverse the tape.
 * - TIME: tape speed, half to double, 1x at the centre.
 * - TIME MOD: how much the FREE and STEP CVs move the speed (centre = none).
 * - FEEDBACK: wear. Fully left the loop repeats forever, unchanged.
 * - FEEDBACK MOD: how much the FEEDBACK CV moves the wear.
 * - AMOUNT: dry/wet. SHIFT + AMOUNT: how much the AMOUNT CV moves it.
 * - SHIFT + FEEDBACK (FILTER): Bastl's DJ filter on the loop, lowpass to the
 *   left, highpass to the right, off in the middle.
 * - SHIFT + FEEDBACK MOD (STEREO): the right channel read by a second head
 *   up to 150 ms behind the first.
 */
class AppLastingDamage : public virtual App
{
public:
    void Init();
    void DeInit();
    FASTCODE void AudioLoop(q15_t *input, q15_t *output, size_t size);
    void UiLoop();
    void MemoryInitialization() {}
    void MidiCallback(midi::Message *msg);

    /// Our ID, recorded in lab/docs/app-ids.md. Keys the app's saved settings.
    uint8_t GetId()
    {
        return 0x6F;
    }

private:
    static constexpr Hardware::AnalogInput CV_FREE = Hardware::AnalogInput::PITCH_1;
    static constexpr Hardware::AnalogInput CV_STEP = Hardware::AnalogInput::PITCH_2;
    static constexpr Hardware::AnalogInput CV_AMOUNT = Hardware::AnalogInput::PARAM_3;
    static constexpr Hardware::AnalogInput CV_FEEDBACK = Hardware::AnalogInput::PARAM_1;

    // About 1.09 s of stereo 16-bit tape: 48000 frames x 4 bytes = 192 kB,
    // most of the RP2040's 264 kB. FX Wizard's delays use about the same.
    static constexpr uint32_t kTapeFrames = 48000;
    static constexpr uint32_t kSeamFrames = 440; // 10 ms at 44 kHz

    // MODE button: shorter than this is a tap (record / close), longer
    // (up to kHoldMax) flips the direction. Counted in audio blocks (~1.1 ms).
    static constexpr uint32_t kTapUnder = s2alr(0.6f);
    static constexpr uint32_t kHoldMax = s2alr(3.0f);
    // How long the LEDs flash bright at the start of each pass.
    static constexpr uint32_t kPassFlash = s2alr(0.05f);
    // STEREO: the right head's offset at full turn, 150 ms. Squared, so the
    // first half of the knob is the few-ms widening range.
    static constexpr uint32_t kMaxSpreadFrames = 6600;
    // FILTER: FX Wizard's own curve for its DJ filter (AppFxWizard,
    // kMapDjFilter): lowpass to the left, a flat spot in the middle.
    static constexpr auto kMapFilter = MapDef<int32_t, 4>{
        {pot(0.0f), pot(0.49f), pot(0.51f), pot(1.0f)},
        {q15(-0.8f), q15(0.0f), q15(0.0f), q15(0.9f)}};

    enum class Pot
    {
        TIME,
        TIME_MOD,
        FEEDBACK,
        FEEDBACK_MOD,
        AMOUNT,
        AMOUNT_MOD,
        FILTER,
        STEREO,
        COUNT
    };

    bool inited_ = false;
    lasting_damage::TapeLoop tape_;
    EnumArray<Pot, std::unique_ptr<FancyPot>> pots_;
    EdgeDetector trigger_detect_ = EdgeDetector(EdgeDetector::Type::RISING);
    DjFilter filter_left_;
    DjFilter filter_right_;

    // Written by UiLoop, read by AudioLoop. Plain 32-bit values, which the
    // M0+ reads and writes in one go, so no locking is needed.
    volatile int32_t speed_q15_ = lasting_damage::TapeLoop::kUnity;
    volatile int32_t wear_q15_ = 0;
    volatile int32_t amount_q15_ = Q15_MAX;
    volatile uint32_t spread_frames_ = 0;
    volatile bool toggle_request_ = false;

    // Audio-side state shown on the LEDs.
    volatile uint32_t blocks_ = 0;
    volatile uint32_t passes_seen_ = 0;
    volatile uint32_t flash_until_ = 0;

    // UI state
    bool reverse_ = false;
    uint32_t mode_pressed_at_ = 0;
    bool mode_press_cancelled_ = false;

    void ToggleRecord();
    void UpdateLeds();
};

} // namespace kastle2
