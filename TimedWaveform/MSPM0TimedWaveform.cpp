#include "MSPM0TimedWaveform.hpp"

namespace LibXR
{

std::array<MSPM0TimedWaveform*, MSPM0TimedWaveform::kMaxDmaChannels>
    MSPM0TimedWaveform::instances_{};

MSPM0TimedWaveform::MSPM0TimedWaveform(Resources resources)
    : resources_(resources)
{
  ASSERT(resources_.timer != nullptr);
  ASSERT(resources_.compare_register != nullptr);
  ASSERT(resources_.dma_channel < kMaxDmaChannels);

  RegisterInstance(*this);
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

bool MSPM0TimedWaveform::OnDmaInterrupt(DMA_Regs* dma)
{
  const auto iidx = DL_DMA_getPendingInterrupt(dma);
  const std::int8_t channel = ChannelFromIidx(iidx);
  if (channel < 0 || static_cast<std::size_t>(channel) >= kMaxDmaChannels)
  {
    return false;
  }

  MSPM0TimedWaveform* instance = instances_[static_cast<std::size_t>(channel)];
  if (instance == nullptr)
  {
    return false;
  }

  instance->CompleteFromInterrupt();
  return true;
}

std::int8_t MSPM0TimedWaveform::ChannelFromIidx(DL_DMA_EVENT_IIDX iidx)
{
  switch (iidx)
  {
    case DL_DMA_EVENT_IIDX_DMACH0:
      return 0;
    case DL_DMA_EVENT_IIDX_DMACH1:
      return 1;
    case DL_DMA_EVENT_IIDX_DMACH2:
      return 2;
    case DL_DMA_EVENT_IIDX_DMACH3:
      return 3;
    case DL_DMA_EVENT_IIDX_DMACH4:
      return 4;
    case DL_DMA_EVENT_IIDX_DMACH5:
      return 5;
    case DL_DMA_EVENT_IIDX_DMACH6:
      return 6;
    case DL_DMA_EVENT_IIDX_DMACH7:
      return 7;
    case DL_DMA_EVENT_IIDX_DMACH8:
      return 8;
    case DL_DMA_EVENT_IIDX_DMACH9:
      return 9;
    case DL_DMA_EVENT_IIDX_DMACH10:
      return 10;
    case DL_DMA_EVENT_IIDX_DMACH11:
      return 11;
    case DL_DMA_EVENT_IIDX_DMACH12:
      return 12;
    case DL_DMA_EVENT_IIDX_DMACH13:
      return 13;
    case DL_DMA_EVENT_IIDX_DMACH14:
      return 14;
    case DL_DMA_EVENT_IIDX_DMACH15:
      return 15;
    default:
      return -1;
  }
}

std::uint32_t MSPM0TimedWaveform::ChannelInterruptMask(std::uint8_t channel)
{
  switch (channel)
  {
    case 0:
      return DL_DMA_INTERRUPT_CHANNEL0;
    case 1:
      return DL_DMA_INTERRUPT_CHANNEL1;
    case 2:
      return DL_DMA_INTERRUPT_CHANNEL2;
    case 3:
      return DL_DMA_INTERRUPT_CHANNEL3;
    case 4:
      return DL_DMA_INTERRUPT_CHANNEL4;
    case 5:
      return DL_DMA_INTERRUPT_CHANNEL5;
    case 6:
      return DL_DMA_INTERRUPT_CHANNEL6;
    case 7:
      return DL_DMA_INTERRUPT_CHANNEL7;
    case 8:
      return DL_DMA_INTERRUPT_CHANNEL8;
    case 9:
      return DL_DMA_INTERRUPT_CHANNEL9;
    case 10:
      return DL_DMA_INTERRUPT_CHANNEL10;
    case 12:
      return DL_DMA_INTERRUPT_CHANNEL12;
    case 13:
      return DL_DMA_INTERRUPT_CHANNEL13;
    case 14:
      return DL_DMA_INTERRUPT_CHANNEL14;
    case 15:
      return DL_DMA_INTERRUPT_CHANNEL15;
    default:
      return 0U;
  }
}

void MSPM0TimedWaveform::RegisterInstance(MSPM0TimedWaveform& instance)
{
  instances_[instance.resources_.dma_channel] = &instance;
}

void MSPM0TimedWaveform::CompleteFromInterrupt()
{
  DL_DMA_disableChannel(DMA, resources_.dma_channel);
  const std::uint32_t mask = ChannelInterruptMask(resources_.dma_channel);
  if (mask != 0U)
  {
    DL_DMA_clearInterruptStatus(DMA, mask);
  }
}

void MSPM0TimedWaveform::ConfigureTrigger()
{
  NVIC_EnableIRQ(resources_.dma_irqn);
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
