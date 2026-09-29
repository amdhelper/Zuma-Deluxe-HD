#ifndef ZUMAHD_STATISTICS_H
#define ZUMAHD_STATISTICS_H

#include <stdint.h>
#include "BallColors.h"

// 游戏统计与结算（分数 / 命 / 宝石 / 连击）——2026-09-29 扩展：
// 旧实现只算单次爆炸的飘字分数、从不累计，也没有命数与宝石计数，
// 导致 HUD/胜负结算无从取值。现在这里既是记分板也是 HUD 的数据源。

void Statistics_Init(void);         // 全部清零（新游戏）
void Statistics_ResetLevel(void);   // 每关重开：分数/连击/本关统计清零，命数与累计宝石保留

// ── 命 ─────────────────────────────────────────────────────────────────────
int  Statistics_Lives(void);
void Statistics_SetLives(int lives);
int  Statistics_LoseLife(void);     // 返回剩余命数
void Statistics_AddLives(int lives);

// ── 分数 ───────────────────────────────────────────────────────────────────
int  Statistics_Score(void);
void Statistics_AddScore(int points);

// ── 累计统计 ───────────────────────────────────────────────────────────────
int  Statistics_Coins(void);        // 累计宝石数
void Statistics_AddCoin(void);
int  Statistics_BallsDestroyed(void);
int  Statistics_MaxCombo(void);
int  Statistics_TotalCombos(void);
int  Statistics_ChainBonus(void);       // 当前未中断的链式连击数
int  Statistics_MaxChainBonus(void);
int  Statistics_LevelsCompleted(void);
void Statistics_AddLevelCompleted(void);

// ── 爆炸记账（BallChain 在炸球时调用；返回本次得分）────────────────────────
int  Statistics_RegisterExplosion(int ballsCount, BallColor color,
                                  int comboLevel, int isChainReaction);
void Statistics_BreakChain(void);

// 飘字（爆炸后调用，文本已由上一步准备好）
void Statistics_BuildAndInstantiateFloatingText(float x, float y, uint32_t color);

void Statistics_AddBulletGap(float distance);
int  Statistics_GapCount(void);      // 本次连击窗口内穿缝次数（GAP BONUS 结算/取证用）

#endif //ZUMAHD_STATISTICS_H