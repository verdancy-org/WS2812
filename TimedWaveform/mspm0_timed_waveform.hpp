#pragma once

#include <cstdint>

#include "timed_waveform.hpp"
#include "mspm0_dma_shared.hpp"
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
  };

  explicit MSPM0TimedWaveform(Resources resources);

  ErrorCode Send(const CompareStream& stream, std::uint32_t timeout_ms) override;
  bool Busy() const override;
  void Stop() override;

 private:
  static constexpr std::size_t MAX_DMA_CHANNELS = 16U;

  static void DmaCallback(void* context);
  void CompleteFromInterrupt();
  void ConfigureTrigger();
  void EnsureInitialized();
  void SetIdleCompare();

  Resources resources_;
  std::uint32_t idle_compare_ = 0U;
  bool initialized_ = false;
};

inline MSPM0TimedWaveform::MSPM0TimedWaveform(Resources resources)
    : resources_(resources)
{
  ASSERT(resources_.timer != nullptr);
  ASSERT(resources_.compare_register != nullptr);
  ASSERT(resources_.dma_channel < MAX_DMA_CHANNELS);

  ASSERT(MSPM0DmaShared::RegisterChannel(resources_.dma_channel, DmaCallback,
                                         this));
}

inline ErrorCode MSPM0TimedWaveform::Send(const CompareStream& stream,
                                          std::uint32_t timeout_ms)
{
  if (stream.samples == nullptr || stream.count == 0U || stream.period_ticks == 0U)
  {
    return ErrorCode::ARG_ERR;
  }

  EnsureInitialized();

  if (Busy())
  {
    return ErrorCode::BUSY;
  }

  idle_compare_ = stream.idle_compare;
  DL_Timer_setLoadValue(resources_.timer, stream.period_ticks);

  DL_DMA_disableChannel(DMA, resources_.dma_channel);
  DL_DMA_setSrcAddr(
      DMA, resources_.dma_channel,
      static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(stream.samples)));
  DL_DMA_setDestAddr(
      DMA, resources_.dma_channel,
      static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(
          const_cast<std::uint32_t*>(resources_.compare_register))));
  DL_DMA_setTransferSize(DMA, resources_.dma_channel, stream.count);

  DL_Timer_disableEvent(resources_.timer, resources_.event_route,
                        resources_.event_mask);
  DL_Timer_enableEvent(resources_.timer, resources_.event_route,
                       resources_.event_mask);
  DL_DMA_enableChannel(DMA, resources_.dma_channel);

  const auto start_ms =
      static_cast<std::uint32_t>(LibXR::Timebase::GetMilliseconds());
  while (Busy())
  {
    if (timeout_ms != 0U)
    {
      const auto now_ms =
          static_cast<std::uint32_t>(LibXR::Timebase::GetMilliseconds());
      if ((now_ms - start_ms) >= timeout_ms)
      {
        Stop();
        return ErrorCode::TIMEOUT;
      }
    }
    __NOP();
  }

  Stop();
  return ErrorCode::OK;
}

inline bool MSPM0TimedWaveform::Busy() const
{
  if (!initialized_)
  {
    return false;
  }

  return DL_DMA_getTransferSize(DMA, resources_.dma_channel) > 0U;
}

inline void MSPM0TimedWaveform::Stop()
{
  if (!initialized_)
  {
    return;
  }

  DL_DMA_disableChannel(DMA, resources_.dma_channel);
  SetIdleCompare();
}

inline void MSPM0TimedWaveform::DmaCallback(void* context)
{
  static_cast<MSPM0TimedWaveform*>(context)->CompleteFromInterrupt();
}

inline void MSPM0TimedWaveform::CompleteFromInterrupt()
{
  DL_DMA_disableChannel(DMA, resources_.dma_channel);
}

inline void MSPM0TimedWaveform::ConfigureTrigger()
{
  MSPM0DmaShared::EnableDmaIRQ();
  DL_Timer_enableEvent(resources_.timer, resources_.event_route,
                       resources_.event_mask);
  DL_Timer_setPublisherChanID(resources_.timer, resources_.timer_publisher_index,
                              resources_.event_channel_id);
  DL_DMA_setSubscriberChanID(DMA, resources_.dma_subscriber_index,
                             resources_.event_channel_id);
  DL_Timer_setCaptCompUpdateMethod(resources_.timer,
                                   DL_TIMER_CC_UPDATE_METHOD_ZERO_EVT,
                                   resources_.compare_index);
}

inline void MSPM0TimedWaveform::EnsureInitialized()
{
  if (initialized_)
  {
    return;
  }

  ConfigureTrigger();
  SetIdleCompare();
  DL_Timer_startCounter(resources_.timer);
  initialized_ = true;
}

inline void MSPM0TimedWaveform::SetIdleCompare()
{
  *resources_.compare_register = idle_compare_;
}

#define MSPM0_TIMED_WAVEFORM_INIT(timer_name, gpio_name, dma_channel_name)       \
  ::LibXR::MSPM0TimedWaveform::Resources                                        \
  {                                                                             \
    timer_name##_INST, gpio_name##_IDX,                                         \
        &((timer_name##_INST)->COUNTERREGS.CC_01[gpio_name##_IDX]),             \
        static_cast<std::uint8_t>(dma_channel_name##_CHAN_ID),                  \
        DL_TIMER_PUBLISHER_INDEX_0, DL_DMA_SUBSCRIBER_INDEX_0,                  \
        DL_TIMER_EVENT_ROUTE_1, DL_TIMER_EVENT_ZERO_EVENT, 1U                   \
  }

}  // namespace LibXR
