#pragma once

// 全局运行选项（命令行解析；自动测试/无头所需）
// 2026-09-29 新增：让游戏可以在无显示器/无音频的环境下脚本化跑完整局，
// 用于回归验证（见 docs/ROADMAP.md 阶段 0）。

typedef struct GameOptions {
    int autotest;        // 1 = 脚本输入自动游玩（无头）
    int maxFrames;       // 自动测试帧上限（默认 30000）
    int startStage;      // 起始大关（1-based）
    int startLevel;      // 起始小关（1-based）
    int difficulty;      // 0..3
    int lives;           // 初始命数
    int levelLimit;      // 自动测试最多游玩几个小关（0 = 不限制）
    int noAudio;         // 1 = 关闭音频
    int noFrameLimit;    // 1 = 不锁 60fps
    int autoplay;        // 1 = 自动测试时自动瞄准开火（默认 1；0 = 只看不动，用于测输局）
    int startAtMenu;     // 1 = 自动测试也从主菜单开始（用于验证菜单链路）
    int screenshotFrame; // >0 = 在第 N 帧存一张截图（自动测试取证）
    const char* screenshotPath;
    const char* screenshotResultPath;  // 非空 = 结算对话框弹出的那一帧自动存图（取证用）
    const char* screenshotNextPath;    // 内部：下一帧渲染完后存这张图（由游戏逻辑触发）
    unsigned int seed;   // 随机种子

    // ── Gauntlet 模式（ROADMAP 3.6）────────────────────────────────────────
    // 无限球流 + 每周目提速：主菜单 Gauntlet 按钮 → 4 个难度（兔/鹰/豹/太阳神）
    int gauntlet;            // 1 = 当前是 Gauntlet 局
    int gauntletDifficulty;  // 0..3
    int gauntletWave;        // 当前目数（从 1 开始）
    int gauntletGauge;       // 本目需要打到的分数
    float gauntletSpeedMul;  // 球速倍率（每周目递增）

    // 限时挑战（ROADMAP 3.13）
    int timed;               // 1 = 限时模式：超过关卡 partTime 就结束本局
    int timedLimit;          // >0 = 覆盖关卡 partTime（自定义限时秒数，测试/自定义用）
    int clearBoard;          // 1 = 启动时清空 Gauntlet 排行榜（--clear-board）
} GameOptions;

extern GameOptions gGameOptions;

void GameOptions_SetDefaults(void);
void GameOptions_Parse(int argc, char** argv);
void GameOptions_PrintUsage(const char* program);
void GameOptions_LogSummary(void);

// 请求"下一帧渲染完成后"存一张截图（游戏逻辑触发取证，如结算对话框弹出）
// 路径取自 gGameOptions.screenshotResultPath
void GameOptions_RequestResultScreenshot(void);

// Gauntlet 局：重置目数/球速倍率/本目目标分（主菜单进入时调用）
void GameOptions_StartGauntlet(int difficulty);

// 难度覆盖（移植自 v0.1.0 Game_Init：难度 0..3 → 颜色数 / 通关分数槽 / 开局球数 / 限时）
// 只在关卡 settings 的副本上应用，不改 LevelMgr 注册表本体。
struct LevelSettings;   // 前向声明（完整定义在 Level.h）
void GameDifficulty_Apply(struct LevelSettings* settings, int difficulty);
const char* GameDifficulty_Name(int difficulty);