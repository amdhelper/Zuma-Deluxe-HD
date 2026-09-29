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
    int screenshotFrame; // >0 = 在第 N 帧存一张截图（自动测试取证）
    const char* screenshotPath;
    unsigned int seed;   // 随机种子
} GameOptions;

extern GameOptions gGameOptions;

void GameOptions_SetDefaults(void);
void GameOptions_Parse(int argc, char** argv);
void GameOptions_PrintUsage(const char* program);
void GameOptions_LogSummary(void);