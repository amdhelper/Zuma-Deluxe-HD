#include "Index.h"

#include "../Level.h"
#include "../Frog.h"
#include "../BallChain.h"
#include "../FloatingText.h"
#include "../Statistics.h"
#include "../LevelMgr.h"
#include "../Menu.h"
#include "../GameOptions.h"
#include "../AutoTest.h"
#include "../Progress.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ═══════════════════════════════════════════════════════════════════════════════
// 游戏场景（2026-09-29 完整可玩闭环）：
//   生成有限球（settings->start）→ 打满分数槽（score）→ 停止生成 → 清空链 = 过关；
//   球进洞 = 掉命（3 命），命尽 = Game Over。
//   带宝石收集、分数/命/进度 HUD、ESC 暂停菜单、结算对话框（ROADMAP 2.5）、
//   进度存档（2.4，最高分/最佳用时/解锁）、自动测试自动游玩。
// ═══════════════════════════════════════════════════════════════════════════════

struct {
    HFrog  frog;
    HLevel level;
    HBallChain chain;
    HBulletList bulletList;
    HBallChainGenerator generator;

    LevelSettings* settings;
    LevelGraphics* graphics;

    // 宝石（原版 treasure：出现→闪烁→消失，打中 +500）
    int   treasureIndex;
    int   treasureState;      // 0=隐藏 1=出现 2=闪烁 3=消失中
    float treasureTimer;
    float treasureScale;

    // 关卡结束
    int   levelComplete;      // 1 = 过关结算中
    int   gameOver;           // 1 = Game Over
    int   endTimer;           // 结束动画计入 → 弹出结算对话框

    // 结算对话框（ROADMAP 2.5）
    HDialogbox resultDialog;
    int   resultAction;       // 1=下一关 2=重玩本关 3=返回主菜单
    int   resultPrimaryIndex; // 主按钮下标（自动测试点它）
    int   resultDialogFrame;  // 对话框出现后的帧数（自动测试脚本点击计时）
    int   levelFrames;        // 本关用时（帧，60fps）
    int   levelSeconds;       // 本关用时（秒，用于存档/结算）
    char  resultLines[8][96]; // 文本框内容（DialogText 只存指针，故在此保管字符串）

    int   paused;
    int   pauseAction;        // 1=继续 2=重开 3=返回菜单
    HQC_VECTOR(HButton) pauseButtons;

    // 开场动画（ROADMAP 3.2）：火花沿曲线跑一遍 + 关卡名淡入
    int   introTimer;
    int   introMax;
    int   introSkip;

    // 接近洞的紧张感（3.4）：音乐切 NEAR_HOLE + 周期警告音
    int   nearHole;
    int   warnTimer;

    int   effectFrame;        // 道具效果持续帧数（打点用）

    int   frame;
    int   comboWindow;        // 连击窗口（帧）

    HQC_Texture texUI_LevelName;
    HQC_Texture texUI_Status;
} game;


static void GoBack_() { Scene_Change(SC_TEST); }

static void _Game_LogBallSamples(void);   // 自动测试取证（定义在绘制之前）


////////////////////////////////////////////////////////////////////////////////
// 助手
////////////////////////////////////////////////////////////////////////////////

static void _Game_ClearPauseButtons() {
    if (!game.pauseButtons) return;

    size_t n = HQC_Container_VectorCount(game.pauseButtons);
    for (size_t i = 0; i < n; i++) {
        HButton* btn = HQC_Container_VectorGet(game.pauseButtons, i);
        Button_Destroy(*btn);
    }
    HQC_Container_FreeVector(game.pauseButtons);
    game.pauseButtons = NULL;
}


static void _Pause_Resume()  { game.pauseAction = 1; }
static void _Pause_Restart() { game.pauseAction = 2; }
static void _Pause_Quit()    { game.pauseAction = 3; }


static void _Game_BuildPauseButtons() {
    game.pauseButtons = HQC_Container_CreateVector(sizeof(HButton));

    HButton resume = Button_Create(640, 300);
    Button_SetText(resume, "Resume");
    Button_OnClick(resume, _Pause_Resume);
    HQC_Container_VectorAdd(game.pauseButtons, &resume);

    HButton restart = Button_Create(640, 400);
    Button_SetText(restart, "Restart");
    Button_OnClick(restart, _Pause_Restart);
    HQC_Container_VectorAdd(game.pauseButtons, &restart);

    HButton quit = Button_Create(640, 500);
    Button_SetText(quit, "Main Menu");
    Button_OnClick(quit, _Pause_Quit);
    HQC_Container_VectorAdd(game.pauseButtons, &quit);
}


////////////////////////////////////////////////////////////////////////////////
// 结算对话框（ROADMAP 2.5）
//   旧实现只是屏幕上两行字 + 定时自动跳转：看不到连击/用时/历史最高，
//   也不能重玩或返回菜单。现在用 Dialogbox（九宫格菜单精灵）+ 按钮。
////////////////////////////////////////////////////////////////////////////////

static void _Result_Next()   { game.resultAction = 1; }
static void _Result_Replay() { game.resultAction = 2; }
static void _Result_Menu()   { game.resultAction = 3; }


static void _Game_ClearResultDialog() {
    if (game.resultDialog) {
        Dialogbox_Destroy(game.resultDialog);
        game.resultDialog = NULL;
    }

    game.resultDialogFrame = 0;
    game.resultPrimaryIndex = -1;
}


