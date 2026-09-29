#pragma once

#include "Level.h"
#include "Bullets.h"
#include "BallColors.h"

// ── 物理常量（移植自 v0.1.0 原版，手感基准）──────────────────────────────
#define BALLS_CHAIN_PAD         32.0f    // 相邻球标准间距（沿曲线）
#define BALLS_CHAIN_PAD_ROUGH   34.0f
#define BALLCHAIN_MAX_LEN       256
#define BALL_ACC                0.05f    // 链尾驱动球加速度
#define BALL_FRC                0.04f    // 摩擦减速
#define BALL_DECC               0.4f     // 同色磁吸回滚加速度
#define BALL_WEIGHT_RATIO       2.5f
#define BALL_MAX_BACK_SPEED     (-10.0f)
#define BALL_EXPLODE_SPEED      0.5f
#define BALL_INSERTION_SPD      4.0f
#define BALLS_TO_EXPLODE        3
#define ROLLING_TO_PIT_SPEED    32.0f    // 球已进洞后整链坠洞速度

typedef void* HBall;

// ── 球 ─────────────────────────────────────────────────────────────────────
float   Ball_Speed(HBall hball);
float   Ball_GetPositionOnCurve(HBall hball);
v2f_t   Ball_GetPositionCoords(HBall ball);
void    Ball_MoveSubChainFrom(HBall hball, float value);
HBall   Ball_Next(HBall hball);
HBall   Ball_Previous(HBall hball);
void    Ball_BulletInsertDone(HBall hball);
float   Ball_GetDistanceBetweenBalls(HBall a, HBall b);

BallColor Ball_GetColor(HBall hball);        // 新
bool      Ball_IsExploding(HBall hball);     // 新
bool      Ball_IsInTunnel(HBall hball);      // 新（隧道内的球不参与命中判定）
void      Ball_SetSpeed(HBall hball, float spd);  // 新

// ── 道具球（ROADMAP 3.1）────────────────────────────────────────────────────
// 素材表里没有道具图标（gameobjects.png 球体条只有纯色球）→ 图标用绘图原语程序化画。
typedef enum BallBonus {
    BONUS_NONE = 0,
    BONUS_EXPLOSION,   // 炸掉附近一圈球（无视颜色）
    BONUS_SLOWDOWN,    // 球链减速一段
    BONUS_PAUSE,       // 球链暂停一段
    BONUS_ACCURACY,    // 精准奖励分
    BONUS_COUNT
} BallBonus;

int         Ball_GetBonus(HBall hball);
void        Ball_SetBonus(HBall hball, int bonus);
const char* BallBonus_Name(int bonus);

// 道具效果生效时的链速倍率（减速 0.35 / 暂停 0），SceneGame 每帧乘进链速
float BallChain_GetSpeedMultiplier(HBallChain hchain);

// 本局用掉的道具数（取证/统计）
int BallChain_PowerupsUsed(void);

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

typedef void* HBallChain;

#define HBALLCHAIN_TYPEDEF 1

HBallChain Ball_GetChain(HBall ball);

void BallChain_Configure(HBallChain hchain, LevelSettings* settings);  // 链速/颜色数/生成概率

HBall BallChain_HasBall(HBallChain hchain, HBall hball);

HBall BallChain_ExplodeBalls(HBall hstartBall, int isChainReaction);

HBallChain BallChain_Create(HLevel level, HBulletList bulletList);
HLevel BallChain_GetLevel(HBallChain hchain);
HBall BallChain_AddToStart(HBallChain hchain, BallColor color);

HBall BallChain_InsertBeforeBall(BallColor color, HBall nextBall, float pos);
HBall BallChain_InsertAfterBall(BallColor color, HBall prevBall, float pos);

// 生成：在链尾（后排）追加一颗球（pos 固定为 0，即出生点）
HBall BallChain_AppendBall(HBallChain hchain, BallColor color);

bool BallChain_IsEmpty(HBallChain hchain);

void BallChain_Update(HBallChain hchain);
void BallChain_Draw(HBallChain hchain);
// 分层绘制：隧道/桥等顶层贴图前后的球分开画（t2 曲线标记）
void BallChain_DrawLayer(HBallChain hchain, bool topLayer);
void BallChain_Destroy(HBallChain hchain);

// ── 状态查询（HUD / 胜负判定 / 自动测试）──────────────────────────────────
int   BallChain_Length(HBallChain hchain);
int   BallChain_ExplodingCount(HBallChain hchain);
float BallChain_GetSpeed(HBallChain hchain);
void  BallChain_SetSpeed(HBallChain hchain, float speed);
float BallChain_FrontProgress(HBallChain hchain);   // 最前球位置 / 曲线长度 (0..1)
bool  BallChain_IsEndReached(HBallChain hchain);
bool  BallChain_IsGenerating(HBallChain hchain);
void  BallChain_SetGenerating(HBallChain hchain, bool generating);
bool  BallChain_ColorIsInChain(HBallChain hchain, BallColor color);
int   BallChain_GetBallColors(HBallChain hchain);

HBall BallChain_GetBallAt(HBallChain hchain, int index);   // 0 = 最后排（最新），len-1 = 最前
HBall BallChain_GetFrontBall(HBallChain hchain);
HBall BallChain_GetBackBall(HBallChain hchain);

void  BallChain_RemoveBall(HBallChain hchain, HBall hball);   // 直接移除（进洞）

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

typedef void* HBallChainGenerator;

HBallChainGenerator BallChainGenerator_Create(HBallChain hballChain);
void BallChainGenerator_Destroy(HBallChainGenerator hballChainGenerator);
void BallChainGenerator_SetInitialCount(HBallChainGenerator hballChainGenerator, int count);
void BallChainGenerator_GenerateSequence(HBallChainGenerator hballChainGenerator, size_t count);

void BallChainGenerator_Stop(HBallChainGenerator hballChainGenerator);
void BallChainGenerator_Update(HBallChainGenerator hballChainGenerator);
bool BallChainGenerator_IsFinished(HBallChainGenerator hballChainGenerator);
int  BallChainGenerator_GeneratedCount(HBallChainGenerator hballChainGenerator);