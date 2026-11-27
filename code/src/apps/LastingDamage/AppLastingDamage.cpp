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

#include "AppLastingDamage.hpp"
#include <cmath>
#include "common/core/Kastle2.hpp"
#include "common/peripherals/WS2812.hpp"
#include "common/utils.hpp"

using namespace kastle2;

// The tape itself. A static array lives in RAM for the whole run, and the
// linker tells us at build time if it doesn't fit.
static int16_t s_tape[2 * 48000];

namespace
{
// Requests from the UI to the audio loop, counted rather than flagged so a
// request can't be lost between the audio loop reading and clearing it.
volatile uint32_t s_toggle_requests = 0;
uint32_t s_toggles_done = 0;
} // namespace

void AppLastingDamage::Init()
{
    inited_ = false;
    static_assert(sizeof(s_tape) == 2 * kTapeFrames * sizeof(int16_t), "tape size");

    // We produce the whole output ourselves (dry/wet included).
    Kastle2::base.SetFeatureEnabled(Base::Feature::AUDIO_CHAIN, false);

    tape_.Init(s_tape, kTapeFrames, kSeamFrames, 0x4C44u); // "LD"

    // Normal layer: the FX Wizard's printed labels.
    pots_[Pot::TIME_MOD] = FancyPot::Create(
        {.pot = Hardware::Pot::POT_1,
         .layer = Hardware::Layer::NORMAL,
         .initial_value = POT_HALF,
         .deadzone = true});
    pots_[Pot::FEEDBACK_MOD] = FancyPot::Create(
        {.pot = Hardware::Pot::POT_2,
         .layer = Hardware::Layer::NORMAL,
         .initial_value = POT_HALF,
         .deadzone = true});
    pots_[Pot::AMOUNT] = FancyPot::Create(
        {.pot = Hardware::Pot::POT_4,
         .layer = Hardware::Layer::NORMAL,
         .initial_value = POT_MAX});
    pots_[Pot::TIME] = FancyPot::Create(
        {.pot = Hardware::Pot::POT_5,
         .layer = Hardware::Layer::NORMAL,
         .initial_value = POT_HALF,
         .deadzone = true, // a little flat spot at 1x
         .freeze = true});
    pots_[Pot::FEEDBACK] = FancyPot::Create(
        {.pot = Hardware::Pot::POT_6,
         .layer = Hardware::Layer::NORMAL,
         .initial_value = POT_MIN});

    // Shift layer
    pots_[Pot::AMOUNT_MOD] = FancyPot::Create(
        {.pot = Hardware::Pot::POT_4,
         .layer = Hardware::Layer::SHIFT,
         .initial_value = POT_MAX});

    for (auto &pot : pots_)
    {
        pot->Init(AUDIO_LOOP_RATE);
    }

    reverse_ = false;
    mode_press_cancelled_ = false;
    s_toggles_done = s_toggle_requests;
    inited_ = true;
}

void AppLastingDamage::DeInit()
{
    inited_ = false;
}

// Runs inside the audio loop, so loop state only ever changes here.
void AppLastingDamage::ToggleRecord()
{
    switch (tape_.GetState())
    {
    case lasting_damage::TapeLoop::State::EMPTY:
    case lasting_damage::TapeLoop::State::LOOPING:
        tape_.Record();
        break;
    case lasting_damage::TapeLoop::State::RECORDING:
        tape_.Close();
        break;
    case lasting_damage::TapeLoop::State::CLOSING:
        break; // already closing, ignore
    }
}

FASTCODE void AppLastingDamage::AudioLoop(q15_t *input, q15_t *output, size_t size)
{
    if (!inited_)
    {
        return;
    }

    for (auto &pot : pots_)
    {
        pot->Process();
    }
    blocks_ = blocks_ + 1;

    // Record / close requests: MODE taps (from UiLoop) and TRIG IN rising edges.
    bool toggle = false;
    uint32_t requests = s_toggle_requests;
    if (requests != s_toggles_done)
    {
        s_toggles_done = requests;
        toggle = true;
    }
    if (trigger_detect_.Process(Kastle2::hw.GetTriggerIn()))
    {
        toggle = true;
    }
    if (toggle)
    {
        ToggleRecord();
    }

    tape_.SetSpeed(speed_q15_);
    tape_.SetWear(wear_q15_);
    const int32_t amount = amount_q15_;
    const uint32_t passes_before = tape_.GetPasses();

    for (size_t i = 0; i < size; i++)
    {
        int16_t dry_l = input[2 * i];
        int16_t dry_r = input[2 * i + 1];
        int16_t wet_l, wet_r;
        tape_.Process(dry_l, dry_r, &wet_l, &wet_r);
        // Dry/wet crossfade. (wet - dry) is at most 65535 and amount at most
        // 32768, so the product fits in 32 bits; amount 32768 is fully wet.
        output[2 * i] = static_cast<q15_t>(dry_l + (((wet_l - dry_l) * amount) >> 15));
        output[2 * i + 1] = static_cast<q15_t>(dry_r + (((wet_r - dry_r) * amount) >> 15));
    }

    if (tape_.GetPasses() != passes_before)
    {
        flash_until_ = blocks_ + kPassFlash;
    }
}

