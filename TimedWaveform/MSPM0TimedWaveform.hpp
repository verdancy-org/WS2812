#pragma once

#include <array>
#include <cstdint>

#include "TimedWaveform.hpp"
#include "ti_msp_dl_config.h"

namespace LibXR
{

class MSPM0TimedWaveform : public TimedWaveform
{
 public:
  struct Resources
  {
    GPTIMER_Regs* timer;
    DL_TIMER_CC_INDEX compare_index;
    volatile std::uint32_t* compare_register;
    std::uint8_t dma_channel;
    DL_TIMER_PUBLISHER_INDEX timer_publisher_index;
    DL_DMA_SUBSCRIBER_INDEX dma_subscriber_index;
    DL_TIMER_EVENT_ROUTE event_route;
    std::uint32_t event_mask;
    std::uint8_t event_channel_id;
    IRQn_Type dma_irqn;
  };

  explicit MSPM0TimedWaveform(Resources resources);

  ErrorCode Send(const CompareStream& stream, std::uint32_t timeout_ms) override;
  bool Busy() const override;
  void Stop() override;

  static bool OnDmaInterrupt(DMA_Regs* dma);

 private:
  static constexpr std::size_t kMaxDmaChannels = 16U;

  static std::int8_t ChannelFromIidx(DL_DMA_EVENT_IIDX iidx);
  static std::uint32_t ChannelInterruptMask(std::uint8_t channel);
  static void RegisterInstance(MSPM0TimedWaveform& instance);

  void CompleteFromInterrupt();
  void ConfigureTrigger();
  void EnsureInitialized();
  void SetIdleCompare();

  Resources resources_;
  std::uint32_t idle_compare_ = 0U;
  bool initialized_ = false;

  static std::array<MSPM0TimedWaveform*, kMaxDmaChannels> instances_;
};

#define MSPM0_TIMED_WAVEFORM_INIT(timer_name, gpio_name, dma_channel_name)       \
  ::LibXR::MSPM0TimedWaveform::Resources                                        \
  {                                                                             \
    timer_name##_INST, gpio_name##_IDX,                                         \
        &((timer_name##_INST)->COUNTERREGS.CC_01[gpio_name##_IDX]),             \
        static_cast<std::uint8_t>(dma_channel_name##_CHAN_ID),                  \
        DL_TIMER_PUBLISHER_INDEX_0, DL_DMA_SUBSCRIBER_INDEX_0,                  \
        DL_TIMER_EVENT_ROUTE_1, DL_TIMER_EVENT_ZERO_EVENT, 1U, DMA_INT_IRQn     \
  }

}  // namespace LibXR
