#pragma once
#include <stdint.h>
#include "stm32746g_discovery.h"
#include "stm32746g_discovery_audio.h"
#include "ex03b/stm32f7xx_it.h"
#include "clockconfig.h"
#include "ct_math.h"

#define TIMx              TIM3
#define TIMx_CLK_ENABLE() __HAL_RCC_TIM3_CLK_ENABLE()
#define TIMx_IRQn         TIM3_IRQn
#define TIMx_IRQHandler   TIM3_IRQHandler

#define VOLUME 70

#define AUDIO_DMA_BUFFER_SIZE 1024 // bytes
#define AUDIO_DMA_BUFFER_SIZE2 (AUDIO_DMA_BUFFER_SIZE >> 1) // 16bit words
#define AUDIO_DMA_BUFFER_SIZE4 (AUDIO_DMA_BUFFER_SIZE >> 2) // half buffer size (in 16bit words)
#define AUDIO_DMA_BUFFER_SIZE8 (AUDIO_DMA_BUFFER_SIZE >> 3) // number of stereo samples (16bit words)

typedef enum {
	BUFFER_OFFSET_NONE = 0, BUFFER_OFFSET_HALF, BUFFER_OFFSET_FULL
} DMABufferState;
