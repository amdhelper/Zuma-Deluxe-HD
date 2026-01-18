#include "Index.h"
#include "../ResourceStore.h"
#include <stdlib.h>

static void _Load() {
    HQC_Log("SceneMinimal: Load");
}

static void _Update() {
    // Do nothing
}

static void _Draw() {
    // Test text drawing
    HQC_Artist_SetColorHex(0x336699);
    HQC_Font font = Store_GetFontByID(0);
    if (font) {
        HQC_Artist_DrawText(font, "Test", 640, 360);
    }
}

static void _Free() {
    HQC_Log("SceneMinimal: Free");
}

HScene Scene_Register_Minimal() {
    return Scene_New("minimal", _Load, _Update, _Draw, _Free);
}