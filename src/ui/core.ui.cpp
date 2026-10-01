#include "core.ui.h"

#include <daisysp.h>
#include "common/common.h"

using namespace synthux::touchfx;
using namespace daisy;
using namespace daisysp;

namespace {
    // Fixed drive / bitcrush "flavors", selected by which pad is held.
    constexpr std::array<float, 3> kDriveLevels { .25f,  .4f,  .45f };
    constexpr std::array<float, 3> kCrushDown   { .3f,  .45f, .7f  };
    constexpr std::array<float, 3> kCrushBits   { .4f,  .5f,  .8f  };

    // Default post-volume for each flavor, used until the S35 knob is
    // moved while its pad is held (soft pickup). Without these, a fresh
    // pad touch mixes in 0% of the driven/crushed signal and the
    // dry->wet crossfade lands on silence instead of the dry signal.
    constexpr std::array<float, 3> kDriveMixDefaults { .8f, .1f, .1f };
    constexpr std::array<float, 3> kCrushMixDefaults { .7f, .8f, 1.f };

    // S34 with a crush pad held sets the bitcrush factor; downsample
    // follows it, keeping the flavor's downsample/bitcrush ratio.
    float crush_downsample(const uint8_t idx, const float bitcrush) {
        return fminf(bitcrush * kCrushDown[idx] / kCrushBits[idx], 1.f);
    }
}

CoreUI::CoreUI(Touch& touch, Core& core):
_touch              { touch },
_core               { core },
_drive_mix_index    { 0 },
_crush_mix_index    { 0 },
_drive_on           { false },
_crush_on           { false },
_bypass_on          { false },
_delay_bypassed     { false },
_reverb_bypassed    { false },
_is_latched         { false }
{}

void CoreUI::init()
{
    // Listen to knobs /////////////////////////////////////////
    ////////////////////////////////////////////////////////////
    _pot_monitor.Init(_ui_queue, _touch.knobs(), 500, 0.005f, 0.002f);

    // Assign callbacks ////////////////////////////////////////
    ////////////////////////////////////////////////////////////
    using namespace std::placeholders;
    auto on_touch = std::bind(&CoreUI::_on_pad_touch, this, _1);
    auto on_release = std::bind(&CoreUI::_on_pad_release, this, _1);
    _touch.pads().set_on_touch(on_touch);
    _touch.pads().set_on_release(on_release);

    for (auto i = 0; i < 3; i++) {
        _drive_mix[i].set(kDriveMixDefaults[i]);
        _crush_mix[i].set(kCrushMixDefaults[i]);
        _drive_amount[i].set(kDriveLevels[i]);
        _crush_amount[i].set(kCrushBits[i]);
    }

    _init_timer.Init();
};

void CoreUI::process()
{
    _touch.process();
    _process_ui_queue();

    // Latch //////////////////////////
    auto switch_a = _touch.switches().A();
    static auto was_latched = false;
    _is_latched = switch_a == 1;
    if (was_latched && !_is_latched && !_touched.any()) {
        _release_latched();
    }
    was_latched = _is_latched;

    // Reverb send / drive / crush amount macro (S34) ...............
    if (_apply.test(Knobs::s34)) {
        _core.set_reverb_send(_reverb_send.value());
        _core.set_drive_amount(_drive_amount[_drive_mix_index].value());
        auto bitcrush = _crush_amount[_crush_mix_index].value();
        _core.set_crush_amount(crush_downsample(_crush_mix_index, bitcrush), bitcrush);
    }

    // Reverb / drive / crush level macro (S35) ......................
    if (_apply.test(Knobs::s35)) {
        _core.set_reverb_amount(_reverb_mix.value());
        _core.set_drive_mix(_drive_mix[_drive_mix_index].value());
        _core.set_crush_mix(_crush_mix[_crush_mix_index].value());
    }

    _apply.reset();
};

void CoreUI::tick()
{
    // No MIDI / external clock for this instrument.
};