static void _Game_BuildResultDialog() {
    _Game_ClearResultDialog();

    // ⚠️ 0-based：Progress/存档一律 0-based（LevelMgr_GetCurrentStage() 是 1-based，别混用）
    int stage = LevelMgr_CurrentStage0();
    int level = LevelMgr_CurrentLevel0();

    int score    = Statistics_Score();
    int best     = Progress_GetBestScore(stage, level);
    int partTime = game.settings ? game.settings->partTime : 0;
    int secs     = game.levelSeconds;

    HDialogbox dlg = Dialogbox_Create(640, 350);
    game.resultDialog = dlg;

    Dialogbox_SetTitle(dlg, game.levelComplete ? "LEVEL COMPLETE" : "GAME OVER");

    // 用时：在 partTime 内 = 绿色（原版标准），超过 2 倍 = 红色
    uint32_t timeColor = C_WHITE;
    if (partTime > 0) {
        if (secs <= partTime)        timeColor = C_GREEN;
        else if (secs > partTime * 2) timeColor = C_RED;
    }

    const float labX = -150.0f;
    const float valX =  130.0f;

    Dialogbox_AddText(dlg, "Score", labX, -180);
    snprintf(game.resultLines[1], 96, "%d", score);
    Dialogbox_AddTextEx(dlg, game.resultLines[1], valX, -180, C_YELLOW);

    snprintf(game.resultLines[2], 96, "Gems %d", Statistics_Coins());
    Dialogbox_AddTextEx(dlg, game.resultLines[2], labX, -138, C_WHITE);

    snprintf(game.resultLines[3], 96, "Max combo %d  (chain %d)",
             Statistics_MaxCombo(), Statistics_MaxChainBonus());
    Dialogbox_AddTextEx(dlg, game.resultLines[3], valX, -138, C_WHITE);

    snprintf(game.resultLines[4], 96, "Time %d:%02d", secs / 60, secs % 60);
    Dialogbox_AddTextEx(dlg, game.resultLines[4], labX, -96, timeColor);

    snprintf(game.resultLines[5], 96, "Best %d", best);
    Dialogbox_AddTextEx(dlg, game.resultLines[5], valX, -96, C_WHITE);

    snprintf(game.resultLines[6], 96, "Powerups used %d", BallChain_PowerupsUsed());
    Dialogbox_AddTextEx(dlg, game.resultLines[6], 0, -60, C_CYAN);

    // 按钮：主按钮放最后（= 最下面），符合"主要动作在底部"
    HButton replay = Button_Create(640, 0);
    Button_SetText(replay, "Replay Level");
    Button_OnClick(replay, _Result_Replay);
    Dialogbox_AddButton(dlg, replay);

    HButton menu = Button_Create(640, 0);
    Button_SetText(menu, "Main Menu");
    Button_OnClick(menu, _Result_Menu);
    Dialogbox_AddButton(dlg, menu);

    HButton primary = Button_Create(640, 0);

    if (game.levelComplete) {
        Button_SetText(primary, "Next Level");
        Button_OnClick(primary, _Result_Next);
        game.resultPrimaryIndex = 2;
    } else {
        Button_SetText(primary, "Retry");
        Button_OnClick(primary, _Result_Replay);
        game.resultPrimaryIndex = 2;
    }

    Dialogbox_AddButton(dlg, primary);

    // ── 结算对话框（ROADMAP 2.5）——弹出来那一帧存图取证
    GameOptions_RequestResultScreenshot();

    AutoTest_Event("RESULT_DIALOG",
                   "result=%s score=%d gems=%d maxcombo=%d maxchain=%d time=%d partTime=%d best=%d",
                   game.levelComplete ? "complete" : "gameover",
                   score, Statistics_Coins(), Statistics_MaxCombo(),
                   Statistics_MaxChainBonus(), secs, partTime, best);
}


// 自动测试：结算对话框出现后点主按钮（Next Level / Retry）
static void _Game_ResultAutoplay() {
    if (!AutoTest_IsActive() || !game.resultDialog)
        return;

    game.resultDialogFrame++;

    if (game.levelComplete) {
        float bx = 0, by = 0;
        Dialogbox_GetButtonPos(game.resultDialog, game.resultPrimaryIndex, &bx, &by);

        if (game.resultDialogFrame == 20) AutoTest_SetPointer((int)bx, (int)by, 1, 0);
        if (game.resultDialogFrame == 22) AutoTest_SetPointer((int)bx, (int)by, 0, 0);
        if (game.resultDialogFrame == 30) AutoTest_Event("RESULT_CLICK_PRIMARY", "x=%d y=%d", (int)bx, (int)by);
    } else {
        AutoTest_ReleasePointer();

        // Game Over：留 90 帧给截图取证，然后收工（旧实现是直接退出，没有对话框）
        if (game.resultDialogFrame > 90)
            AutoTest_RequestStop();
    }
}


// 结算动作（按钮回调 → 下一关 / 重玩 / 回菜单）
static void _Game_HandleResultAction(int action) {
    int stage = LevelMgr_CurrentStage0();     // 0-based
    int level = LevelMgr_CurrentLevel0();

    if (action == 1) {                                     // 下一关
        AutoTest_Event("RESULT_NEXT", "from=%d-%d", stage + 1, level + 1);

        if (AutoTest_IsActive() && gGameOptions.levelLimit > 0 &&
            Statistics_LevelsCompleted() >= gGameOptions.levelLimit) {
            AutoTest_Event("LEVEL_LIMIT_REACHED", "completed=%d", Statistics_LevelsCompleted());
            AutoTest_RequestStop();
            return;
        }

        if (LevelMgr_AdvanceLevel()) {
            Scene_Change(SC_GAME);
        } else {
            HQC_Log("All stages complete! Adventure finished.");
            AutoTest_Event("ADVENTURE_COMPLETE", "coins=%d", Statistics_Coins());

            if (AutoTest_IsActive()) { AutoTest_RequestStop(); return; }
            Scene_Change(SC_MENU);
        }
        return;
    }

    if (action == 2) {                                     // 重玩本关
        AutoTest_Event("RESULT_REPLAY", "stage=%d level=%d", stage + 1, level + 1);

        // Game Over 后重玩 = 新的一局（补满命）
        if (Statistics_Lives() <= 0) {
            Statistics_Init();
            Statistics_SetLives(gGameOptions.lives);
        }

        Scene_Change(SC_GAME);
        return;
    }

    AutoTest_Event("RESULT_MENU", "stage=%d level=%d", stage + 1, level + 1);
    Scene_Change(SC_MENU);
}


