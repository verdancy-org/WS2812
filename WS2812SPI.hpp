#pragma once

// clang-format off
/* === MODULE MANIFEST V2 ===
module_description: WS2812 LED strip driver over SPI waveform encoding
constructor_args:
  - spi_alias: ws2812_spi
  - led_count: 16
  - brightness: 32
  - demo_period_ms: 40
template_args:
  - MaxLeds: 16
  - ResetBytes: 304
required_hardware:
  - spi
depends: []
=== END MANIFEST === */
// clang-format on

#include <array>
#include <cstddef>
#include <cstdint>

#include "WS2812.hpp"
#include "app_framework.hpp"
#include "libxr.hpp"
#include "spi.hpp"

/**
 * @brief SPI-backed WS2812 module using 0xC0/0xF8 symbol encoding.
 *
 * @tparam MaxLeds Maximum LED count and pixel buffer capacity.
 * @tparam ResetBytes Reset low-level bytes before and after a frame.
 */
template <std::size_t MaxLeds = 16, std::size_t ResetBytes = 304>
class WS2812SPI : public LibXR::Application, public WS2812<MaxLeds>
{
 public:
  static constexpr std::size_t BYTES_PER_LED = WS2812<MaxLeds>::BITS_PER_LED;
  static constexpr std::size_t FRAME_BYTES =
      (MaxLeds * BYTES_PER_LED) + (ResetBytes * 2U);

  WS2812SPI(LibXR::HardwareContainer& hw, LibXR::ApplicationManager& app,
            const char* spi_alias, std::uint16_t led_count, std::uint8_t brightness,
            std::uint32_t demo_period_ms)
      : WS2812<MaxLeds>(led_count, brightness),
        spi_(hw.template FindOrExit<LibXR::SPI>({spi_alias})),
        demo_period_ms_(demo_period_ms)
  {
    Clear();
    app.Register(*this);
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
    const std::size_t frame_size = EncodeFrame();
    LibXR::Semaphore sem;
    LibXR::SPI::OperationRW op(sem, SPI_WRITE_TIMEOUT_MS);
    return spi_->Write(LibXR::ConstRawData(frame_.data(), frame_size), op);
  }

 private:
  static constexpr std::uint8_t SYMBOL_0 = 0xC0U;
  static constexpr std::uint8_t SYMBOL_1 = 0xF8U;
  static constexpr std::uint32_t SPI_WRITE_TIMEOUT_MS = 20U;

  void EncodeByte(std::size_t& offset, std::uint8_t value)
  {
    for (std::uint8_t mask = 0x80U; mask != 0U; mask >>= 1U)
    {
      frame_[offset++] = ((value & mask) != 0U) ? SYMBOL_1 : SYMBOL_0;
    }
  }

  std::size_t EncodeFrame()
  {
    std::size_t offset = 0;
    for (std::size_t i = 0; i < ResetBytes; ++i)
    {
      frame_[offset++] = 0U;
    }

    this->ForEachScaledGrbByte(
        [this, &offset](std::uint8_t value) { EncodeByte(offset, value); });

    for (std::size_t i = 0; i < ResetBytes; ++i)
    {
      frame_[offset++] = 0U;
    }

    return offset;
  }

  LibXR::SPI* spi_;
  std::uint32_t demo_period_ms_;
  bool demo_started_ = false;
  std::uint32_t last_demo_ms_ = 0U;
  std::array<std::uint8_t, FRAME_BYTES> frame_{};
};
