#pragma once

#include "../global/HQC.h"

typedef struct LevelGraphics {
    const char*         id;

    const char*         curveAFile;
    const char*         curveBFile;

    const char*         textureFile;
    const char*         textureTopLayerFile;

    const char*         dispName;

    v2f_t               frogPos;

    HQC_VECTOR(v2f_t)   coinsPosList;   // 宝石点（levels.xml 的 <TreasurePoint>）
} LevelGraphics;


typedef struct LevelSettings {
    const char*         id;

    float               ballSpd;
    int                 ballStartCount;
    int                 gaugeScore;
    int                 repeatChance;
    int                 singleChance;
    int                 ballColors;
    int                 partTime;
    
    float               slowFactor;
} LevelSettings;


typedef void* HLevel;

HLevel Level_Load(LevelSettings* settings, LevelGraphics* graphics);
void Level_Free(HLevel hlevel);
const char* Level_GetDisplayName(HLevel hlevel);

v2f_t Level_GetCurveCoords(HLevel hlevel, float pos);
int Level_GetCurveLength(HLevel hlevel);
void Level_Draw(HLevel hlevel, float x, float y);

// ── 新增（2026-09-29，完成玩法闭环所需）────────────────────────────────────
LevelSettings* Level_GetSettings(HLevel hlevel);
LevelGraphics* Level_GetGraphics(HLevel hlevel);

v2f_t Level_GetCurveDirection(HLevel hlevel, float pos);            // 曲线单位方向（球朝向）
void  Level_GetCurveFlags(HLevel hlevel, float pos, int* isTunnel, int* isTopPriority);
v2f_t Level_GetPitPos(HLevel hlevel);                              // 曲线尽头的洞（骷髅）位置
int   Level_GetTreasureCount(HLevel hlevel);
v2f_t Level_GetTreasurePos(HLevel hlevel, int index);
void  Level_DrawTopLayer(HLevel hlevel, float x, float y);         // 隧道/桥等顶层贴图