static void _Game_SetLevelText() {
    if (game.texUI_LevelName) {
        HQC_Artist_FreeTexture(game.texUI_LevelName);
        game.texUI_LevelName = NULL;
    }

    const char* name = Level_GetDisplayName(game.level);
    if (name)
        game.texUI_LevelName = HQC_Artist_CreateTextTexture(Store_GetFontByID(FONT_CANCUN_12), name, 0xFFFFFF);
}


////////////////////////////////////////////////////////////////////////////////
// 关卡装载 / 重开
////////////////////////////////////////////////////////////////////////////////

static void _Game_UnloadObjects() {
    if (game.generator)  { BallChainGenerator_Destroy(game.generator); game.generator = NULL; }
    if (game.chain)      { BallChain_Destroy(game.chain); game.chain = NULL; }
    if (game.frog)       { Frog_Destroy(game.frog); game.frog = NULL; }
    if (game.bulletList) { BulletList_Free(game.bulletList); game.bulletList = NULL; }
    if (game.level)      { Level_Free(game.level); game.level = NULL; }
}


static int _Game_LoadLevel() {
    LevelSettings* settings = LevelMgr_GetCurrentSettings();
    LevelGraphics* gx       = LevelMgr_GetCurrentGraphics();

    if (!settings || !gx) {
        HQC_Log("LevelMgr has no level for current progress — back to menu");
        Scene_Change(SC_MENU);
        return 0;
    }

    // settings 副本：难度覆盖只改副本，不污染 LevelMgr 注册表（否则重开本关会叠加）
    static LevelSettings settingsCopy;
    settingsCopy = *settings;
    GameDifficulty_Apply(&settingsCopy, gGameOptions.difficulty);

    // Gauntlet（ROADMAP 3.6）：在难度副本上再叠速度倍率/本目目标分
    // ⚠️ 必须在 Level_Load 之前改好：关卡会记住这份 settings
    if (gGameOptions.gauntlet) {
        settingsCopy.ballSpd    *= gGameOptions.gauntletSpeedMul;
        settingsCopy.gaugeScore  = gGameOptions.gauntletGauge;
    }

    settings = &settingsCopy;

    game.settings = settings;
    game.graphics = gx;

    game.level = Level_Load(settings, gx);
    if (!game.level) {
        HQC_Log("Failed to load level, returning to menu");
        Scene_Change(SC_MENU);
        return 0;
    }

    game.bulletList = BulletList_Create();
    game.frog       = Frog_Create(gx->frogPos.x, gx->frogPos.y, game.bulletList);
    game.chain      = BallChain_Create(game.level, game.bulletList);
    game.generator  = BallChainGenerator_Create(game.chain);

    BallChain_Configure(game.chain, settings);
    Frog_Configure(game.frog, settings->ballColors);
    BallChainGenerator_SetInitialCount(game.generator, settings->ballStartCount);

    if (gGameOptions.gauntlet) {
        AutoTest_Event("GAUNTLET_START", "wave=%d difficulty=%d speed=%.2f gauge=%d colors=%d",
                       gGameOptions.gauntletWave, gGameOptions.gauntletDifficulty,
                       settings->ballSpd, gGameOptions.gauntletGauge, settings->ballColors);
    }

    game.treasureState = 0;
    game.treasureTimer = 300;    // 5 秒后出现第一颗宝石
    game.treasureScale = 1.0f;
    game.treasureIndex = 0;

    game.levelComplete = 0;
    game.gameOver      = 0;
    game.endTimer      = 0;
    game.paused        = 0;
    game.pauseAction   = 0;
    game.frame         = 0;
    game.comboWindow   = 0;

    game.levelFrames   = 0;
    game.levelSeconds  = 0;

    // 开场动画（自动测试里缩短，别把帧预算耗在动画上）
    game.introTimer = AutoTest_IsActive() ? 20 : 100;
    game.introMax   = game.introTimer;
    game.introSkip  = 0;

    game.nearHole   = 0;
    game.warnTimer  = 60;
    game.effectFrame = 0;

    _Game_ClearResultDialog();

    Statistics_ResetLevel();

    _Game_SetLevelText();

    Store_PlayMusic(MUS_GAME);

    AutoTest_Event("LEVEL_START", "stage=%d level=%d settings=%s speed=%.2f start=%d gauge=%d colors=%d",
                   LevelMgr_GetCurrentStage(), LevelMgr_GetCurrentLevelIndex(),
                   settings->id ? settings->id : "?", settings->ballSpd,
                   settings->ballStartCount, settings->gaugeScore, settings->ballColors);

    AutoTest_Observe("levels_started", LevelMgr_GetCurrentStage() * 100 + LevelMgr_GetCurrentLevelIndex());

    return 1;
}


static void _Game_Start_() {
    HQC_Log("Starting game scene...");

    _Game_ClearPauseButtons();

    if (!_Game_LoadLevel())
        return;

    HQC_Log("Game scene started (stage %d level %d)",
            LevelMgr_GetCurrentStage(), LevelMgr_GetCurrentLevelIndex());
}


