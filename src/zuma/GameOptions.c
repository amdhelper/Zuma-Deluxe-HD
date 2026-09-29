#include "GameOptions.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "../global/HQC.h"
#include "Level.h"

GameOptions gGameOptions;


static int _streq(const char* a, const char* b) {
    return a && b && strcmp(a, b) == 0;
}


void GameOptions_SetDefaults() {
    gGameOptions.autotest     = 0;
    gGameOptions.maxFrames    = 0;
    gGameOptions.startStage   = 1;
    gGameOptions.startLevel   = 1;
    gGameOptions.difficulty   = 0;
    gGameOptions.lives        = 3;
    gGameOptions.levelLimit   = 0;
    gGameOptions.noAudio      = 0;
    gGameOptions.noFrameLimit = 0;
    gGameOptions.autoplay     = 1;
    gGameOptions.startAtMenu  = 0;
    gGameOptions.screenshotFrame = 0;
    gGameOptions.screenshotPath  = "/tmp/zuma-shot.bmp";
    gGameOptions.seed         = 0;
}


void GameOptions_PrintUsage(const char* program) {
    printf(
        "Zuma Deluxe HD\n"
        "usage: %s [options]\n"
        "  --autotest             无头自动测试（脚本输入自动游玩，不锁帧）\n"
        "  --frames N             自动测试帧上限（默认 30000）\n"
        "  --levels N             自动测试最多游玩 N 个小关后退出\n"
        "  --stage N              起始大关（1-based，默认 1）\n"
        "  --level N              起始小关（1-based，默认 1）\n"
        "  --difficulty N         难度 0..3（默认 0）\n"
        "  --lives N              初始命数（默认 3）\n"
        "  --seed N               随机种子\n"
        "  --no-audio             关闭音频\n"
        "  --no-frame-limit       不锁 60fps\n"
        "  --no-autoplay          自动测试时不瞄准开火（用于验证输局/Game Over）\n"
        "  --start-menu           自动测试也从主菜单开始（验证菜单→选关→开打链路）\n"
        "  --screenshot N PATH    在第 N 帧存一张截图（BMP，取证用）\n"
        "  --help                 显示本帮助\n",
        program ? program : "ZumaHD");
}


void GameOptions_Parse(int argc, char** argv) {
    GameOptions_SetDefaults();

    for (int i = 1; i < argc; i++) {
        const char* a = argv[i];

        if (_streq(a, "--help") || _streq(a, "-h")) {
            GameOptions_PrintUsage(argv[0]);
            exit(0);
        } else if (_streq(a, "--autotest")) {
            gGameOptions.autotest = 1;
        } else if (_streq(a, "--no-audio")) {
            gGameOptions.noAudio = 1;
        } else if (_streq(a, "--no-frame-limit")) {
            gGameOptions.noFrameLimit = 1;
        } else if (_streq(a, "--no-autoplay")) {
            gGameOptions.autoplay = 0;
        } else if (_streq(a, "--start-menu")) {
            gGameOptions.startAtMenu = 1;
        } else if (_streq(a, "--screenshot") && i + 2 < argc) {
            gGameOptions.screenshotFrame = atoi(argv[++i]);
            gGameOptions.screenshotPath  = argv[++i];
        } else if (_streq(a, "--frames") && i + 1 < argc) {
            gGameOptions.maxFrames = atoi(argv[++i]);
        } else if (_streq(a, "--levels") && i + 1 < argc) {
            gGameOptions.levelLimit = atoi(argv[++i]);
        } else if (_streq(a, "--stage") && i + 1 < argc) {
            gGameOptions.startStage = atoi(argv[++i]);
        } else if (_streq(a, "--level") && i + 1 < argc) {
            gGameOptions.startLevel = atoi(argv[++i]);
        } else if (_streq(a, "--difficulty") && i + 1 < argc) {
            gGameOptions.difficulty = atoi(argv[++i]);
        } else if (_streq(a, "--lives") && i + 1 < argc) {
            gGameOptions.lives = atoi(argv[++i]);
        } else if (_streq(a, "--seed") && i + 1 < argc) {
            gGameOptions.seed = (unsigned int)strtoul(argv[++i], NULL, 10);
        } else {
            fprintf(stderr, "unknown option: %s (see --help)\n", a);
            exit(2);
        }
    }

    if (gGameOptions.maxFrames <= 0)
        gGameOptions.maxFrames = 30000;

    // 自动测试默认：无头、静音、不锁帧（否则 30000 帧要跑 8 分钟）
    if (gGameOptions.autotest) {
        setenv("SDL_VIDEODRIVER", "dummy", 0);   // 已设置则不覆盖
        gGameOptions.noAudio      = 1;
        gGameOptions.noFrameLimit = 1;
    }

    if (gGameOptions.noAudio)
        setenv("ZUMA_NO_AUDIO", "1", 1);

    if (gGameOptions.seed == 0)
        gGameOptions.seed = (unsigned int)time(NULL);

    srand(gGameOptions.seed);
}


void GameOptions_LogSummary() {
    HQC_Log("[options] autotest=%d frames=%d levels=%d stage=%d level=%d difficulty=%d lives=%d seed=%u",
            gGameOptions.autotest, gGameOptions.maxFrames, gGameOptions.levelLimit,
            gGameOptions.startStage, gGameOptions.startLevel, gGameOptions.difficulty,
            gGameOptions.lives, gGameOptions.seed);
}


// ── 难度覆盖（移植自 v0.1.0 Game_Init 的 4 档预设）──────────────────────────
const char* GameDifficulty_Name(int difficulty) {
    switch (difficulty) {
        case 0: return "Easy";
        case 1: return "Normal";
        case 2: return "Hard";
        case 3: return "Expert";
        default: return "?";
    }
}

void GameDifficulty_Apply(struct LevelSettings* settings, int difficulty) {
    if (!settings) return;

    if (difficulty < 0) difficulty = 0;
    if (difficulty > 3) difficulty = 3;

    switch (difficulty) {
        case 0:
            settings->ballColors     = 4;
            settings->partTime       = 70;
            settings->ballStartCount = 40;
            break;
        case 1:
            settings->ballColors     = 5;
            settings->partTime       = 100;
            settings->ballStartCount = 50;
            break;
        case 2:
            settings->ballColors     = 6;
            settings->repeatChance   = 25;
            settings->partTime       = 120;
            settings->ballStartCount = 60;
            break;
        case 3:
            settings->ballColors     = 6;
            settings->partTime       = 150;
            settings->ballStartCount = 60;
            break;
    }

    if (settings->ballColors > 6) settings->ballColors = 6;
}