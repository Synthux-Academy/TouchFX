#pragma once

#include <array>
#include <bitset>
#include <daisy_seed.h>

#include "hw/touch.h"
#include "core/core.h"

#include "common/mvalue.h"

namespace synthux {
namespace touchfx {

class CoreUI {
public:
    CoreUI(Touch&, Core&);
    ~CoreUI() = default;

    void init();
    void process();

    void tick();

private:
    Touch& _touch;
    Core& _core;

    // Reverb / drive / crush all share one "level" macro knob (S35).
    // Each tracks it independently, catching the knob only while its
    // own pad (or, for reverb, no pad at all) is held.
    MValue _reverb_mix;
    std::array<MValue, 3> _drive_mix;
    std::array<MValue, 3> _crush_mix;

    // Reverb send / drive amount / crush amount share S34, with the
    // same pad-gated soft pickup as the S35 level macro.
    MValue _reverb_send;
    std::array<MValue, 3> _drive_amount;
    std::array<MValue, 3> _crush_amount;

    uint8_t _drive_mix_index;
    uint8_t _crush_mix_index;

    std::bitset<Knobs::Count> _apply;

    daisy::UiEventQueue _ui_queue;
    daisy::PotMonitor<Knobs, Knobs::Count> _pot_monitor;
    void _process_ui_queue();

    daisy::StopwatchTimer _init_timer;

    std::bitset<Pads::Count> _touched;
    void _on_pad_touch(Pads::Pad);
    void _on_pad_release(Pads::Pad);

    void _engage_drive(const uint8_t idx);
    void _engage_crush(const uint8_t idx);
    void _toggle_bypass();
    void _toggle_delay();
    void _toggle_reverb();
    void _release_latched();

    bool _drive_on;
    bool _crush_on;
    bool _bypass_on;
    bool _delay_bypassed;
    bool _reverb_bypassed;

    bool _is_latched;
    bool _is_init;
};

}; // namespace touchfx
}; // namespace synthux
