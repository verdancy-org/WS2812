#pragma once

// clang-format off
/* === MODULE MANIFEST V2 ===
module_description: 通过 SPI 驱动 WS2812 灯带的模块 / WS2812 LED strip driver over SPI
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

#include "app_framework.hpp"
#include "libxr.hpp"
#include "spi.hpp"

/**
 * @brief 通过 SPI 波形编码驱动 WS2812 灯带 / Drive a WS2812 LED strip by SPI waveform encoding
 *
 * @tparam MaxLeds 最大灯珠数量，也是像素缓冲区容量 / Maximum LED count and pixel buffer capacity
 * @tparam ResetBytes 帧前后 reset 低电平字节数 / Reset low-level bytes before and after a frame
 */
template <std::size_t MaxLeds = 16, std::size_t ResetBytes = 304>
class WS2812 : public LibXR::Application
{
 public:
  /// 每颗灯珠编码后的 SPI 字节数 / Encoded SPI bytes per LED
  static constexpr std::size_t kBytesPerLed = 24;

  /// 完整发送帧长度，包含前后 reset 区 / Full frame size including reset bytes
  static constexpr std::size_t kFrameBytes = (MaxLeds * kBytesPerLed) + (ResetBytes * 2);

  /// RGB 颜色值，发送时转换为 GRB 顺序 / RGB color, converted to GRB when sent
  struct Color
  {
    std::uint8_t r;
    std::uint8_t g;
    std::uint8_t b;
  };

  /**
   * @brief 构造 WS2812 模块并注册到 XRobot ApplicationManager / Construct and register the WS2812 module
   *
   * @param hw LibXR 硬件容器 / LibXR hardware container
   * @param app XRobot 应用管理器 / XRobot application manager
   * @param spi_alias SPI 设备别名，例如 `ws2812_spi` / SPI alias, for example `ws2812_spi`
   * @param led_count 实际灯珠数量，超过 MaxLeds 会被裁剪 / Active LED count, clipped by MaxLeds
   * @param brightness 初始全局亮度，范围 0-255 / Initial global brightness, range 0-255
   * @param demo_period_ms 彩虹 demo 周期，单位 ms；0 表示关闭 / Rainbow demo period in ms; 0 disables it
   */
  WS2812(LibXR::HardwareContainer& hw, LibXR::ApplicationManager& app,
         const char* spi_alias, std::uint16_t led_count, std::uint8_t brightness,
         std::uint32_t demo_period_ms)
      : spi_(hw.template FindOrExit<LibXR::SPI>({spi_alias})),
        led_count_(ClipLedCount(led_count)),
        brightness_(brightness),
        demo_period_ms_(demo_period_ms)
  {
    Clear();
    app.Register(*this);
  }

