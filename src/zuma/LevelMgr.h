#pragma once

#include "../global/HQC.h"
#include "Level.h"

void LevelMgr_Init();
bool LevelMgr_LoadLevels(const char* xmlPath);
bool LevelMgr_AdvanceLevel();
void LevelMgr_Reset();
void LevelMgr_SetProgress(int stage, int level);     // 0-based

LevelSettings* LevelMgr_GetCurrentSettings();
LevelGraphics* LevelMgr_GetCurrentGraphics();

// ⚠️ 命名陷阱：下面这两个是 **1-based**（HUD / 事件日志 / 玩家可见编号用），
//    要和存档、选关（0-based）混用请用 LevelMgr_CurrentStage0/CurrentLevel0。
int LevelMgr_GetCurrentStage();
int LevelMgr_GetCurrentLevelIndex();
int LevelMgr_CurrentStage0();       // 0-based 当前大关（存档/Progress/LevelMgr_GetLevelXxx 用）
int LevelMgr_CurrentLevel0();       // 0-based 当前小关
const char* LevelMgr_GetCurrentLevelID();
int LevelMgr_GetCurrentStageLevelCount();
int LevelMgr_GetStageCount();
int LevelMgr_GetSettingsCount();

// 选关界面用（0-based）
LevelGraphics* LevelMgr_GetLevelGraphics(int stage, int level);
LevelSettings* LevelMgr_GetLevelSettings(int stage, int level);
int LevelMgr_GetLevelCount(int stage);
int LevelMgr_GetGraphicsIndex(int stage, int level);      // 缩略图索引（0..17 循环）
void LevelMgr_ClampProgress(int* stage, int* level);