// 掉命重开本关（进度不变）
static void _Game_RestartLevel() {
    _Game_UnloadObjects();
    _Game_LoadLevel();

    HQC_Log("Level restarted (lives left: %d)", Statistics_Lives());
    AutoTest_Event("LEVEL_RESTART", "lives=%d", Statistics_Lives());
}


////////////////////////////////////////////////////////////////////////////////
// 宝石
////////////////////////////////////////////////////////////////////////////////

static void _Game_UpdateTreasure() {
    int count = Level_GetTreasureCount(game.level);
    if (count <= 0) return;

    game.treasureTimer -= 1.0f;

    if (game.treasureState == 0) {
        if (game.treasureTimer <= 0.0f) {
            game.treasureIndex = rand() % count;
            game.treasureState = 1;                 // 出现
            game.treasureTimer = 600.0f;            // 10 秒寿命
            game.treasureScale = 1.0f;

            HQC_DJ_PlaySound(Store_GetSoundByID(SND_JEWELAPPEAR));
        }
        return;
    }

    if (game.treasureState == 1) {
        if (game.treasureTimer < 180.0f) {          // 最后 3 秒闪烁
            game.treasureState = 2;
            HQC_DJ_PlaySound(Store_GetSoundByID(SND_WARNING1));
        }
    } else if (game.treasureState == 2) {
        if (game.treasureTimer <= 0.0f) {
            game.treasureState = 0;
            game.treasureTimer = 420.0f;
            HQC_DJ_PlaySound(Store_GetSoundByID(SND_GEMVANISHES));
            return;
        }
    }

    // 命中判定：打中 +500 且累计宝石数
    v2f_t pos = Level_GetTreasurePos(game.level, game.treasureIndex);

    if (BulletList_TryHitPoint(game.bulletList, pos, 54.0f)) {
        Statistics_AddScore(500);
        Statistics_AddCoin();

        FloatingTextFactory_Instantiate(pos.x, pos.y, 0xFFD700, "BONUS +500");

        HQC_DJ_PlaySound(Store_GetSoundByID(SND_COINGRAB));

        game.treasureState = 0;
        game.treasureTimer = 420.0f;

        AutoTest_Event("TREASURE_COLLECTED", "coins=%d index=%d",
                       Statistics_Coins(), game.treasureIndex);
    }
}


static void _Game_DrawTreasure() {
    if (game.treasureState == 0) return;

    v2f_t pos = Level_GetTreasurePos(game.level, game.treasureIndex);

    HQC_Animation anim = Store_GetAnimationByID(ANIM_COIN);
    HQC_Animation_Tick(anim);

    // 闪烁：隔帧画
    if (game.treasureState == 2 && (game.frame / 8) % 2 == 0)
        return;

    HQC_Artist_DrawAnimation(anim, pos.x, pos.y);
}


////////////////////////////////////////////////////////////////////////////////
// 胜负判定
////////////////////////////////////////////////////////////////////////////////

static void _Game_CheckEnd() {
    if (game.levelComplete || game.gameOver)
        return;

    int chainEmpty  = BallChain_IsEmpty(game.chain);
    int endReached  = BallChain_IsEndReached(game.chain);

    // ── 输：球进洞（链坠空）────────────────────────────────────────────────
    if (chainEmpty && endReached) {
        int lives = Statistics_LoseLife();

        game.levelSeconds = game.levelFrames / 60;

        Progress_ReportLevel(LevelMgr_CurrentStage0(), LevelMgr_CurrentLevel0(),
                             Statistics_Score(), game.levelSeconds, 0);

        AutoTest_Event("LIFE_LOST", "lives=%d score=%d stage=%d level=%d",
                       lives, Statistics_Score(),
                       LevelMgr_GetCurrentStage(), LevelMgr_GetCurrentLevelIndex());

        if (lives > 0) {
            HQC_DJ_PlaySound(Store_GetSoundByID(SND_CHANT14));
            _Game_RestartLevel();
        } else {
            game.gameOver = 1;
            game.endTimer = 60;      // 短暂定格 → 结算对话框

            Store_PlayMusic(MUS_GAME_OVER);
            HQC_DJ_PlaySound(Store_GetSoundByID(SND_CHANT8));

            AutoTest_Event("GAME_OVER", "score=%d coins=%d stage=%d level=%d",
                           Statistics_Score(), Statistics_Coins(),
                           LevelMgr_GetCurrentStage(), LevelMgr_GetCurrentLevelIndex());
        }
        return;
    }

    if (endReached)
        return;

    // ── Gauntlet：分数过本目目标 = 进下一目（越来越快，随机换图）──────────────
    if (gGameOptions.gauntlet) {
        if (Statistics_Score() > gGameOptions.gauntletGauge) {
            gGameOptions.gauntletWave++;
            gGameOptions.gauntletGauge += 1000 + 250 * gGameOptions.gauntletDifficulty;
            gGameOptions.gauntletSpeedMul *= 1.06f;

            HQC_DJ_PlaySound(Store_GetSoundByID(SND_CHANT4));
            Store_PlayMusic(MUS_GAUNTLET);

            AutoTest_Event("GAUNTLET_WAVE", "wave=%d score=%d nextGauge=%d speedMul=%.2f",
                           gGameOptions.gauntletWave, Statistics_Score(),
                           gGameOptions.gauntletGauge, gGameOptions.gauntletSpeedMul);
            AutoTest_Observe("gauntlet_wave", gGameOptions.gauntletWave);

            if (AutoTest_IsActive() && gGameOptions.levelLimit > 0 &&
                gGameOptions.gauntletWave > gGameOptions.levelLimit) {
                AutoTest_Event("LEVEL_LIMIT_REACHED", "waves=%d", gGameOptions.gauntletWave - 1);
                AutoTest_RequestStop();
                return;
            }

            int sc = LevelMgr_GetStageCount();
            if (sc > 0) {
                int st = rand() % sc;
                int lc = LevelMgr_GetLevelCount(st);
                int lv = lc > 0 ? rand() % lc : 0;

                LevelMgr_SetProgress(st, lv);
                Scene_Change(SC_GAME);
            }
        }

        return;   // Gauntlet 不做"清空链 = 过关"
    }

    // ── 分数槽打满 → 停止生成（余下的球必须清掉才过关，原版行为）──────────
    if (BallChain_IsGenerating(game.chain) && Statistics_Score() > game.settings->gaugeScore) {
        BallChain_SetGenerating(game.chain, false);
        BallChainGenerator_Stop(game.generator);

        HQC_DJ_PlaySound(Store_GetSoundByID(SND_CHANT4));
        AutoTest_Event("GAUGE_REACHED", "score=%d gauge=%d", Statistics_Score(), game.settings->gaugeScore);
    }

    // ── 赢：分数过线 + 链清空 ──────────────────────────────────────────────
    if (chainEmpty && BallChainGenerator_IsFinished(game.generator)) {
        game.levelComplete = 1;
        game.endTimer      = 60;      // 短暂定格 → 结算对话框

        game.levelSeconds = game.levelFrames / 60;

        Statistics_AddLevelCompleted();

        Store_PlayMusic(MUS_WIN);
        HQC_DJ_PlaySound(Store_GetSoundByID(SND_CHANT2));

        AutoTest_Event("LEVEL_COMPLETE", "score=%d coins=%d lives=%d stage=%d level=%d",
                       Statistics_Score(), Statistics_Coins(), Statistics_Lives(),
                       LevelMgr_GetCurrentStage(), LevelMgr_GetCurrentLevelIndex());

        AutoTest_Observe("last_score", Statistics_Score());
        AutoTest_Observe("levels_completed", Statistics_LevelsCompleted());
        AutoTest_Observe("coins", Statistics_Coins());

        // 进度存档：最高分 / 最佳用时 / 解锁下一关（ROADMAP 2.4）
        Progress_ReportLevel(LevelMgr_CurrentStage0(), LevelMgr_CurrentLevel0(),
                             Statistics_Score(), game.levelSeconds, 1);
    }
}


