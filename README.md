# WS2812

通过 SPI 驱动 WS2812 灯带 / WS2812 LED strip driver over SPI

---

## 简介 / Overview

本模块将 RGB 像素缓冲区编码为 WS2812 所需的时序，并通过 LibXR SPI 接口发送。SPI 应作为 WS2812 专用 TX-only 输出使用，不需要 MISO、SCK 或 CS 连接到灯带。

This module encodes an RGB pixel buffer into the waveform required by WS2812 and sends it through the LibXR SPI interface. The SPI peripheral should be used as a dedicated TX-only output for WS2812; MISO, SCK, and CS do not need to be connected to the strip.

当前编码顺序为 GRB。WS2812 bit `0` 编码为 `0xC0`，bit `1` 编码为 `0xF8`。

The current color order is GRB. WS2812 bit `0` is encoded as `0xC0`, and bit `1` is encoded as `0xF8`.

---

## 所需硬件 / Required Hardware

- `ws2812_spi`: WS2812 数据输出 SPI 设备 / SPI device for WS2812 data output

只需要把选定 SPI 外设的 MOSI 连接到 WS2812 数据输入；SCK、MISO 和片选信号不需要连接到灯带。

Only the selected SPI peripheral MOSI pin needs to be connected to the WS2812
data input. SCK, MISO, and chip-select signals do not need to be connected to
the strip.

---

## 构造参数 / Constructor Arguments

| 参数 / Parameter | 说明 / Description | 默认值 / Default |
| --- | --- | --- |
| `spi_alias` | SPI 设备别名 / SPI device alias | `ws2812_spi` |
| `led_count` | 实际灯珠数量，超过 `MaxLeds` 会被裁剪 / Active LED count, clipped by `MaxLeds` | `16` |
| `brightness` | 初始全局亮度，范围 `0~255` / Initial global brightness, range `0~255` | `32` |
| `demo_period_ms` | 彩虹 demo 周期，单位 ms；`0` 表示关闭 / Rainbow demo period in ms; `0` disables it | `40` |

---

## 模板参数 / Template Arguments

| 参数 / Parameter | 说明 / Description | 默认值 / Default |
| --- | --- | --- |
| `MaxLeds` | 最大灯珠数量，也是内部像素缓冲区容量 / Maximum LED count and pixel buffer capacity | `16` |
| `ResetBytes` | 帧前后 reset 低电平字节数 / Reset low-level bytes before and after a frame | `304` |

---

## 配置项 / Configuration

使用该模块的工程应在自己的 `User/xrobot.yaml` 中配置实例参数。

Consuming projects should configure instance arguments in their own
`User/xrobot.yaml`.

| 配置项 / Key | 说明 / Description |
| --- | --- |
| `WS2812MaxLeds` | 最大灯珠数量 / Maximum LED count |
| `WS2812LedCount` | 实际灯珠数量 / Active LED count |
| `WS2812ResetBytes` | reset 低电平填充字节数 / Reset low-level padding bytes |
| `WS2812SpiTxBufferBytes` | SPI TX buffer 大小，按 `MaxLeds * 24 + ResetBytes * 2` 计算 / SPI TX buffer size, calculated as `MaxLeds * 24 + ResetBytes * 2` |
| `WS2812Brightness` | 初始全局亮度 / Initial global brightness |
| `WS2812DemoPeriodMs` | 彩虹 demo 周期 / Rainbow demo period |

---

## 主要接口 / Main APIs

| 接口 / API | 说明 / Description |
| --- | --- |
| `Size()` | 获取当前有效灯珠数量 / Get active LED count |
| `SetBrightness(brightness)` | 设置全局亮度，下一次 `Show()` 生效 / Set global brightness for the next `Show()` |
| `SetPixel(index, red, green, blue)` | 设置单颗灯珠颜色，不会立即发送 / Set one LED color without sending immediately |
| `Fill(red, green, blue)` | 填充所有有效灯珠颜色，不会立即发送 / Fill all active LEDs without sending immediately |
| `Clear()` | 熄灭所有灯珠并立即发送 / Turn all LEDs off and send immediately |
| `Show()` | 编码当前像素缓冲区并通过 SPI 发送 / Encode current pixels and send over SPI |

---

## 发送注意事项 / Transfer Notes

WS2812 对帧内间隔敏感。建议将该 SPI 实例配置为 TX-only、连续发送，并只用作 WS2812 数据输出。

WS2812 is sensitive to gaps inside a frame. Configure the SPI instance as
TX-only with continuous transmission and use it only as the WS2812 data output.

建议根据实际 SPI 实现确认 TX buffer 位于可访问 RAM 中，并保证大小不小于 `MaxLeds * 24 + ResetBytes * 2`。

Make sure the TX buffer is placed in RAM accessible by the selected SPI implementation and is at least `MaxLeds * 24 + ResetBytes * 2` bytes.

---

## 依赖 / Depends

- 无其他 XRobot 模块依赖 / No other XRobot module dependencies
