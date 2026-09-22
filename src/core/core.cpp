#include "core.h"

#include <algorithm>
#include <daisysp.h>
#include "sdram_alloc.h"
#include "common.h"

using namespace synthux::touchfx;
using namespace daisysp;

void Core::init(const float sample_rate, const float cb_rate)
{
    (void)cb_rate;

    for (auto& d: _drive) d.Init();
    _drive_fade.set_stage(1.f);

    for (auto& c: _crush) {
        c.Init();
        c.SetDownsampleFactor(1.0f);
    }
    _crush_fade.set_stage(1.f);

    for (auto& d: _delay) {
        d = infrasonic::SDRAM::allocate<infrasonic::EchoDelay<kDelayLengthSamples>>();
        d->Init(sample_rate);
        d->SetLagTime(.3f);
        d->SetFeedback(.6f);
    }

    _delay_lfo.Init(sample_rate);
    _delay_lfo.SetWaveform(daisysp::Oscillator::WAVE_TRI);

    _reverb = infrasonic::SDRAM::allocate<ReverbSc>();
    _reverb->Init(sample_rate);
    _reverb->SetLpFreq(8000.f);

    _drive_on.init(sample_rate);
    _crush_on.init(sample_rate);
    _delay_bypass.init(sample_rate);
    _reverb_bypass.init(sample_rate);
    _master_bypass.init(sample_rate);

    set_drive_amount(.36f);
    set_crush_amount(.0f, .86f);
};

/* Signal path .............................................................
   Drive and bitcrush are momentary additions, off until a pad engages them.
   Delay and reverb are always-on sends (per their mix knobs); their pads
   mute/unmute (bypass) them instead of turning them on.

   in >-- Drive --- Crush --- Delay send --- Reverb send --|
    |                                                       |
    |------------------------------------- Master bypass ---|--> out
*/
void Core::process(const float* const* in, float** out, size_t size)
{
    for (size_t i = 0; i < size; i++) {
        auto mod = 0.25f * _delay_lfo.Process();
        auto delay_time = std::max(_delay_time + mod, 0.f);
        _delay[0]->SetDelayTime(delay_time);
        _delay[1]->SetDelayTime(delay_time);

        auto in0 = in[0][i];
        auto in1 = in[1][i];
        auto bus0 = in0;
        auto bus1 = in1;

        // Drive
        _drive_fade.set_stage(_drive_on.process());
        _drive_fade.process(bus0, bus1,
            _drive[0].Process(bus0) * _drive_mix,
            _drive[1].Process(bus1) * _drive_mix,
            bus0, bus1);

        // Bitcrush
        _crush_fade.set_stage(_crush_on.process());
        _crush_fade.process(bus0, bus1,
            _crush[0].Process(bus0) * _crush_mix,
            _crush[1].Process(bus1) * _crush_mix,
            bus0, bus1);

        // Delay send
        auto delay_send = _delay_bypass.process(true) * _delay_mix;
        auto delay_in0 = bus0 * delay_send;
        auto delay_in1 = bus1 * delay_send;
        _delay_fade.set_stage(delay_send);
        _delay_fade.process(bus0, bus1,
            _delay[0]->Process(delay_in0),
            _delay[1]->Process(delay_in1),
            bus0, bus1);

        // Reverb send
        auto reverb_gate = _reverb_bypass.process(true) * _reverb_mix;
        auto reverb_in0 = bus0 * reverb_gate * _reverb_send;
        auto reverb_in1 = bus1 * reverb_gate * _reverb_send;
        float reverb_out0, reverb_out1;
        _reverb->Process(reverb_in0, reverb_in1, &reverb_out0, &reverb_out1);
        _reverb_fade.set_stage(reverb_gate);
        _reverb_fade.process(bus0, bus1, reverb_out0, reverb_out1, bus0, bus1);

        // Master bypass
        _bypass_fade.set_stage(_master_bypass.process());
        _bypass_fade.process(bus0, bus1, in0, in1, bus0, bus1);

        out[0][i] = SoftLimit(bus0);
        out[1][i] = SoftLimit(bus1);
    }
};

void Core::set_drive_amount(const float amount)
{
    _drive[0].SetDrive(amount);
    _drive[1].SetDrive(amount);
}

void Core::set_drive_on(const bool on)
{
    _drive_on.set_on(on);
}

void Core::set_drive_mix(const float norm)
{
    _drive_mix = norm;
}

void Core::set_crush_amount(const float downsample, const float bitcrush)
{
    for (auto& c: _crush) {
        c.SetDownsampleFactor(downsample);
        c.SetBitcrushFactor(bitcrush);
    }
}

void Core::set_crush_on(const bool on)
{
    _crush_on.set_on(on);
}

void Core::set_crush_mix(const float norm)
{
    _crush_mix = norm;
}

void Core::set_delay_bypass(const bool bypass)
{
    _delay_bypass.set_on(bypass);
}

void Core::set_delay_mix(const float norm)
{
    _delay_mix = norm;
}

void Core::set_delay_feedback(const float norm)
{
    auto fb = norm * 1.02f;
    _delay[0]->SetFeedback(fb);
    _delay[1]->SetFeedback(fb);
}

void Core::set_delay_time(const float norm)
{
    _delay_time = fmap(norm, 0.01f, 5.f, Mapping::EXP);
}

void Core::set_delay_mod_speed(const float norm)
{
    _delay_lfo.SetFreq(fmap(norm, 0.05f, 10.f));
}

void Core::set_delay_mod_amount(const float norm)
{
    _delay_lfo.SetAmp(fmap(norm, 0.f, 0.05f));
}

void Core::set_reverb_bypass(const bool bypass)
{
    _reverb_bypass.set_on(bypass);
}

void Core::set_reverb_mix(const float norm)
{
    _reverb_mix = norm;
}

void Core::set_reverb_send(const float norm)
{
    _reverb_send = norm;
}

void Core::set_reverb_feedback(const float norm)
{
    _reverb->SetFeedback(fmap(norm, 0.3f, 1.f));
}

void Core::set_bypass_on(const bool on)
{
    _master_bypass.set_on(on);
}
