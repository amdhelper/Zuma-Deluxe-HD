#pragma once

#include "Bullets.h"
#include "BallColors.h"

typedef void* HFrog;

// 青蛙（射手）：鼠标瞄准、左键发射、右键交换当前/下一颗球。
// 2026-09-29 扩展：颜色数由关卡设置决定（原来是硬编码 4 色），
// 并提供"链里没有的颜色强制换掉"接口（原版行为：只发场上存在的颜色）。

HFrog Frog_Create(float x, float y, HBulletList bulletList);
void  Frog_Configure(HFrog hfrog, int colorCount);

void Frog_Update(HFrog hfrog);
void Frog_Draw(HFrog hfrog);
void Frog_DrawTop(HFrog hfrog);

void Frog_Destroy(HFrog hfrog);

// ── 状态（HUD / 自动测试）──────────────────────────────────────────────────
BallColor Frog_GetBallColor(HFrog hfrog);
BallColor Frog_GetNextBallColor(HFrog hfrog);
void      Frog_SwapBalls(HFrog hfrog);
void      Frog_SetColors(HFrog hfrog, BallColor current, BallColor next);
v2f_t     Frog_GetPos(HFrog hfrog);
v2f_t     Frog_GetBallPos(HFrog hfrog);
float     Frog_GetAngle(HFrog hfrog);
bool      Frog_IsFiring(HFrog hfrog);