  /// XRobot 周期调度入口，用于运行内置彩虹 demo / Periodic hook for the built-in rainbow demo
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
      RenderDemo();
      (void)Show();
    }
  }

  /// 获取当前有效灯珠数量 / Get active LED count
  [[nodiscard]] std::uint16_t Size() const { return led_count_; }

  /// 设置全局亮度，下一次 Show() 生效 / Set global brightness for the next Show()
  void SetBrightness(std::uint8_t brightness) { brightness_ = brightness; }

  /**
   * @brief 设置单颗灯珠颜色，不会立即发送 / Set one LED color without sending immediately
   *
   * @param index 灯珠序号，越界会被忽略 / LED index, ignored when out of range
   * @param red 红色分量 / Red channel
   * @param green 绿色分量 / Green channel
   * @param blue 蓝色分量 / Blue channel
   */
  void SetPixel(std::uint16_t index, std::uint8_t red, std::uint8_t green,
                std::uint8_t blue)
  {
    if (index >= led_count_)
    {
      return;
    }

    pixels_[index] = Color{red, green, blue};
  }

  /// 填充所有有效灯珠颜色，不会立即发送 / Fill all active LEDs without sending immediately
  void Fill(std::uint8_t red, std::uint8_t green, std::uint8_t blue)
  {
    for (std::uint16_t i = 0; i < led_count_; ++i)
    {
      SetPixel(i, red, green, blue);
    }
  }

  /// 熄灭所有灯珠并立即发送 / Turn all LEDs off and send immediately
  void Clear()
  {
    Fill(0U, 0U, 0U);
    (void)Show();
  }

  /**
   * @brief 编码当前像素缓冲区并通过 SPI 发送 / Encode current pixels and send over SPI
   *
   * @return LibXR 错误码 / LibXR error code
   */
  LibXR::ErrorCode Show()
  {
    const std::size_t frame_size = EncodeFrame();
    LibXR::Semaphore sem;
    LibXR::SPI::OperationRW op(sem, kSpiWriteTimeoutMs);
    return spi_->Write(LibXR::ConstRawData(frame_.data(), frame_size), op);
  }

 private:
  /// WS2812 bit 0 的 SPI 符号 / SPI symbol for WS2812 bit 0
  static constexpr std::uint8_t kSymbol0 = 0xC0U;

  /// WS2812 bit 1 的 SPI 符号 / SPI symbol for WS2812 bit 1
  static constexpr std::uint8_t kSymbol1 = 0xF8U;

  /// 单帧 SPI 写超时时间 / SPI write timeout for one frame
  static constexpr std::uint32_t kSpiWriteTimeoutMs = 20U;

  /// 将实际灯珠数量限制在缓冲区容量内 / Clip active LED count to buffer capacity
  static constexpr std::uint16_t ClipLedCount(std::uint16_t led_count)
  {
    return (led_count > MaxLeds) ? static_cast<std::uint16_t>(MaxLeds) : led_count;
  }

  /// 应用全局亮度缩放 / Apply global brightness scaling
  static std::uint8_t ApplyBrightness(std::uint8_t value, std::uint8_t brightness)
  {
    return static_cast<std::uint8_t>((static_cast<std::uint16_t>(value) * brightness) /
                                     255U);
  }

  /// 根据色轮位置生成彩虹 demo 颜色 / Generate rainbow demo color from wheel position
  static Color ColorWheel(std::uint8_t pos)
  {
    if (pos < 85U)
    {
      return Color{static_cast<std::uint8_t>(255U - pos * 3U),
                   static_cast<std::uint8_t>(pos * 3U), 0U};
    }
    if (pos < 170U)
    {
      pos = static_cast<std::uint8_t>(pos - 85U);
      return Color{0U, static_cast<std::uint8_t>(255U - pos * 3U),
                   static_cast<std::uint8_t>(pos * 3U)};
    }

    pos = static_cast<std::uint8_t>(pos - 170U);
    return Color{static_cast<std::uint8_t>(pos * 3U), 0U,
                 static_cast<std::uint8_t>(255U - pos * 3U)};
  }

  /// 将 1 个颜色字节编码为 8 个 SPI 符号字节 / Encode one color byte into eight SPI symbols
  void EncodeByte(std::size_t& offset, std::uint8_t value)
  {
    for (std::uint8_t mask = 0x80U; mask != 0U; mask >>= 1U)
    {
      frame_[offset++] = ((value & mask) != 0U) ? kSymbol1 : kSymbol0;
    }
  }

  /// 编码完整 WS2812 帧：reset + GRB 数据 + reset / Encode full frame: reset + GRB data + reset
  std::size_t EncodeFrame()
  {
    std::size_t offset = 0;
    for (std::size_t i = 0; i < ResetBytes; ++i)
    {
      frame_[offset++] = 0U;
    }

    for (std::uint16_t i = 0; i < led_count_; ++i)
    {
      const Color color = pixels_[i];
      EncodeByte(offset, ApplyBrightness(color.g, brightness_));
      EncodeByte(offset, ApplyBrightness(color.r, brightness_));
      EncodeByte(offset, ApplyBrightness(color.b, brightness_));
    }

    for (std::size_t i = 0; i < ResetBytes; ++i)
    {
      frame_[offset++] = 0U;
    }

    return offset;
  }

  /// 渲染一帧内置彩虹 demo / Render one built-in rainbow demo frame
  void RenderDemo()
  {
    for (std::uint16_t i = 0; i < led_count_; ++i)
    {
      const auto wheel =
          static_cast<std::uint8_t>((static_cast<std::uint16_t>(i) * 256U /
                                     (led_count_ == 0U ? 1U : led_count_)) +
                                    demo_phase_);
      pixels_[i] = ColorWheel(wheel);
    }
    ++demo_phase_;
  }

  LibXR::SPI* spi_;
  std::uint16_t led_count_;
  std::uint8_t brightness_;
  std::uint32_t demo_period_ms_;
  bool demo_started_ = false;
  std::uint32_t last_demo_ms_ = 0U;
  std::uint8_t demo_phase_ = 0U;
  std::array<Color, MaxLeds> pixels_{};
  std::array<std::uint8_t, kFrameBytes> frame_{};
};
