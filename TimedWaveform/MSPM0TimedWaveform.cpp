#include "MSPM0TimedWaveform.hpp"

#include "mspm0_dma_shared.hpp"

namespace LibXR
{

MSPM0TimedWaveform::MSPM0TimedWaveform(Resources resources)
    : resources_(resources)
{
  ASSERT(resources_.timer != nullptr);
  ASSERT(resources_.compare_register != nullptr);
  ASSERT(resources_.dma_channel < MSPM0DmaShared::MAX_DMA_CHANNELS);

  const bool registered = MSPM0DmaShared::RegisterChannel(
      resources_.dma_channel, &MSPM0TimedWaveform::OnDmaComplete, this);
  ASSERT(registered);
}

ErrorCode MSPM0TimedWaveform::Send(const CompareStream& stream,
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

bool MSPM0TimedWaveform::Busy() const
{
  if (!initialized_)
  {
    return false;
  }

  return DL_DMA_getTransferSize(DMA, resources_.dma_channel) > 0U;
}

void MSPM0TimedWaveform::Stop()
{
  if (!initialized_)
  {
    return;
  }

  DL_DMA_disableChannel(DMA, resources_.dma_channel);
  SetIdleCompare();
}

void MSPM0TimedWaveform::OnDmaComplete(void* context)
{
  auto* instance = static_cast<MSPM0TimedWaveform*>(context);
  if (instance != nullptr)
  {
    instance->CompleteFromInterrupt();
  }
}

void MSPM0TimedWaveform::CompleteFromInterrupt()
{
  DL_DMA_disableChannel(DMA, resources_.dma_channel);
}

void MSPM0TimedWaveform::ConfigureTrigger()
{
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

void MSPM0TimedWaveform::EnsureInitialized()
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

void MSPM0TimedWaveform::SetIdleCompare()
{
  *resources_.compare_register = idle_compare_;
}

}  // namespace LibXR