// 结束计时（过关 / Game Over）→ 弹出结算对话框 → 等玩家选按钮
static void _Game_UpdateEnd() {
    if (!game.levelComplete && !game.gameOver)
        return;

    // 对话框已弹出：更新按钮，等动作
    if (game.resultDialog) {
        Dialogbox_Update(game.resultDialog);

        _Game_ResultAutoplay();

        if (game.resultAction) {
            int action = game.resultAction;
            game.resultAction = 0;

            _Game_ClearResultDialog();
            _Game_HandleResultAction(action);
        }
        return;
    }

    game.endTimer--;

    if (game.endTimer > 0)
        return;

    _Game_BuildResultDialog();
}


////////////////////////////////////////////////////////////////////////////////
// 自动测试：自动游玩
////////////////////////////////////////////////////////////////////////////////

static void _Game_Autoplay() {
    if (!AutoTest_IsActive())
        return;

    if (!gGameOptions.autoplay) {
        AutoTest_ReleasePointer();
        return;
    }

    // 宝石出现时优先打宝石（顺便覆盖宝石链路）
    if (game.treasureState != 0) {
        int count = Level_GetTreasureCount(game.level);
        if (count > 0) {
            v2f_t gp = Level_GetTreasurePos(game.level, game.treasureIndex);
            int leftClick = (game.frame % 18) < 2;
            AutoTest_SetPointer((int)gp.x, (int)gp.y, leftClick, 0);
            return;
        }
    }

    int len = BallChain_Length(game.chain);

    BallColor want = Frog_GetBallColor(game.frog);

    // 选目标：优先"当前颜色且最靠洞"的球（最紧急）；顺手判断该颜色在不在场上
    HBall target = NULL;
    BallColor targetColor = BALL_NONE;
    int colorInChain = 0;

    for (int i = len - 1; i >= 0; i--) {
        HBall ball = BallChain_GetBallAt(game.chain, i);
        if (!ball || Ball_IsExploding(ball)) continue;

        BallColor c = Ball_GetColor(ball);
        if (c == want) { target = ball; targetColor = c; colorInChain = 1; break; }
    }

    if (!target) {
        // 没有当前颜色的球 → 找场上最靠洞的球，准备换色
        for (int i = len - 1; i >= 0; i--) {
            HBall ball = BallChain_GetBallAt(game.chain, i);
            if (!ball || Ball_IsExploding(ball)) continue;

            target = ball;
            targetColor = Ball_GetColor(ball);
            break;
        }

        for (int i = 0; i < len; i++) {
            HBall ball = BallChain_GetBallAt(game.chain, i);
            if (ball && !Ball_IsExploding(ball) && Ball_GetColor(ball) == want) { colorInChain = 1; break; }
        }
    }

    if (!target) {
        AutoTest_ReleasePointer();
        return;
    }

    v2f_t tp = Ball_GetPositionCoords(target);

    // 当前颜色不在场上 → 右键换球（每隔几帧一次，别刷屏）
    int rightClick = 0;
    if (!colorInChain && (game.frame % 30 == 0))
        rightClick = 1;

    // 发射节奏：每 18 帧点一次（按住 2 帧）
    int leftClick = (game.frame % 18) < 2;

    AutoTest_SetPointer((int)tp.x, (int)tp.y, leftClick, rightClick);
}


