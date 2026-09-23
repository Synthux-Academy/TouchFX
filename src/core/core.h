#pragma once

#include <array>
#include <daisysp.h>

#include "synthux.h"
#include "nocopy.h"
#include "echo.h"
#include "level.h"

namespace synthux {
namespace touchfx {

class Core {
public:
  Core() = default;
  ~Core() = default;

  void init(const float sample_rate, const float cb_buffer_size);
  void process(const float* const* in, float** out, size_t size);

  // Drive (touch pads select amount, softswitch engages/disengages) ////
  void set_drive_amount(const float amount);
  void set_drive_on(const bool on);
  void set_drive_mix(const float norm);

  // Bitcrush /////////////////////////////////////////////////////////
  void set_crush_amount(const float downsample, const float bitcrush);
  void set_crush_on(const bool on);
  void set_crush_mix(const float norm);

  // Delay is an always-on send; the pad mutes/unmutes it (bypass) //////
  void set_delay_bypass(const bool bypass);
  void set_delay_mix(const float norm);
  void set_delay_feedback(const float norm);
  void set_delay_time(const float norm);
  void set_delay_mod_speed(const float norm);
  void set_delay_mod_amount(const float norm);

  // Reverb is an always-on send; the pad mutes/unmutes it (bypass) ////
  void set_reverb_bypass(const bool bypass);
  void set_reverb_mix(const float norm);
  void set_reverb_send(const float norm);
  void set_reverb_feedback(const float norm);

  // Master bypass ////////////////////////////////////////////////////
  void set_bypass_on(const bool on);

private:
  NOCOPY(Core)

  static constexpr uint32_t kDelayLengthSec = 5;
  static constexpr uint32_t kSampleRate = 48000;
  static constexpr size_t kDelayLengthSamples = kDelayLengthSec * kSampleRate;

  synthux::Level _level;
  std::array<daisysp::Overdrive, 2> _drive;
  std::array<daisysp::Decimator, 2> _crush;
  std::array<infrasonic::EchoDelay<kDelayLengthSamples>*, 2> _delay;
  daisysp::ReverbSc* _reverb;
  daisysp::Oscillator _delay_lfo;

  SoftSwitch _drive_on;
  SoftSwitch _crush_on;
  SoftSwitch _delay_bypass;
  SoftSwitch _reverb_bypass;
  SoftSwitch _master_bypass;

  XFade _drive_fade;
  XFade _crush_fade;
  XFade _delay_fade;
  XFade _reverb_fade;
  XFade _bypass_fade;

  float _drive_mix { 0.f };
  float _crush_mix { 0.f };

  float _delay_mix { 0.f };
  float _delay_time { 0.f };

  float _reverb_send { 0.f };
  float _reverb_mix { 0.f };
};

};
};
