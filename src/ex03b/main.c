#include <math.h>
#include "ex03b/main.h"
#include "ctss/synth.h"
#include "ctss/adsr.h"
#include "ctss/iir.h"
#include "ctss/biquad.h"
#include "ctss/panning.h"
#include "ctss/osc.h"
#include "ctss/pluck.h"
#include "ctss/node_ops.h"
#include "ctss/delay.h"
#include "macros.h"

static void initSynth();
static void updateAudioBuffer();
static void initTimer(uint16_t period);

TIM_HandleTypeDef TimHandle;

static CTSS_Synth synth;

static __IO DMABufferState bufferState = BUFFER_OFFSET_NONE;
static uint8_t audioBuf[AUDIO_DMA_BUFFER_SIZE];
static uint32_t lastNote = 0;
static uint32_t voiceID = 0;

int main() {
	CPU_CACHE_Enable();
	HAL_Init();
	SystemClock_Config();

	initTimer(20);
	initSynth();

	if (BSP_AUDIO_OUT_Init(OUTPUT_DEVICE_HEADPHONE, VOLUME, SAMPLE_RATE) != 0) {
		Error_Handler();
	}
	BSP_AUDIO_OUT_SetAudioFrameSlot(CODEC_AUDIOFRAME_SLOT_02);
	BSP_AUDIO_OUT_SetVolume(VOLUME);
	BSP_AUDIO_OUT_Play((uint16_t *) audioBuf, AUDIO_DMA_BUFFER_SIZE);

	while (1) {
		BSP_LED_Toggle(LED_GREEN);
		HAL_Delay(100);
	}

	return 0;
}

static void initStack(CTSS_DSPStack *stack, float freq) {
	float f1 = HZ_TO_RAD(freq);
	//float f2 = HZ_TO_RAD(freq * 1.01f);
	CTSS_DSPNode *env = ctss_adsr("env", synth.lfo[0], 0.1f,
			0.5f, 2.0f, 1.0f, 0.4f);
	CTSS_DSPNode *osc1 = ctss_osc("osc1", ctss_process_osc_sin, 0.0f, f1,
			1.0f, 0.0f);
	//CTSS_DSPNode *osc2 = ctss_osc("b", oscFunctions[preset->osc2Fn], 0.0f, f2,
	//		preset->osc2Gain, 0.0f);
	CTSS_DSPNode *sum = ctss_op2("sum", osc1, env,
			ctss_process_mult);

	CTSS_DSPNode *pan = ctss_panning("pan", sum, NULL, 0.5f);
	CTSS_DSPNode *nodes[] = { env, osc1, sum, pan };
	ctss_init_stack(stack);
	ctss_build_stack(stack, nodes, 4);
}

static void initSynth() {
	ctss_init(&synth, 3);
	//synth.lfo[0] = ctss_osc("glofo", ctss_process_osc_sin, 0.0f,
	//		HZ_TO_RAD(1 / 24.0f), 0.6f, 1.0f);
	synth.numLFO = 0;
	for (uint8_t i = 0; i < synth.numStacks; i++) {
		initStack(&synth.stacks[i], 110.0f);
	}
	ctss_collect_stacks(&synth);
}
void BSP_AUDIO_OUT_HalfTransfer_CallBack(void) {
	bufferState = BUFFER_OFFSET_HALF;
	//updateAudioBuffer();
}

void BSP_AUDIO_OUT_TransferComplete_CallBack(void) {
	bufferState = BUFFER_OFFSET_FULL;
	//updateAudioBuffer();
}

void BSP_AUDIO_OUT_Error_CallBack(void) {
	Error_Handler();
}

void renderAudio(int16_t *ptr) {
	uint32_t tick = HAL_GetTick();
	if (tick - lastNote > 500) {
		CTSS_DSPStack *voice = &synth.stacks[voiceID];
		ctss_reset_adsr(ctss_node_for_id(voice, "env"));
		NODE_ID_STATE(CTSS_OscState, voice, "osc1")->freq = HZ_TO_RAD((float)(tick % 1666));
		ctss_activate_stack(voice);
		lastNote = tick;
		voiceID = (voiceID + 1) % synth.numStacks;
	}
	ctss_update_mix_stereo_i16(&synth, ctss_mixdown_i16, AUDIO_DMA_BUFFER_SIZE8, ptr);
}

static void updateAudioBuffer() {
	if (bufferState == BUFFER_OFFSET_HALF) {
		int16_t *ptr = (int16_t*) &audioBuf[0];
		//renderAudio(ptr);
		bufferState = BUFFER_OFFSET_NONE;
	} else if (bufferState == BUFFER_OFFSET_FULL) {
		int16_t *ptr = (int16_t*) &audioBuf[0] + AUDIO_DMA_BUFFER_SIZE4;
		//renderAudio(ptr);
		bufferState = BUFFER_OFFSET_NONE;
	}
}

static void initTimer(uint16_t period) {
	uint32_t prescaler = (uint32_t) ((SystemCoreClock / 2) / 10000) - 1;
	TimHandle.Instance = TIMx;

	/* Initialize TIMx peripheral as follows:
	 * Period = 10000 - 1
	 * Prescaler = ((SystemCoreClock / 2)/10000) - 1
	 * ClockDivision = 0
	 * Counter direction = Up
	 */
	TimHandle.Init.Period = period - 1;
	TimHandle.Init.Prescaler = prescaler;
	TimHandle.Init.ClockDivision = 0;
	TimHandle.Init.CounterMode = TIM_COUNTERMODE_UP;
	TimHandle.Init.RepetitionCounter = 0;

	if (HAL_TIM_Base_Init(&TimHandle) != HAL_OK) {
		Error_Handler();
	}

	if (HAL_TIM_Base_Start_IT(&TimHandle) != HAL_OK) {
		Error_Handler();
	}
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
	updateAudioBuffer();
}
