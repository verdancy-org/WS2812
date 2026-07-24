#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

/**
 * @brief Shared WS2812 pixel buffer, brightness scaling, and demo rendering logic.
 *
 * This class deliberately contains no XRobot module manifest and no hardware I/O.
 * Concrete transport modules such as WS2812SPI and WS2812PWM inherit it and provide
 * their own Show() implementation.
 *
 * @tparam MaxLeds Maximum LED count and pixel buffer capacity.
 */
template <std::size_t MaxLeds>
class WS2812
{
 public:
  struct Color
  {
    std::uint8_t r;
    std::uint8_t g;
    std::uint8_t b;
  };

  explicit WS2812(std::uint16_t led_count, std::uint8_t brightness)
      : led_count_(ClipLedCount(led_count)), brightness_(brightness)
  {
  }

  [[nodiscard]] std::uint16_t Size() const { return led_count_; }

  void SetBrightness(std::uint8_t brightness) { brightness_ = brightness; }

  [[nodiscard]] std::uint8_t GetBrightness() const { return brightness_; }

  void SetPixel(std::uint16_t index, std::uint8_t red, std::uint8_t green,
                std::uint8_t blue)
  {
    if (index >= led_count_)
    {
      return;
    }

    pixels_[index] = Color{red, green, blue};
  }

  void Fill(std::uint8_t red, std::uint8_t green, std::uint8_t blue)
  {
    for (std::uint16_t i = 0; i < led_count_; ++i)
    {
      SetPixel(i, red, green, blue);
    }
  }

  void ClearPixels() { Fill(0U, 0U, 0U); }

 protected:
  static constexpr std::size_t kBitsPerLed = 24U;

  static constexpr std::uint16_t ClipLedCount(std::uint16_t led_count)
  {
    return (led_count > MaxLeds) ? static_cast<std::uint16_t>(MaxLeds) : led_count;
  }

  static std::uint8_t Scale(std::uint8_t value, std::uint8_t brightness)
  {
    return static_cast<std::uint8_t>(
        (static_cast<std::uint16_t>(value) * brightness) / 255U);
  }

  static Color Wheel(std::uint8_t pos)
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

  template <typename Callback>
  void ForEachScaledGrbByte(Callback&& callback) const
  {
    for (std::uint16_t i = 0; i < led_count_; ++i)
    {
      const Color color = pixels_[i];
      callback(Scale(color.g, brightness_));
      callback(Scale(color.r, brightness_));
      callback(Scale(color.b, brightness_));
    }
  }

  void RenderDemoFrame()
  {
    for (std::uint16_t i = 0; i < led_count_; ++i)
    {
      const auto wheel =
          static_cast<std::uint8_t>((static_cast<std::uint16_t>(i) * 256U /
                                     (led_count_ == 0U ? 1U : led_count_)) +
                                    demo_phase_);
      pixels_[i] = Wheel(wheel);
    }
    ++demo_phase_;
  }

  std::uint16_t led_count_;
  std::uint8_t brightness_;
  std::uint8_t demo_phase_ = 0U;
  std::array<Color, MaxLeds> pixels_{};
};