////////////////////////////////////////////////////////////////////////////////
// 场景回调
////////////////////////////////////////////////////////////////////////////////

static void Game_Update__() {
    if (!game.level)
        return;

    game.frame++;

    // ── 开场动画（ROADMAP 3.2）：火花沿曲线跑一遍 + 关卡名，期间球链静止 ──
    if (game.introTimer > 0) {
        game.introTimer--;

        // 玩家点击/空格可跳过（自动测试不跳，保证动画帧数可断言）
        if (!AutoTest_IsActive() &&
            (HQC_Input_MouseLeftPressed() || HQC_Input_KeyPressed(HQC_KEY_SPACE))) {
            game.introTimer = 0;
        }

        if (game.introTimer == 0) {
            game.introSkip = 1;
            AutoTest_Event("INTRO_DONE", "frames=%d stage=%d level=%d",
                           game.introMax, LevelMgr_GetCurrentStage(), LevelMgr_GetCurrentLevelIndex());
            Statistics_ResetLevel();
        }

        FloatingTextFactory_Update();
        return;
    }

    if (Statistics_Lives() <= 0 && !game.gameOver) {
        // 兜底（正常由 _Game_CheckEnd 触发）
        game.gameOver = 1;
        game.endTimer = 300;
    }

    // ── 暂停 ───────────────────────────────────────────────────────────────
    if (HQC_Input_KeyPressed(HQC_KEY_ESCAPE) && !game.levelComplete && !game.gameOver) {
        game.paused = !game.paused;
        HQC_DJ_PlaySound(Store_GetSoundByID(SND_BUTTON1));
    }

    if (game.paused) {
        if (!game.pauseButtons)
            _Game_BuildPauseButtons();

        size_t n = HQC_Container_VectorCount(game.pauseButtons);
        for (size_t i = 0; i < n; i++) {
            HButton* btn = HQC_Container_VectorGet(game.pauseButtons, i);
            Button_Update(*btn);
        }

        if (game.pauseAction) {
            int action = game.pauseAction;
            game.pauseAction = 0;
            game.paused = 0;
            _Game_ClearPauseButtons();

            if (action == 1) {
                // 继续
            } else if (action == 2) {
                HQC_Log("Restart from pause menu");
                _Game_RestartLevel();
            } else {
                Scene_Change(SC_MENU);
            }
        }
        return;
    }

    // ── 关卡结束流程 ───────────────────────────────────────────────────────
    if (game.levelComplete || game.gameOver) {
        _Game_UpdateEnd();
        FloatingTextFactory_Update();
        return;
    }

    // ── 生成速度：靠近洞就减速（原版 slowFactor）+ 道具效果倍率（3.1）──────
    float baseSpeed = game.settings->ballSpd;

    if (!BallChain_IsEndReached(game.chain)) {
        if (BallChain_FrontProgress(game.chain) > 0.8f)
            baseSpeed = baseSpeed / game.settings->slowFactor;

        // 减速/暂停道具：倍率在 BallChain 里倒计时（每帧取一次）
        float mul = BallChain_GetSpeedMultiplier(game.chain);
        float speed = baseSpeed * mul;

        BallChain_SetSpeed(game.chain, speed);

        // 效果生效期打点：证明链速真的被乘过（比如暂停 = mul 0.00）
        if (mul != 1.0f) {
            if (game.effectFrame == 0)
                AutoTest_Event("EFFECT_START", "mul=%.2f speed=%.3f base=%.2f", mul, speed, baseSpeed);

            if (game.effectFrame % 30 == 0)
                AutoTest_Event("EFFECT_ACTIVE", "mul=%.2f speed=%.3f", mul, speed);

            game.effectFrame++;
        } else if (game.effectFrame > 0) {
            AutoTest_Event("EFFECT_END", "frames=%d", game.effectFrame);
            game.effectFrame = 0;
        }
    }

    // ── 接近洞的紧张感（ROADMAP 3.4）：音乐切 NEAR_HOLE + 周期警告音 ────────
    {
        int progress = (int)(BallChain_FrontProgress(game.chain) * 100);
        int nearHole = progress > 80;

        if (nearHole != game.nearHole) {
            game.nearHole = nearHole;

            Store_PlayMusic(nearHole ? MUS_NEAR_HOLE
                                     : (gGameOptions.gauntlet ? MUS_GAUNTLET : MUS_GAME));

            AutoTest_Event("MUSIC", "near_hole=%d front=%d%%", nearHole, progress);
        } else if (game.nearHole) {
            if (--game.warnTimer <= 0) {
                game.warnTimer = 50;
                HQC_DJ_PlaySound(Store_GetSoundByID(SND_WARNING1));
            }
        }
    }

    // ── 主循环更新 ─────────────────────────────────────────────────────────
    _Game_Autoplay();

    game.levelFrames++;      // 本关用时（结算 / 最佳用时）

    Frog_Update(game.frog);
    BallChain_Update(game.chain);
    BallChainGenerator_Update(game.generator);
    BulletList_Update(game.bulletList);
    BulletList_UpdateChainCollisions(game.bulletList, game.chain);
    FloatingTextFactory_Update();
    _Game_UpdateTreasure();

    // 连击链：爆炸动画期间保持连击窗口，窗口过后断开
    if (BallChain_ExplodingCount(game.chain) > 0)
        game.comboWindow = 60;
    else if (game.comboWindow > 0)
        game.comboWindow--;
    else
        Statistics_BreakChain();

    // 青蛙只发场上存在的颜色（原版行为，避免发死色）
    if (BallChain_Length(game.chain) > 0) {
        if (!BallChain_ColorIsInChain(game.chain, Frog_GetBallColor(game.frog)) &&
            !BallChain_ColorIsInChain(game.chain, Frog_GetNextBallColor(game.frog))) {
            // 场上一颗都没有 → 随机换个存在的颜色
            for (int i = 0; i < BallChain_Length(game.chain); i++) {
                HBall ball = BallChain_GetBallAt(game.chain, i);
                if (ball && !Ball_IsExploding(ball)) {
                    Frog_SetColors(game.frog, Ball_GetColor(ball), Frog_GetNextBallColor(game.frog));
                    break;
                }
            }
        }
    }

    _Game_CheckEnd();

    _Game_LogBallSamples();

    AutoTest_Observe("score", Statistics_Score());
    AutoTest_Observe("lives", Statistics_Lives());
    AutoTest_Observe("chain_len", BallChain_Length(game.chain));
    AutoTest_Observe("front_progress_x1000", (int)(BallChain_FrontProgress(game.chain) * 1000));
}


