#include "Application.h"

#include "global/HQC.h"

#include "zuma/ResourceStore.h"
#include "zuma/Scene.h" 
#include "zuma/scenes/Index.h"
#include "zuma/LevelMgr.h"
#include "zuma/GameOptions.h"
#include "zuma/AutoTest.h"
#include "zuma/Statistics.h"
#include "zuma/FloatingText.h"

// Forward declaration for minimal scene
HScene Scene_Register_Minimal();

HScene SC_GAME;
HScene SC_TEST;
HScene SC_MENU;
HScene SC_MINIMAL;

#include <stdlib.h>

#define WINDOW_WIDTH    1280
#define WINDOW_HEIGHT   720

#define MAX_FPS 60
#define FRAME_DELAY (1000 / MAX_FPS)

static struct {
    int curLvl;
    int curDifficulty;

    int inMenu;
    int mouseClicked;

    uint32_t frameStart;
    int frameTime;

    bool isRunning;
} app;


static void _Init() {
    app.curLvl          = gGameOptions.startLevel;
    app.curDifficulty   = gGameOptions.difficulty;
    app.inMenu          = 1;
    app.isRunning       = true;
}


static int _LoadResources(void) {
    if (!LevelMgr_LoadLevels("levels/levels.xml"))
        return 8;

    return 0;
}


static void _HandleEvents(void) {
    HQC_Event event;
    while (HQC_Window_PollEvent(&event)) {
        switch (event) {
            case HQC_EVENT_QUIT:
                exit(0);
                break;
        }
    }

    // 开发用场景快捷键（1=游戏 2=测试场景 M=菜单）。
    // ⚠️ ESC 不在这里处理：菜单场景 ESC=退出，游戏场景 ESC=暂停（由场景自己管）。
    static bool key1Pressed = false;
    static bool key2Pressed = false;
    static bool keyMPressed = false;

    bool key1Current = HQC_Input_IsKeyDown(HQC_KEY_1);
    bool key2Current = HQC_Input_IsKeyDown(HQC_KEY_2);
    bool keyMCurrent = HQC_Input_IsKeyDown(HQC_KEY_M);

    if (key1Current && !key1Pressed) {
        HQC_Log("Switching to game scene");
        Scene_Change(SC_GAME);
    }
    if (key2Current && !key2Pressed) {
        HQC_Log("Switching to test scene");
        Scene_Change(SC_TEST);
    }
    if (keyMCurrent && !keyMPressed) {
        HQC_Log("Switching to menu scene");
        Scene_Change(SC_MENU);
    }

    key1Pressed = key1Current;
    key2Pressed = key2Current;
    keyMPressed = keyMCurrent;
}


static void _Start(void) {
    Store_LoadAll();

    Scene_RegisterAll();

    FloatingTextFactory_Init();
    Statistics_Init();
    Statistics_SetLives(gGameOptions.lives);

    // 起始大关/小关（1-based → 内部 0-based）
    LevelMgr_Reset();
    LevelMgr_SetProgress(gGameOptions.startStage - 1, gGameOptions.startLevel - 1);

    if (AutoTest_IsActive() && !gGameOptions.startAtMenu) {
        HQC_Log("Application: autotest — starting game scene directly");
        Scene_Change(SC_GAME);
        return;
    }

    HQC_Log("Application: Starting menu scene");
    Scene_Change(SC_MENU);
}


static void _Update(void) {
    Scene_Update();
}


static void _Draw(void) {
    HQC_Artist_Clear();
    Scene_Draw();
    HQC_Artist_Display();
}


int ApplicationZuma_Start(void) {
    HQC_Init();
    HQC_CreateWindow(
        "Zuma HD. By GalaxyShad and s4lat", 
        WINDOW_WIDTH, WINDOW_HEIGHT
    );

    LevelMgr_Init();
    _LoadResources();

    _Init();

    _Start();
    HQC_Log("Application: Entering main loop");
    HQC_Artist_SetColorHex(C_BLACK);

    int frameCount = 0;
    while (app.isRunning) {
        frameCount++;

        // 输入锁存：同帧内所有查询结果一致（脚本输入也在这一步生效）
        HQC_Input_Update();

        _HandleEvents();

        if (frameCount % 60 == 0)
            HQC_Log("Application: Frame %d", frameCount);

        app.frameStart = HQC_GetTicks();
        _Update();
        _Draw();
        app.frameTime  = HQC_GetTicks() - app.frameStart;

        if (!gGameOptions.noFrameLimit && app.frameTime < FRAME_DELAY)
            HQC_Delay(FRAME_DELAY - app.frameTime);

        // 截图取证（自动测试/人工检查画面）
        if (gGameOptions.screenshotFrame > 0 && frameCount == gGameOptions.screenshotFrame) {
            if (HQC_Artist_SaveScreenshot(gGameOptions.screenshotPath, WINDOW_WIDTH, WINDOW_HEIGHT))
                HQC_Log("Screenshot saved: %s", gGameOptions.screenshotPath);
            else
                HQC_Log("Screenshot FAILED: %s", gGameOptions.screenshotPath);
        }

        if (AutoTest_IsActive() && frameCount >= AutoTest_MaxFrames()) {
            HQC_Log("Application: autotest frame limit reached (%d)", frameCount);
            AutoTest_Event("FRAME_LIMIT", "frames=%d", frameCount);
            break;
        }

        if (AutoTest_StopRequested()) {
            HQC_Log("Application: autotest stop requested (%d)", frameCount);
            break;
        }
    }

    AutoTest_Report(frameCount, 0);

    HQC_Cleanup();

    return 0;
}