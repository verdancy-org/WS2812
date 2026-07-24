# WS2812

WS2812 shared core plus SPI transport module.

## Overview

`WS2812.hpp` contains the common pixel buffer, brightness scaling, GRB ordering,
and rainbow demo logic. It has no XRobot manifest and performs no hardware I/O.

`WS2812SPI.hpp` is the XRobot-instantiable SPI backend. It encodes each WS2812
bit into one SPI byte and sends frames through `LibXR::SPI`.

`TimedWaveform/timed_waveform.hpp` and `TimedWaveform/mspm0_timed_waveform.hpp` are kept in this
folder as the WS2812-local hardware-container interface for PWM-style LED
transports.

The PWM transport lives in `Modules/WS2812/WS2812PWM.hpp` and consumes that
interface without directly touching DMA or TI DriverLib.

## Required Hardware

For `WS2812SPI`:

- `ws2812_spi`: SPI device used as the WS2812 data output

Only MOSI must be connected to WS2812 DIN. SCK, MISO, and CS do not need to be
connected to the strip.

## Constructor Arguments

| Parameter | Description | Default |
| --- | --- | --- |
| `spi_alias` | SPI device alias | `ws2812_spi` |
| `led_count` | Active LED count, clipped by `MaxLeds` | `16` |
| `brightness` | Initial global brightness, `0..255` | `32` |
| `demo_period_ms` | Rainbow demo period; `0` disables it | `40` |

## Template Arguments

| Parameter | Description | Default |
| --- | --- | --- |
| `MaxLeds` | Pixel buffer capacity | `16` |
| `ResetBytes` | Reset low-level bytes before and after a frame | `304` |

## APIs

| API | Description |
| --- | --- |
| `Size()` | Get active LED count |
| `SetBrightness(brightness)` | Set brightness for the next `Show()` |
| `SetPixel(index, red, green, blue)` | Set one LED without sending immediately |
| `Fill(red, green, blue)` | Fill all active LEDs without sending immediately |
| `Clear()` | Clear pixels and send immediately |
| `Show()` | Encode and send through the concrete transport |

## SPI Encoding

The color order is GRB. WS2812 bit `0` is encoded as `0xC0`; bit `1` is encoded
as `0xF8`.
