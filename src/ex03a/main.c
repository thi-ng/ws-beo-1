#include <math.h>
#include "ex03a/main.h"
#include "gui/bt_blackangle48_12.h"
#include "macros.h"

static void touchScreenError();
static void initAppGUI();
static void drawGUI();
static void updateAudioBuffer();

TIM_HandleTypeDef TimHandle;

static TS_StateTypeDef rawTouchState;
static GUITouchState touchState;

static SpriteSheet dialSheet = { .pixels = bt_blackangle48_12_argb8888,
		.spriteWidth = 48, .spriteHeight = 48, .numSprites = 12, .format =
		CM_ARGB8888 };

static GUI *gui;

static __IO DMABufferState bufferState = BUFFER_OFFSET_NONE;
static uint8_t audioBuf[AUDIO_DMA_BUFFER_SIZE];

static Oscillator osc = { .phase = 0.0f, .freq = HZ_TO_RAD(220.0f) };

int main() {
	CPU_CACHE_Enable();
	HAL_Init();
	SystemClock_Config();
	BSP_LCD_Init();
	if (BSP_TS_Init(BSP_LCD_GetXSize(), BSP_LCD_GetYSize()) == TS_OK) {
		BSP_LCD_LayerDefaultInit(LTDC_ACTIVE_LAYER, LCD_FRAME_BUFFER);
		BSP_LCD_SelectLayer(LTDC_ACTIVE_LAYER);

		if (BSP_AUDIO_OUT_Init(OUTPUT_DEVICE_HEADPHONE, VOLUME, SAMPLE_RATE)
				!= 0) {
			Error_Handler();
		}
		BSP_AUDIO_OUT_SetAudioFrameSlot(CODEC_AUDIOFRAME_SLOT_02);
		BSP_AUDIO_OUT_SetVolume(VOLUME);
		BSP_AUDIO_OUT_Play((uint16_t *) audioBuf, AUDIO_DMA_BUFFER_SIZE);

		initAppGUI();

		while (1) {
			drawGUI();
			HAL_Delay(100);
		}

		if (BSP_AUDIO_OUT_Stop(CODEC_PDWN_HW) != AUDIO_OK) {
			Error_Handler();
		}
	} else {
		touchScreenError();
	}
	return 0;
}

static void touchScreenError() {
	BSP_LCD_SetFont(&LCD_DEFAULT_FONT);
	BSP_LCD_SetBackColor(LCD_COLOR_RED);
	BSP_LCD_Clear(LCD_COLOR_RED);
	BSP_LCD_SetTextColor(LCD_COLOR_WHITE);
	BSP_LCD_DisplayStringAt(0, BSP_LCD_GetYSize() / 2 - 24,
			(uint8_t *) "Touchscreen error!", CENTER_MODE);
	while (1) {
	}
}

static void setVolume(GUIElement *e) {
	DialButtonState *db = (DialButtonState *) (e->userData);
	BSP_AUDIO_OUT_SetVolume((uint8_t) (db->value * 100));
}

static void setOscFreq(GUIElement *e) {
	DialButtonState *db = (DialButtonState *) (e->userData);
	osc.freq = HZ_TO_RAD(50.0f + db->value * 4950.0f);
}

static void initAppGUI() {
	gui = initGUI(2, &UI_FONT, LCD_COLOR_BLACK, LCD_COLOR_LIGHTGRAY);
	gui->items[0] = guiDialButton(0, "Volume", 10, 10, (float) VOLUME / 100.0f,
			0.025f, &dialSheet, setVolume);
	gui->items[1] = guiDialButton(1, "Freq", 80, 10,
			osc.freq / 5000.0f, 0.025f, &dialSheet, setOscFreq);
}

static void drawGUI() {
	BSP_LCD_Clear(gui->bgColor);
	uint16_t w = MIN(BSP_LCD_GetXSize(), AUDIO_DMA_BUFFER_SIZE2);
	uint16_t h = BSP_LCD_GetYSize() / 2;
	int16_t *ptr = (int16_t*) &audioBuf[0];
	for (uint16_t x = 0; x < w; x++) {
		float y = (float) (ptr[x]) / 32767.0;
		BSP_LCD_DrawPixel(x, (uint16_t) (h + h * y), LCD_COLOR_CYAN);
	}
	BSP_TS_GetState(&rawTouchState);
	guiUpdateTouch(&rawTouchState, &touchState);
	guiUpdate(gui, &touchState);
	guiForceRedraw(gui);
	guiDraw(gui);
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

static void renderAudio(int16_t *ptr) {
	uint32_t len = AUDIO_DMA_BUFFER_SIZE8;
	while (len--) {
		osc.phase += osc.freq;
		if (osc.phase >= TAU) {
			osc.phase -= TAU;
		}
		float y = sinf(osc.phase);
		int16_t yi = ct_clamp16((int32_t) (y * 0x7fff));
		*ptr++ = yi;
		*ptr++ = yi;
	}
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
