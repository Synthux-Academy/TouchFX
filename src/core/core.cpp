#include "core.h"

#include <algorithm>
#include <daisysp.h>
#include "sdram_alloc.h"
#include "common.h"
#include "expose.h"

using namespace synthux::touchfx;
using namespace daisysp;

void Core::init(const float sample_rate, const float cb_rate)
{
    (void)cb_rate;

    for (auto& d: _drive) d.Init();
    _drive_mix.set_stage(1.f);

    for (auto& c: _crush) {
        c.Init();
        c.SetDownsampleFactor(1.0f);
    }
    _crush_mix.set_stage(1.f);

    for (auto& d: _delay) {
        d = infrasonic::SDRAM::allocate<infrasonic::EchoDelay<kDelayLengthSamples>>();
        d->Init(sample_rate);
        d->SetLagTime(.3f);
        d->SetFeedback(.6f);
    }

    _level.init(sample_rate);

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
    |-------------------------------------- Master bypass --|--> out
*/
void Core::process(const float* const* in, float** out, size_t size)
{
    for (size_t i = 0; i < size; i++) {
        auto mod = 0.25f * _delay_lfo.Process();
        auto delay_time = std::max(_delay_time + mod, 0.f);
        _delay[0]->SetDelayTime(delay_time);
        _delay[1]->SetDelayTime(delay_time);

        auto bus0 = in[0][i];
        auto bus1 = in[1][i];

        // Drive
        auto drive0 = bus0, drive1 = bus1;
        _level.pre_gain(drive0, drive1);
        drive0 = _drive[0].Process(drive0);
        drive1 = _drive[1].Process(drive1);
        _level.post_gain(drive0, drive1);
        
        _drive_mix.process(bus0, bus1, drive0, drive1, drive0, drive1);
        auto drive_on = _drive_on.process();
        auto drive_off = 1.f - drive_on;
        bus0 = bus0 * drive_off + drive0 * drive_on;
        bus1 = bus1 * drive_off + drive1 * drive_on;

        // Bitcrush
        auto crush0 = bus0, crush1 = bus1;
        _crush_mix.process(bus0, bus1, _crush[0].Process(crush0), _crush[1].Process(crush1), crush0, crush1);
        auto crush_on = _crush_on.process();
        auto crush_off = 1.f - crush_on;
        bus0 = bus0 * crush_off + crush0 * crush_on;
        bus1 = bus1 * crush_off + crush1 * crush_on;

        // Delay send
        auto delay_send = _delay_bypass.process(true) * _delay_amount;
        auto delay_in0 = bus0 * delay_send;
        auto delay_in1 = bus1 * delay_send;
        _delay_mix.set_stage(delay_send);
        _delay_mix.process(bus0, bus1,
            _delay[0]->Process(delay_in0),
            _delay[1]->Process(delay_in1),
            bus0, bus1);

        // Reverb send
        auto reverb_gate = _reverb_bypass.process(true) * _reverb_send;
        auto reverb_in0 = bus0 * reverb_gate * _reverb_amount;
        auto reverb_in1 = bus1 * reverb_gate * _reverb_amount;
        float reverb_out0, reverb_out1;
        _reverb->Process(reverb_in0, reverb_in1, &reverb_out0, &reverb_out1);
        _reverb_mix.set_stage(reverb_gate);
        _reverb_mix.process(bus0, bus1, reverb_out0, reverb_out1, bus0, bus1);

        // Master bypass
        _bypass_mix.set_stage(_master_bypass.process());
        _bypass_mix.process(bus0, bus1, in[0][i], in[1][i], bus0, bus1);

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
    _drive_mix.set_stage(norm);
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
    _crush_mix.set_stage(norm);
}

void Core::set_delay_bypass(const bool bypass)
{
    _delay_bypass.set_on(bypass);
}
void Core::set_delay_amnt(const float norm)
{
    _delay_amount = norm;
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
void Core::set_reverb_send(const float norm)
{
    _reverb_send = norm;
}
void Core::set_reverb_amount(const float norm)
{
    _reverb_amount = norm;
}
void Core::set_reverb_feedback(const float norm)
{
    _reverb->SetFeedback(fmap(norm, 0.3f, 1.f));
}

void Core::set_bypass_on(const bool on)
{
    _master_bypass.set_on(on);
}
