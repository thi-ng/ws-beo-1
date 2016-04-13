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
#include "ctss/ctss_math.h"

static void initSynth();
static void updateAudioBuffer();

TIM_HandleTypeDef TimHandle;

static __IO DMABufferState bufferState = BUFFER_OFFSET_NONE;
static uint8_t audioBuf[AUDIO_DMA_BUFFER_SIZE];
static CTSS_Synth synth;

uint32_t lastNote = 0;
uint32_t voiceID = 0;

int main() {
	CPU_CACHE_Enable();
	HAL_Init();
	SystemClock_Config();

	initSynth();

	if (BSP_AUDIO_OUT_Init(OUTPUT_DEVICE_HEADPHONE, VOLUME, SAMPLE_RATE) != 0) {
		Error_Handler();
	}

	BSP_AUDIO_OUT_SetAudioFrameSlot(CODEC_AUDIOFRAME_SLOT_02);
	BSP_AUDIO_OUT_SetVolume(VOLUME);
	BSP_AUDIO_OUT_Play((uint16_t *) audioBuf, AUDIO_DMA_BUFFER_SIZE);

	BSP_LED_Init(LED_GREEN);
	while (1) {
		BSP_LED_On(LED_GREEN);
		HAL_Delay(100);
		BSP_LED_Off(LED_GREEN);
		HAL_Delay(100);
	}

	return 0;
}

static void initStack(CTSS_DSPStack *stack, float freq) {
	float f1 = HZ_TO_RAD(freq);
	//float f2 = HZ_TO_RAD(freq * 1.01f);
	CTSS_DSPNode *env = ctss_adsr("env", synth.lfo[0], 0.1f,
			0.5f, 2.0f, 1.0f, 0.4f);
	CTSS_DSPNode *osc1 = ctss_osc("oscA", ctss_process_osc_spiral, 0.0f, f1,
			1.0f, 0.0f);
//	CTSS_DSPNode *osc2 = ctss_osc("b", oscFunctions[preset->osc2Fn], 0.0f, f2,
//			preset->osc2Gain, 0.0f);
	CTSS_DSPNode *sum = ctss_op2("sum", osc1, env, ctss_process_mult/*, osc2, env, ctss_process_madd*/);
	CTSS_DSPNode *pan = ctss_panning("pan", sum, NULL, 0.5f);
	CTSS_DSPNode *nodes[] = { env, osc1/*, osc2*/, sum, pan };
	ctss_init_stack(stack);
	ctss_build_stack(stack, nodes, 4);
	stack->flags = 0;
}

static void initSynth() {
	ctss_init(&synth, 3);
	synth.lfo[0] = ctss_osc("lfo1", ctss_process_osc_sin, 0.0f,
			HZ_TO_RAD(1 / 24.0f), 0.6f, 1.0f);
	synth.numLFO = 1;
	for (uint8_t i = 0; i < synth.numStacks; i++) {
		initStack(&synth.stacks[i], 110.0f);
	}
	ctss_collect_stacks(&synth);
}

void renderAudio(int16_t *ptr) {
	uint32_t tick = HAL_GetTick();
	if (tick - lastNote >= 500) {
		lastNote = tick;
		CTSS_DSPStack *voice = &synth.stacks[voiceID];
		CTSS_DSPNode *env = ctss_node_for_id(voice, "env");
		ctss_reset_adsr(env);
		NODE_ID_STATE(CTSS_OscState, voice, "oscA")->freq = HZ_TO_RAD(tick % 1666);
		ctss_activate_stack(voice);
		voiceID = (voiceID + 1) % synth.numStacks;
	}
	ctss_update_mix_stereo_i16(&synth, ctss_mixdown_i16, AUDIO_DMA_BUFFER_SIZE8, ptr);
}

static float mix(float a, float b, float t) {
	return a + (b - a) * t;
}

static float mapValue(float x, float a, float b, float c, float d) {
	return mix(c, d, (x - a) / (b - a));
}

void BSP_AUDIO_OUT_HalfTransfer_CallBack(void) {
	bufferState = BUFFER_OFFSET_HALF;
	updateAudioBuffer();
}

void BSP_AUDIO_OUT_TransferComplete_CallBack(void) {
	bufferState = BUFFER_OFFSET_FULL;
	updateAudioBuffer();
}

void BSP_AUDIO_OUT_Error_CallBack(void) {
	Error_Handler();
}

static void updateAudioBuffer() {
	if (bufferState == BUFFER_OFFSET_HALF) {
		int16_t *ptr = (int16_t*) &audioBuf[0];
		renderAudio(ptr);
		bufferState = BUFFER_OFFSET_NONE;
	} else if (bufferState == BUFFER_OFFSET_FULL) {
		int16_t *ptr = (int16_t*) &audioBuf[0] + AUDIO_DMA_BUFFER_SIZE4;
		renderAudio(ptr);
		bufferState = BUFFER_OFFSET_NONE;
	}
}
