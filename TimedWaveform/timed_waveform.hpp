#pragma once

#include <cstddef>
#include <cstdint>

#include "libxr.hpp"

namespace LibXR {

/**
 * @brief Output a finite sequence of timer compare samples.
 *
 * This interface is kept with the WS2812 module because it currently exists to
 * support PWM-style LED transports. It still hides whether the backend uses
 * DMA, FIFO, interrupts, or polling.
 */
class TimedWaveform {
public:
  struct CompareStream {
    const std::uint32_t *samples;
    std::size_t count;
    std::uint32_t period_ticks;
    std::uint32_t idle_compare;
  };

  virtual ErrorCode Send(const CompareStream &stream,
                         std::uint32_t timeout_ms) = 0;
  virtual bool Busy() const = 0;
  virtual void Stop() = 0;
};

} // namespace LibXR