void AppLastingDamage::UiLoop()
{
    for (auto &pot : pots_)
    {
        pot->ReadValue();
    }

    // MODE button: a tap records / closes, a hold flips the direction.
    // Pressing SHIFT during the press (settings, or Bastl's own combos)
    // cancels it.
    if (Kastle2::hw.JustPressed(Hardware::Button::MODE))
    {
        mode_pressed_at_ = blocks_;
        mode_press_cancelled_ = false;
    }
    if (Kastle2::hw.Pressed(Hardware::Button::MODE) && Kastle2::hw.Pressed(Hardware::Button::SHIFT))
    {
        mode_press_cancelled_ = true;
    }
    if (Kastle2::hw.JustReleased(Hardware::Button::MODE) && !mode_press_cancelled_)
    {
        uint32_t held = blocks_ - mode_pressed_at_;
        if (held < kTapUnder)
        {
            s_toggle_requests = s_toggle_requests + 1;
        }
        else if (held < kHoldMax)
        {
            reverse_ = !reverse_;
        }
    }

    // TIME: half to double speed, exponential so each side is one octave,
    // 1x in the middle. TIME MOD scales the FREE and STEP CVs in, like the
    // FX Wizard. Float is fine here: this is the UI loop, not the audio path.
    int32_t time = pots_[Pot::TIME]->GetValue();
    int32_t time_mod = pots_[Pot::TIME_MOD]->GetValue();
    time += apply_pot_mod_attenuvert(Kastle2::hw.GetAnalogValue(CV_FREE), time_mod);
    time += apply_pot_mod_attenuvert(Kastle2::hw.GetAnalogValue(CV_STEP), time_mod);
    time = constrain(time, POT_MIN, POT_MAX);
    float octaves = static_cast<float>(time - POT_HALF) / static_cast<float>(POT_HALF);
    int32_t speed = static_cast<int32_t>(std::lround(std::exp2(octaves) * 32768.0f));
    speed_q15_ = reverse_ ? -speed : speed;

    // FEEDBACK: wear amount. 0-4095 becomes 0-32760 in Q15.
    int32_t feedback = pots_[Pot::FEEDBACK]->GetValue();
    feedback += apply_pot_mod_attenuvert(Kastle2::hw.GetAnalogValue(CV_FEEDBACK), pots_[Pot::FEEDBACK_MOD]->GetValue());
    feedback = constrain(feedback, POT_MIN, POT_MAX);
    wear_q15_ = feedback * 8;

    // AMOUNT: dry/wet, fully clockwise is exactly wet.
    int32_t amount = pots_[Pot::AMOUNT]->GetValue();
    amount += apply_pot_mod_attenuvert(Kastle2::hw.GetAnalogValue(CV_AMOUNT), pots_[Pot::AMOUNT_MOD]->GetValue());
    amount = constrain(amount, POT_MIN, POT_MAX);
    amount_q15_ = (amount >= POT_MAX) ? 32768 : amount * 8;

    UpdateLeds();
}

// Top LEDs, when no button layer owns them:
// - empty: dim white (the input passes straight through)
// - recording: red
// - looping: orange, or cyan when reversed, flashing at the start of each pass
void AppLastingDamage::UpdateLeds()
{
    Hardware::Layer layer = Kastle2::hw.GetLayer();
    if (layer != Hardware::Layer::NORMAL && layer != Hardware::Layer::MODE)
    {
        return;
    }
    uint32_t color = WS2812::NONE;
    switch (tape_.GetState())
    {
    case lasting_damage::TapeLoop::State::EMPTY:
        color = WS2812::ApplyBrightness(WS2812::WHITE, 60);
        break;
    case lasting_damage::TapeLoop::State::RECORDING:
    case lasting_damage::TapeLoop::State::CLOSING:
        color = WS2812::RED;
        break;
    case lasting_damage::TapeLoop::State::LOOPING:
    {
        uint32_t base = reverse_ ? WS2812::CYAN : WS2812::ORANGE;
        bool flash = static_cast<int32_t>(flash_until_ - blocks_) > 0;
        color = WS2812::ApplyBrightness(base, flash ? 255 : 70);
        break;
    }
    }
    Kastle2::hw.SetLed(Hardware::Led::LED_1, color);
    Kastle2::hw.SetLed(Hardware::Led::LED_2, color);
}

void AppLastingDamage::MidiCallback([[maybe_unused]] midi::Message *msg)
{
    // Nothing yet. Bastl's Base still handles MIDI clock and its own CCs.
}
