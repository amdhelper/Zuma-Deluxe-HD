#include "Index.h"

#include "../Menu.h"
#include "../ResourceStore.h"
#include <stdio.h>

// Forward declaration
HScene Scene_Register_Menu();


#define private static

private HQC_VECTOR(HButton) _btnList;

private int spritesCount;
private int sprId;
private int sndId;

private void _OnBtnNextClick() { 
    sprId++; 
    sprId %= Store_CountSprites();
}


private void _OnBtnPrevClick() { 
    sprId--; 
    if (sprId < 0) sprId = Store_CountSprites()-1;
}


private void _OnBtnNextSndClick() { 
    HQC_DJ_PlaySound(Store_GetSoundByID(sndId)); 
    sndId++; 
    sndId %= Store_CountSounds();
}

private void _OnBtnPrevSndClick() {  
    HQC_DJ_PlaySound(Store_GetSoundByID(sndId)); 
    sndId--; 
    if (sndId < 0) sndId = Store_CountSounds()-1;
}

private void _Load() {
    _btnList = HQC_Container_CreateVector(sizeof(HButton));

    // Initialize sprite and sound IDs
    sprId = 0;
    sndId = 0;
    
    HQC_Log("SceneTest: Loading with %d sprites and %d sounds", 
            Store_CountSprites(), Store_CountSounds());

    HButton btnNext = Button_Create(1100, 650);
    Button_OnClick(btnNext, _OnBtnNextClick);
    Button_SetText(btnNext, "next");

    HButton btnPrev = Button_Create(900, 650);
    Button_SetText(btnPrev, "prev");
    Button_OnClick(btnPrev, _OnBtnPrevClick);

    HButton btnNextSnd = Button_Create(1100, 650-100);
    Button_OnClick(btnNextSnd, _OnBtnNextSndClick);
    Button_SetText(btnNextSnd, "next");

    HButton btnPrevSnd = Button_Create(900, 650-100);
    Button_SetText(btnPrevSnd, "prev");
    Button_OnClick(btnPrevSnd, _OnBtnPrevSndClick);

    HQC_Container_VectorAdd(_btnList, &btnNext);
    HQC_Container_VectorAdd(_btnList, &btnPrev);
    HQC_Container_VectorAdd(_btnList, &btnNextSnd);
    HQC_Container_VectorAdd(_btnList, &btnPrevSnd);
}

private void _Update() {
    for (int i = 0; i < HQC_Container_VectorCount(_btnList); i++)
        Button_Update(*((HButton*)HQC_Container_VectorGet(_btnList, i)));

}

private void _Draw() {
    // Draw a simple background color first
    HQC_Artist_SetColorHex(0x2C3E50);
    
    // Draw sprite info
    HQC_Artist_SetColorHex(0xFFFFFF);
    HQC_Artist_DrawText(
        Store_GetFontByID(0), 
        "Test Scene - Sprite Viewer", 
        640, 50
    );
    
    // Verify sprite ID is valid
    int maxSprites = Store_CountSprites();
    if (sprId >= 0 && sprId < maxSprites) {
        HQC_Sprite sprite = Store_GetSpriteByID(sprId);
        if (sprite) {
            HQC_Artist_DrawSprite(sprite, 1280/2, 720/2);
            HQC_Log("Drawing sprite ID %d at center", sprId);
        } else {
            HQC_Log("Sprite ID %d is NULL", sprId);
        }
        
        // Draw sprite info
        char spriteInfo[256];
        snprintf(spriteInfo, sizeof(spriteInfo), "Sprite ID: %d / %d", sprId, maxSprites - 1);
        HQC_Artist_DrawText(Store_GetFontByID(0), spriteInfo, 640, 100);
    } else {
        HQC_Artist_DrawText(
            Store_GetFontByID(0), 
            "Invalid sprite ID", 
            640, 360
        );
    }

    // Draw sound info
    int maxSounds = Store_CountSounds();
    char soundInfo[256];
    snprintf(soundInfo, sizeof(soundInfo), "Sound ID: %d / %d", sndId, maxSounds - 1);
    HQC_Artist_DrawText(Store_GetFontByID(0), soundInfo, 640, 150);

    for (int i = 0; i < HQC_Container_VectorCount(_btnList); i++)
        Button_Draw(*((HButton*)HQC_Container_VectorGet(_btnList, i)));
}

private void _Free() {
    for (int i = 0; i < HQC_Container_VectorCount(_btnList); i++)
        Button_Destroy(*((HButton*)HQC_Container_VectorGet(_btnList, i)));
}

HScene Scene_Register_Test() {
    return Scene_New("test", _Load, _Update, _Draw, _Free);
}

void Scene_RegisterAll() {
    SC_GAME = Scene_Register_Game();
    SC_TEST = Scene_Register_Test();
    SC_MENU = Scene_Register_Menu();
}