#pragma once

// clang-format off
/* === MODULE MANIFEST V2 ===
module_description: LCKFB TQX MSPM0G3519 WS2812 driver using SysConfig TIMA1 PWM and DMA CH0
constructor_args:
  - waveform_alias: ws2812_waveform
  - led_count: 4
  - brightness: 32
  - demo_period_ms: 40
template_args:
  - MaxLeds: 4
required_hardware:
  - timed_waveform
depends: []
=== END MANIFEST === */
// clang-format on

#include <array>
#include <cstddef>
#include <cstdint>

#include "TimedWaveform.hpp"
#include "WS2812.hpp"
#include "app_framework.hpp"
#include "libxr.hpp"

/**
 * @brief WS2812 backend using a TimedWaveform compare stream.
 */
template <std::size_t MaxLeds = 4>
class WS2812PWM : public LibXR::Application, public WS2812<MaxLeds>
{
 public:
  WS2812PWM(LibXR::HardwareContainer& hw, LibXR::ApplicationManager& app,
            const char* waveform_alias, std::uint16_t led_count,
            std::uint8_t brightness, std::uint32_t demo_period_ms)
      : WS2812<MaxLeds>(led_count, brightness),
        waveform_(hw.template FindOrExit<LibXR::TimedWaveform>({waveform_alias})),
        demo_period_ms_(demo_period_ms)
  {
    Clear();
    app.Register(*this);
  }

  WS2812PWM(LibXR::HardwareContainer& hw, LibXR::ApplicationManager& app,
            std::uint16_t led_count, std::uint8_t brightness,
            std::uint32_t demo_period_ms)
      : WS2812PWM(hw, app, "ws2812_waveform", led_count, brightness,
                  demo_period_ms)
  {
  }

  void OnMonitor() override
  {
    if (demo_period_ms_ == 0U)
    {
      return;
    }

    const auto now = static_cast<std::uint32_t>(LibXR::Timebase::GetMilliseconds());
    if (!demo_started_ || (now - last_demo_ms_) >= demo_period_ms_)
    {
      demo_started_ = true;
      last_demo_ms_ = now;
      this->RenderDemoFrame();
      (void)Show();
    }
  }

  void Clear()
  {
    this->ClearPixels();
    (void)Show();
  }

  LibXR::ErrorCode Show()
  {
    Encode();
    return waveform_->Send(
        LibXR::TimedWaveform::CompareStream{wave_.data(), wave_.size(),
                                            kPeriodTicks, kIdleCompare},
        kSendTimeoutMs);
  }

 private:
  static constexpr std::uint32_t kPeriodTicks = 100U;
  static constexpr std::uint32_t kT0hTicks = 30U;
  static constexpr std::uint32_t kT1hTicks = 60U;
  static constexpr std::uint32_t kIdleCompare = 1U;
  static constexpr std::uint32_t kSendTimeoutMs = 20U;
  static constexpr std::size_t kResetSlots = 50U;
  static constexpr std::size_t kWaveSlots =
      MaxLeds * WS2812<MaxLeds>::kBitsPerLed + kResetSlots;

  void EncodeColor(std::uint8_t value, std::size_t& offset)
  {
    for (std::uint8_t mask = 0x80U; mask != 0U; mask >>= 1U)
    {
      wave_[offset++] = ((value & mask) != 0U) ? kT1hTicks : kT0hTicks;
    }
  }

  void Encode()
  {
    std::size_t offset = 0;
    this->ForEachScaledGrbByte(
        [this, &offset](std::uint8_t value) { EncodeColor(value, offset); });

    while (offset < wave_.size())
    {
      wave_[offset++] = kIdleCompare;
    }
  }

  LibXR::TimedWaveform* waveform_;
  std::uint32_t demo_period_ms_;
  bool demo_started_ = false;
  std::uint32_t last_demo_ms_ = 0U;
  std::array<std::uint32_t, kWaveSlots> wave_{};
};
