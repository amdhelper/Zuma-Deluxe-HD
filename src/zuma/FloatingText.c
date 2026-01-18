#include <stdio.h>
#include <stdarg.h>
#include "FloatingText.h"
#include "ResourceStore.h"

typedef struct FloatingText_ {
    float x, y;
    float ysp;
    char text[64];
    uint32_t color;
    bool isLiving;
} FloatingText_;

#define MAX_FLOATING_TEXT 32
static FloatingText_ floatingTextArray_[MAX_FLOATING_TEXT];
static int floatingTextCounter_ = 0;

void FloatingTextFactory_Instantiate(float x, float y, uint32_t color, const char* format, ...) {
    int attempts = 0;
    while (floatingTextArray_[floatingTextCounter_].isLiving && attempts < MAX_FLOATING_TEXT) {
        floatingTextCounter_++;
        if (floatingTextCounter_ >= MAX_FLOATING_TEXT)
            floatingTextCounter_ = 0;
        attempts++;
    }
    // If full, we just overwrite the slot at floatingTextCounter_ (oldest or next available)

    floatingTextArray_[floatingTextCounter_].isLiving = true;
    floatingTextArray_[floatingTextCounter_].x = x;
    floatingTextArray_[floatingTextCounter_].y = y;
    floatingTextArray_[floatingTextCounter_].ysp = -5;
    floatingTextArray_[floatingTextCounter_].color = color;

    va_list args;
    va_start(args, format);
    vsnprintf(floatingTextArray_[floatingTextCounter_].text, 64, format, args);
    va_end(args);
    
    // Move to next slot for next time
    floatingTextCounter_++;
    if (floatingTextCounter_ >= MAX_FLOATING_TEXT) floatingTextCounter_ = 0;
}

void FloatingTextFactory_Init() {
    for (int i = 0; i < MAX_FLOATING_TEXT; i++) floatingTextArray_[i].isLiving = false;

    floatingTextCounter_ = 0;
}

void FloatingTextFactory_Update() {
    for (int i = 0; i < MAX_FLOATING_TEXT; i++) {
        if (!floatingTextArray_[i].isLiving) continue;

        floatingTextArray_[i].ysp += 0.125f;

        if (floatingTextArray_[i].ysp > 1)
            floatingTextArray_[i].isLiving = false;

        floatingTextArray_[i].y += floatingTextArray_[i].ysp;
    }
}

void FloatingTextFactory_Draw() {
    for (int i = 0; i < MAX_FLOATING_TEXT; i++) {
        FloatingText_ ft = floatingTextArray_[i];

        if (!ft.isLiving) continue;

        HQC_Artist_SetColorHex(C_BLACK);
        HQC_Artist_SetFontOutline(Store_GetFontByID(FONT_CANCUN_12), 2);
        HQC_Artist_DrawText(Store_GetFontByID(FONT_CANCUN_12), ft.text, ft.x, ft.y);
        HQC_Artist_SetFontOutline(Store_GetFontByID(FONT_CANCUN_12), 0);
        HQC_Artist_SetColorHex(ft.color);
        HQC_Artist_DrawText(Store_GetFontByID(FONT_CANCUN_12), ft.text, ft.x, ft.y);
        HQC_Artist_SetColorHex(C_WHITE);
    }
}

