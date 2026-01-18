#include "Index.h"

#include "../Menu.h"
#include "../ResourceStore.h"
#include <stdlib.h>

#define private static

private HQC_VECTOR(HButton) _btnList;
private HScene _pendingScene = NULL;

private void _OnBtnGameClick() { 
    _pendingScene = SC_GAME;
}

private void _OnBtnTestClick() { 
    _pendingScene = SC_TEST;
}

private void _OnBtnQuitClick() {  
    exit(0);
}

private void _Load() {
    _btnList = HQC_Container_CreateVector(sizeof(HButton));
    _pendingScene = NULL;

    HQC_Log("SceneMenu: Creating buttons...");

    // Create main menu buttons
    HButton btnGame = Button_Create(640, 300);
    if (btnGame) {
        Button_OnClick(btnGame, _OnBtnGameClick);
        Button_SetText(btnGame, "Start Game");
        HQC_Container_VectorAdd(_btnList, &btnGame);
        HQC_Log("SceneMenu: Game button created");
    } else {
        HQC_Log("SceneMenu: Failed to create game button");
    }

    HButton btnTest = Button_Create(640, 400);
    if (btnTest) {
        Button_SetText(btnTest, "Test Scene");
        Button_OnClick(btnTest, _OnBtnTestClick);
        HQC_Container_VectorAdd(_btnList, &btnTest);
        HQC_Log("SceneMenu: Test button created");
    } else {
        HQC_Log("SceneMenu: Failed to create test button");
    }

    HButton btnQuit = Button_Create(640, 500);
    if (btnQuit) {
        Button_SetText(btnQuit, "Quit");
        Button_OnClick(btnQuit, _OnBtnQuitClick);
        HQC_Container_VectorAdd(_btnList, &btnQuit);
        HQC_Log("SceneMenu: Quit button created");
    } else {
        HQC_Log("SceneMenu: Failed to create quit button");
    }
    
    HQC_Log("SceneMenu: Total buttons created: %zu", HQC_Container_VectorCount(_btnList));
}

private void _Update() {
    size_t count = HQC_Container_VectorCount(_btnList);
    for (int i = 0; i < count; i++) {
        HButton* btn = HQC_Container_VectorGet(_btnList, i);
        Button_Update(*btn);
    }

    if (_pendingScene) {
        Scene_Change(_pendingScene);
        _pendingScene = NULL;
    }
}

private void _Draw() {
    // Draw background using menu texture
    HQC_Texture menuTexture = Store_GetTextureByID(TEX_MENU);
    if (menuTexture) {
        // Draw menu background sprite
        HQC_Sprite menuBg = Store_GetSpriteByID(SPR_MENU_SCREEN_MAIN);
        if (menuBg) {
            HQC_Artist_DrawSprite(menuBg, 640, 360);
        }
    }
    
    // Draw title
    HQC_Artist_SetColorHex(0xFFFFFF);
    HQC_Artist_DrawText(
        Store_GetFontByID(0), 
        "Zuma HD - Main Menu", 
        640, 150
    );
    
    // Draw instructions
    HQC_Artist_DrawText(
        Store_GetFontByID(0), 
        "Press 1 for Game, 2 for Test, M for Menu, ESC to quit", 
        640, 600
    );

    // Draw buttons
    size_t count = HQC_Container_VectorCount(_btnList);
    for (int i = 0; i < count; i++) {
        HButton* btn = HQC_Container_VectorGet(_btnList, i);
        Button_Draw(*btn);
    }
}

private void _Free() {
    size_t count = HQC_Container_VectorCount(_btnList);
    for (int i = 0; i < count; i++) {
        HButton* btn = HQC_Container_VectorGet(_btnList, i);
        Button_Destroy(*btn);
    }
    HQC_Container_FreeVector(_btnList);
}

HScene Scene_Register_Menu() {
    return Scene_New("menu", _Load, _Update, _Draw, _Free);
}