// 自动测试取证：把球链上若干球的位置/颜色打出来，配合截图做像素级校验
// （验证"画出来的球 = 模拟的球"，旧 bug 正是球位置回绕导致画面与模拟不符）
static void _Game_LogBallSamples() {
    if (!AutoTest_IsActive()) return;
    if (gGameOptions.screenshotFrame <= 0) return;
    if (game.frame != gGameOptions.screenshotFrame) return;

    int len = BallChain_Length(game.chain);
    int logged = 0;

    for (int i = 0; i < len && logged < 8; i++) {
        HBall ball = BallChain_GetBallAt(game.chain, i);
        if (!ball || Ball_IsExploding(ball)) continue;

        v2f_t p = Ball_GetPositionCoords(ball);
        AutoTest_Event("BALL_SAMPLE", "idx=%d x=%d y=%d color=%d pos=%d",
                       i, (int)p.x, (int)p.y, (int)Ball_GetColor(ball),
                       (int)Ball_GetPositionOnCurve(ball));
        logged++;
    }

    AutoTest_Event("BALL_SAMPLE_DONE", "frame=%d chain_len=%d logged=%d",
                   game.frame, len, logged);
}


////////////////////////////////////////////////////////////////////////////////
// 绘制
////////////////////////////////////////////////////////////////////////////////

// 开场动画：火花沿曲线跑一遍 + 关卡名缩放淡入（ROADMAP 3.2）
static void _Game_DrawIntro() {
    if (game.introTimer <= 0 || !game.level)
        return;

    float t = 1.0f - (float)game.introTimer / (float)(game.introMax > 0 ? game.introMax : 1);

    float len = (float)Level_GetCurveLength(game.level);

    if (len > 0) {
        HQC_Animation sparkle = Store_GetAnimationByID(ANIM_SPARKLE);
        HQC_Animation_Tick(sparkle);

        for (int i = 0; i < 18; i++) {
            float p = t * len * 1.15f - i * 42.0f;

            if (p < 0.0f || p > len)
                continue;

            v2f_t c = Level_GetCurveCoords(game.level, p);

            HQC_Artist_DrawAnimation(sparkle, c.x, c.y);
        }
    }

    // 关卡名：0→0.5 淡入放大，0.5→1 淡出
    if (game.texUI_LevelName) {
        float alpha = t < 0.6f ? (t / 0.6f) : (1.0f - t) / 0.4f;
        if (alpha < 0.0f) alpha = 0.0f;
        if (alpha > 1.0f) alpha = 1.0f;

        HQC_Artist_DrawSetAlpha(alpha);
        HQC_Artist_DrawSetScale(0.7f + 0.5f * t);
        HQC_Artist_DrawTexture(game.texUI_LevelName, 640, 330);
        HQC_Artist_DrawSetScale(1.0f);
        HQC_Artist_DrawSetAlpha(1.0f);
    }

    if (AutoTest_IsActive()) {
        char buff[64];
        snprintf(buff, sizeof(buff), "INTRO %d%%", (int)(t * 100));
        HQC_Artist_SetColorHex(C_WHITE);
        HQC_Artist_DrawText(Store_GetFontByID(FONT_CANCUN_8), buff, 640, 120);
    }
}


