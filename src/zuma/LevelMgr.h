#pragma once

#include "../global/HQC.h"
#include "Level.h"

void LevelMgr_Init();
bool LevelMgr_LoadLevels(const char* xmlPath);
bool LevelMgr_AdvanceLevel();
void LevelMgr_Reset();

LevelSettings* LevelMgr_GetCurrentSettings();
LevelGraphics* LevelMgr_GetCurrentGraphics();

int LevelMgr_GetCurrentStage();
int LevelMgr_GetCurrentLevelIndex();
const char* LevelMgr_GetCurrentLevelID();
