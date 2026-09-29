#include "AutoTest.h"
#include "global/HQC_Env.h"
#include "GameOptions.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#define AUTOTEST_MAX_OBSERVED 64

typedef struct Observed {
    char key[40];
    int  value;
} Observed;

static struct {
    int      active;
    int      frames;
    int      levelsStarted;
    int      levelsCompleted;
    int      gameOvers;
    int      deaths;
    int      explosions;
    int      stopRequested;
    Observed observed[AUTOTEST_MAX_OBSERVED];
    int      observedCount;
} at;


static Observed* _Find(const char* key) {
    for (int i = 0; i < at.observedCount; i++)
        if (strcmp(at.observed[i].key, key) == 0)
            return &at.observed[i];
    return NULL;
}


void AutoTest_Init() {
    memset(&at, 0, sizeof(at));
    at.active = gGameOptions.autotest;

    if (at.active) {
        // 自动测试不许动玩家真实存档：没显式指定就用 /tmp 下的沙箱文件
        // （要测"存档跨进程保留"就自己 export ZUMA_PROGRESS_FILE=...）
        if (!getenv("ZUMA_PROGRESS_FILE"))
            HQC_Env_Set("ZUMA_PROGRESS_FILE", "/tmp/zumahd-autotest-progress.dat", 0);

        AutoTest_Event("AUTOTEST_START", "frames=%d levels=%d stage=%d level=%d difficulty=%d seed=%u",
                       gGameOptions.maxFrames, gGameOptions.levelLimit,
                       gGameOptions.startStage, gGameOptions.startLevel,
                       gGameOptions.difficulty, gGameOptions.seed);
    }
}


int AutoTest_IsActive() {
    return at.active;
}


int AutoTest_MaxFrames() {
    return gGameOptions.maxFrames;
}


void AutoTest_SetPointer(int x, int y, int leftDown, int rightDown) {
    if (!at.active)
        return;

    HQC_Input_SetScripted(true, x, y, leftDown, rightDown);
}


void AutoTest_ReleasePointer() {
    if (!at.active)
        return;

    HQC_Input_SetScripted(true, 640, 360, false, false);
}


void AutoTest_KeyDown(HQC_Key key) {
    if (!at.active)
        return;

    HQC_Input_SetScriptedKey(key, true);
}


void AutoTest_KeyUp(HQC_Key key) {
    if (!at.active)
        return;

    HQC_Input_SetScriptedKey(key, false);
}


void AutoTest_Event(const char* name, const char* format, ...) {
    // 事件计数（供汇总）
    if (strcmp(name, "LEVEL_START") == 0)     at.levelsStarted++;
    if (strcmp(name, "LEVEL_COMPLETE") == 0)  at.levelsCompleted++;
    if (strcmp(name, "GAME_OVER") == 0)       at.gameOvers++;
    if (strcmp(name, "LIFE_LOST") == 0)       at.deaths++;
    if (strcmp(name, "EXPLOSION") == 0)       at.explosions++;

    printf("[TEST] %s", name);

    if (format && format[0]) {
        printf(" ");
        va_list args;
        va_start(args, format);
        vprintf(format, args);
        va_end(args);
    }

    printf("\n");
    fflush(stdout);
}


void AutoTest_Observe(const char* key, int value) {
    if (!at.active || !key)
        return;

    Observed* o = _Find(key);
    if (o) {
        o->value = value;
        return;
    }

    if (at.observedCount >= AUTOTEST_MAX_OBSERVED)
        return;

    snprintf(at.observed[at.observedCount].key, sizeof(at.observed[0].key), "%s", key);
    at.observed[at.observedCount].value = value;
    at.observedCount++;
}


int AutoTest_GetObserved(const char* key) {
    Observed* o = _Find(key);
    return o ? o->value : 0;
}


void AutoTest_RequestStop() {
    at.stopRequested = 1;
}


int AutoTest_StopRequested() {
    return at.stopRequested;
}


void AutoTest_Report(int frames, int exitCode) {
    if (!at.active)
        return;

    at.frames = frames;

    printf("[TEST] AUTOTEST_REPORT frames=%d levels_started=%d levels_completed=%d "
           "game_overs=%d deaths=%d explosions=%d exit=%d\n",
           at.frames, at.levelsStarted, at.levelsCompleted,
           at.gameOvers, at.deaths, at.explosions, exitCode);

    for (int i = 0; i < at.observedCount; i++)
        printf("[TEST] OBSERVED %s=%d\n", at.observed[i].key, at.observed[i].value);

    fflush(stdout);
}