static void _Game_DrawHUD() {
    HQC_Artist_SetColorHex(C_BLACK);
    HQC_Artist_DrawSprite(Store_GetSpriteByID(SPR_GAME_HUD_BORDER), 640, 360);

    char buff[64];

    // 分数（右上）
    snprintf(buff, sizeof(buff), "%d", Statistics_Score());
    HQC_Artist_SetColorHex(C_YELLOW);
    HQC_Artist_DrawText(Store_GetFontByID(FONT_CANCUN_13), buff, 545, 20);

    // 关卡号
    snprintf(buff, sizeof(buff), "lvl%d-%d", LevelMgr_GetCurrentStage(), LevelMgr_GetCurrentLevelIndex());
    HQC_Artist_DrawText(Store_GetFontByID(FONT_CANCUN_10), buff, 430, 20);

    // Gauntlet：显示目数与当前目标分
    if (gGameOptions.gauntlet) {
        snprintf(buff, sizeof(buff), "wave %d  target %d", gGameOptions.gauntletWave, gGameOptions.gauntletGauge);
        HQC_Artist_SetColorHex(C_CYAN);
        HQC_Artist_DrawText(Store_GetFontByID(FONT_CANCUN_8), buff, 430, 42);
        HQC_Artist_SetColorHex(C_YELLOW);
    }

    // 命（青蛙图标）
    HQC_Sprite live = Store_GetSpriteByID(SPR_GAME_HUD_LIVE);
    int lives = Statistics_Lives();
    for (int i = 0; i < lives && i < 6; i++)
        HQC_Artist_DrawSprite(live, 57 + 40 * i, 22);

    // 分数槽（进度条）：达到 gauge 变黄
    int gauge = game.settings ? game.settings->gaugeScore : 1000;
    int w = 0;
    if (gauge > 0) {
        w = (int)(94.0f * (float)Statistics_Score() / (float)gauge);
        if (w > 94) w = 94;
        if (w < 0)  w = 0;
    }

    HQC_Texture hudTex = Store_GetTextureByID(TEX_GAME_HUD);

    irect_t emptyRect = { 773, 28, 94, 27 };
    HQC_Artist_DrawTextureRect(hudTex, 820, 16, emptyRect);

    if (w > 0) {
        irect_t fillRect = { 773, 0, w, 27 };
        HQC_Artist_DrawTextureRect(hudTex, 773 + w / 2, 16, fillRect);
    }

    // 宝石数
    snprintf(buff, sizeof(buff), "%d", Statistics_Coins());
    HQC_Artist_DrawText(Store_GetFontByID(FONT_CANCUN_8), buff, 700, 20);

    // HUD 菜单按钮（点击暂停）
    HQC_Sprite btn = Store_GetSpriteByID(SPR_GAME_HUD_BTN_MENU);
    HQC_Artist_DrawSprite(btn, 1128, 22);

    HQC_Artist_SetColorHex(C_WHITE);

    // 自动测试：把关键数据画在屏幕上（截图取证用）
    if (AutoTest_IsActive()) {
        snprintf(buff, sizeof(buff), "score=%d chain=%d lives=%d front=%d%%",
                 Statistics_Score(), BallChain_Length(game.chain), Statistics_Lives(),
                 (int)(BallChain_FrontProgress(game.chain) * 100));
        HQC_Artist_DrawText(Store_GetFontByID(FONT_CANCUN_8), buff, 640, 70);
    }
}


static void _Game_DrawPit() {
    if (!game.level) return;

    v2f_t pit = Level_GetPitPos(game.level);

    // 骷髅（洞）：球越靠近开得越大
    HQC_Animation skull = Store_GetAnimationByID(ANIM_SKULL);
    float progress = BallChain_FrontProgress(game.chain);
    int frame = (int)(11.0f * progress);
    if (frame > 11) frame = 11;

    HQC_Animation_SetFrame(skull, frame);
    HQC_Artist_DrawAnimation(skull, pit.x, pit.y);
}


static void _Game_DrawOverlay() {
    if (game.paused) {
        HQC_Color dim = { 16, 16, 16, 200 };
        HQC_Artist_SetColor(dim);
        HQC_Artist_FillRect(0, 0, 1280, 720);

        HQC_Artist_SetColorHex(C_WHITE);
        HQC_Artist_DrawText(Store_GetFontByID(FONT_CANCUN_13), "PAUSED", 640, 180);

        size_t n = HQC_Container_VectorCount(game.pauseButtons);
        for (size_t i = 0; i < n; i++) {
            HButton* btn = HQC_Container_VectorGet(game.pauseButtons, i);
            Button_Draw(*btn);
        }
    }
}


static void Game_Draw__() {
    if (!game.level) {
        HQC_Artist_SetColorHex(0x2C3E50);
        HQC_Artist_FillRect(0, 0, 1280, 720);
        return;
    }

    Level_Draw(game.level, 640, 360);

    _Game_DrawPit();

    // 球链分层：隧道内的球画在顶层贴图之下，标了 t2 的球画在其上
    if (game.graphics && game.graphics->textureTopLayerFile) {
        BallChain_DrawLayer(game.chain, false);
        Level_DrawTopLayer(game.level, 640, 360);
        BallChain_DrawLayer(game.chain, true);
    } else {
        BallChain_Draw(game.chain);
    }

    if (game.frog)      Frog_Draw(game.frog);
    if (game.bulletList) BulletList_Draw(game.bulletList);
    if (game.frog)      Frog_DrawTop(game.frog);

    _Game_DrawTreasure();
    FloatingTextFactory_Draw();

    _Game_DrawIntro();

    _Game_DrawHUD();

    if (game.levelComplete) {
        HQC_Artist_SetColorHex(C_YELLOW);
        HQC_Artist_DrawText(Store_GetFontByID(FONT_NATIVE_ALIEN_48), "LEVEL COMPLETE!", 640, 300);
    }

    if (game.gameOver) {
        HQC_Artist_SetColorHex(C_RED);
        HQC_Artist_DrawText(Store_GetFontByID(FONT_NATIVE_ALIEN_48), "GAME OVER", 640, 300);
    }

    // 结算对话框（过关/Game Over）
    if (game.resultDialog)
        Dialogbox_Draw(game.resultDialog);

    if (game.texUI_LevelName)
        HQC_Artist_DrawTexture(game.texUI_LevelName, 640, 60);

    _Game_DrawOverlay();

    HQC_Artist_SetColorHex(C_WHITE);
}


static void Game_Free_() {
    _Game_ClearPauseButtons();
    _Game_ClearResultDialog();
    _Game_UnloadObjects();

    if (game.texUI_LevelName) { HQC_Artist_FreeTexture(game.texUI_LevelName); game.texUI_LevelName = NULL; }
}


HScene Scene_Register_Game() {
    return Scene_New("game", _Game_Start_, Game_Update__, Game_Draw__, Game_Free_);
}