void CoreUI::_process_ui_queue()
{
    // Init the reverb send/level macros from their knobs so nothing jumps at boot.
    if (!_is_init && _init_timer.HasPassedMs(100)) {
        _reverb_send.set(_touch.knobs().GetPotValue(Knobs::s34));
        _reverb_mix.set(_touch.knobs().GetPotValue(Knobs::s35));
        _apply.set(Knobs::s34);
        _apply.set(Knobs::s35);
        _is_init = true;
    }

    _pot_monitor.Process();
    while(!_ui_queue.IsQueueEmpty()) {
        auto event = _ui_queue.GetAndRemoveNextEvent();
        if (event.type == UiEventQueue::Event::EventType::potMoved) {
            auto id = event.asPotMoved.id;
            auto val = event.asPotMoved.newPosition;
            val = infrasonic::map(val, .02f, .95f, 0.f, 1.f);
            _apply.set(id);
            switch (id) {
                case Knobs::s30:
                    _core.set_delay_amnt(val);
                    break;

                case Knobs::s31:
                    _core.set_delay_feedback(val);
                    break;

                case Knobs::s32:
                    _core.set_delay_mod_speed(val);
                    break;

                case Knobs::s33:
                    _core.set_delay_mod_amount(val);
                    break;

                case Knobs::s34: {
                    auto none_touched = !_touched.any();
                    _reverb_send.process(val, none_touched);
                    _drive_amount[0].process(val, _touched.test(Pads::TopLeft));
                    _drive_amount[1].process(val, _touched.test(Pads::CenterLeft));
                    _drive_amount[2].process(val, _touched.test(Pads::Left));
                    _crush_amount[0].process(val, _touched.test(Pads::TopRight));
                    _crush_amount[1].process(val, _touched.test(Pads::CenterRight));
                    _crush_amount[2].process(val, _touched.test(Pads::Right));
                    break;
                }

                case Knobs::s35: {
                    auto level = val * .8f;
                    auto none_touched = !_touched.any();
                    _reverb_mix.process(val, none_touched);
                    _drive_mix[0].process(level, _touched.test(Pads::TopLeft));
                    _drive_mix[1].process(level, _touched.test(Pads::Left));
                    _drive_mix[2].process(level, _touched.test(Pads::CenterLeft));
                    _crush_mix[0].process(level, _touched.test(Pads::TopRight));
                    _crush_mix[1].process(level, _touched.test(Pads::CenterRight));
                    _crush_mix[2].process(level, _touched.test(Pads::Right));
                    break;
                }

                case Knobs::s36:
                    _core.set_delay_time(val);
                    break;

                case Knobs::s37:
                    _core.set_reverb_feedback(val);
                    break;
            }
        }
    }
}

void CoreUI::_on_pad_touch(Pads::Pad pad)
{
    _touched.set(pad);
    switch (pad) {
        case Pads::TopLeft:     _engage_drive(0); break;
        case Pads::CenterLeft:  _engage_drive(1); break;
        case Pads::Left:        _engage_drive(2); break;

        case Pads::TopRight:    _engage_crush(0); break;
        case Pads::CenterRight: _engage_crush(1); break;
        case Pads::Right:       _engage_crush(2); break;

        case Pads::Center:      _toggle_bypass(); break;
        case Pads::BottomLeft:  _toggle_delay(); break;
        case Pads::BottomRight: _toggle_reverb(); break;

        default: break; // TopCenter, Tou, Ch: unused
    }
};

void CoreUI::_on_pad_release(Pads::Pad pad)
{
    _touched.reset(pad);
    if (_is_latched) return;

    switch (pad) {
        case Pads::TopLeft:
        case Pads::Left:
        case Pads::CenterLeft:
            _drive_on = false;
            _core.set_drive_on(false);
            break;

        case Pads::TopRight:
        case Pads::CenterRight:
        case Pads::Right:
            _crush_on = false;
            _core.set_crush_on(false);
            break;

        case Pads::Center:
            _bypass_on = false;
            _core.set_bypass_on(false);
            break;

        case Pads::BottomLeft:
            _delay_bypassed = false;
            _core.set_delay_bypass(false);
            break;

        case Pads::BottomRight:
            _reverb_bypassed = false;
            _core.set_reverb_bypass(false);
            break;

        default: break;
    }
};

void CoreUI::_engage_drive(const uint8_t idx)
{
    _drive_mix_index = idx;
    _core.set_drive_amount(_drive_amount[idx].value());
    _core.set_drive_mix(_drive_mix[idx].value());
    _drive_on = !(_is_latched && _drive_on);
    _core.set_drive_on(_drive_on);
}

void CoreUI::_engage_crush(const uint8_t idx)
{
    _crush_mix_index = idx;
    auto bitcrush = _crush_amount[idx].value();
    _core.set_crush_amount(crush_downsample(idx, bitcrush), bitcrush);
    _core.set_crush_mix(_crush_mix[idx].value());
    _crush_on = !(_is_latched && _crush_on);
    _core.set_crush_on(_crush_on);
}

void CoreUI::_toggle_bypass()
{
    _bypass_on = !(_is_latched && _bypass_on);
    _core.set_bypass_on(_bypass_on);
}

void CoreUI::_toggle_delay()
{
    _delay_bypassed = !(_is_latched && _delay_bypassed);
    _core.set_delay_bypass(_delay_bypassed);
}

void CoreUI::_toggle_reverb()
{
    _reverb_bypassed = !(_is_latched && _reverb_bypassed);
    _core.set_reverb_bypass(_reverb_bypassed);
}

void CoreUI::_release_latched()
{
    _drive_on = false;
    _core.set_drive_on(false);

    _crush_on = false;
    _core.set_crush_on(false);

    _bypass_on = false;
    _core.set_bypass_on(false);

    _delay_bypassed = false;
    _core.set_delay_bypass(false);

    _reverb_bypassed = false;
    _core.set_reverb_bypass(false);
}
