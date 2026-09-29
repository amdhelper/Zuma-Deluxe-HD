#include <math.h>
#include <stdlib.h>
#include "BallChain.h"

#include "ResourceStore.h"
#include "Statistics.h"
#include "AutoTest.h"

// ═══════════════════════════════════════════════════════════════════════════════
// 球链（链表方向）：
//   start = 最后排（最新生成 = 驱动球，pos 最小，出生点附近）
//   end   = 最前排（最先入洞，pos 最大 → 到曲线末端就进洞）
//   每个球的 prev 指向"更靠后"、next 指向"更靠前"。
// 推进方式与原版一致：链尾驱动球加速到链速，其余球贴住后球跟随；
// 出现缝隙时同色球磁吸回滚合拢（≥3 同色相连即爆），异色球摩擦停住。
// 2026-09-29 重写：旧实现里最前球越过曲线末端会把 pos 减一圈跑回起点
// （导致链永远在循环、关卡永远不可能结束），且生成器无上限（球无限生成）。
// ═══════════════════════════════════════════════════════════════════════════════

typedef struct Ball {
    struct BallChain*  chain;

    float       pos;
    float       spd;

    BallColor   color;

    bool        isExploding;
    bool        isNew;          // 出生淡入
    bool        isSeparated;    // 与前球脱开过（用于合拢判定）
    bool        justClosed;     // 本帧刚合拢（触发连环爆炸判定）
    bool        inTunnel;       // 曲线 t1：隧道内（画在顶层之下）
    bool        isTopPriority;  // 曲线 t2：画在顶层之上

    HQC_Animation animation;

    struct Ball* prev;   // 更靠后
    struct Ball* next;   // 更靠前
} Ball;


typedef struct BallChain {
    HLevel level;

    Ball* start;
    Ball* end;

    int   len;

    float speed;         // 链速（settings->ballSpd）
    int   ballColors;
    int   singleChance;
    int   repeatChance;

    bool  isGenerating;
    bool  isEndReached;

    int   explodedThisFrame;

    HBulletList bulletList;
} BallChain;


static BallChain* Cast__(HBallChain hchain) {
    return (BallChain*)hchain;
}

static Ball* _Ball(HBall hball) {
    return (Ball*)hball;
}


static uint32_t _Ball_GetColorUint32(BallColor color) {
    switch (color) {
        case BALL_BLUE:   return 0x00FEFD;
        case BALL_GREEN:  return 0x00FA44;
        case BALL_YELLOW: return 0xFDFD06;
        case BALL_RED:    return 0xF96F4C;
        case BALL_PURPLE: return 0xFD88F8;
        case BALL_GRAY:   return 0xFDEABB;
        default:          return 0xFFFFFF;
    }
}

static HQC_Animation _Ball_MakeAnim(BallColor color) {
    return HQC_Animation_Clone(Store_GetAnimationByID(ANIM_BALL_BLUE + (int)color));
}


////////////////////////////////////////////////////////////////////////////////
// 球：创建 / 摘除 / 释放
////////////////////////////////////////////////////////////////////////////////

static Ball* Ball_Create__(BallChain* chain, BallColor color, float pos) {
    Ball* ball = HQC_Memory_Allocate(sizeof(*ball));

    ball->chain      = chain;
    ball->pos        = pos;
    ball->spd        = 0.0f;
    ball->color      = color;
    ball->isExploding   = false;
    ball->isNew         = true;
    ball->isSeparated   = false;
    ball->justClosed    = false;
    ball->inTunnel      = false;
    ball->isTopPriority = false;
    ball->prev       = NULL;
    ball->next       = NULL;
    ball->animation  = _Ball_MakeAnim(color);

    return ball;
}


void BallChain_RemoveBall(HBallChain hchain, HBall hball) {
    BallChain* chain = Cast__(hchain);
    Ball* ball = _Ball(hball);

    if (!chain || !ball) return;
    if (BallChain_HasBall(hchain, hball) == NULL) return;

    if (ball->prev != NULL) ball->prev->next = ball->next;
    else                    chain->start = ball->next;

    if (ball->next != NULL) ball->next->prev = ball->prev;
    else                    chain->end = ball->prev;

    // 被摘掉的球后面那颗球，与前球之间出现了缝隙 → 允许磁吸合拢
    if (ball->prev != NULL) ball->prev->isSeparated = true;

    if (ball->animation) HQC_Animation_Free(ball->animation);

    HQC_Memory_Free(ball);
    chain->len--;
}


////////////////////////////////////////////////////////////////////////////////
// 球：查询
////////////////////////////////////////////////////////////////////////////////

float Ball_Speed(HBall hball) {
    Ball* ball = _Ball(hball);
    return ball ? ball->spd : 0.0f;
}

void Ball_SetSpeed(HBall hball, float spd) {
    Ball* ball = _Ball(hball);
    if (ball) ball->spd = spd;
}

float Ball_GetPositionOnCurve(HBall hball) {
    Ball* ball = _Ball(hball);
    return ball ? ball->pos : 0.0f;
}

v2f_t Ball_GetPositionCoords(HBall hball) {
    Ball* ball = _Ball(hball);
    if (!ball || !ball->chain) return v2f_t_default;

    return Level_GetCurveCoords(ball->chain->level, (ball->pos >= 0) ? ball->pos : 0);
}

HBall Ball_Next(HBall hball) {
    Ball* ball = _Ball(hball);
    return ball ? ball->next : NULL;
}

HBall Ball_Previous(HBall hball) {
    Ball* ball = _Ball(hball);
    return ball ? ball->prev : NULL;
}

BallColor Ball_GetColor(HBall hball) {
    Ball* ball = _Ball(hball);
    return ball ? ball->color : BALL_NONE;
}

bool Ball_IsExploding(HBall hball) {
    Ball* ball = _Ball(hball);
    return ball ? ball->isExploding : false;
}

bool Ball_IsInTunnel(HBall hball) {
    Ball* ball = _Ball(hball);
    return ball ? ball->inTunnel : false;
}

float Ball_GetDistanceBetweenBalls(HBall a, HBall b) {
    Ball* ba = _Ball(a);
    Ball* bb = _Ball(b);

    if (!ba || !bb) return 0.0f;

    return bb->pos - ba->pos;
}

HBallChain Ball_GetChain(HBall hball) {
    Ball* ball = _Ball(hball);
    return ball ? ball->chain : NULL;
}

void Ball_MoveSubChainFrom(HBall hball, float value) {
    Ball* ball = _Ball(hball);
    if (!ball) return;

    ball->pos += value;

    for (Ball* b = ball->next; b != NULL; b = b->next) {
        if (b->pos - b->prev->pos > BALLS_CHAIN_PAD_ROUGH)
            return;

        b->pos += value;
    }
}

void Ball_BulletInsertDone(HBall hball) {
    Ball* ball = _Ball(hball);
    if (!ball) return;

    // 插入完成后这颗球就是"新合拢点"，交由合拢逻辑判定三连
    ball->justClosed = true;
}


HBallChain BallChain_HasBall(HBallChain hchain, HBall hball) {
    BallChain* chain = Cast__(hchain);
    if (!chain || !hball) return NULL;

    for (Ball* ball = chain->start; ball != NULL; ball = ball->next)
        if (ball == (Ball*)hball) return ball;

    return NULL;
}


////////////////////////////////////////////////////////////////////////////////
// 链：创建 / 配置 / 插入 / 生成
////////////////////////////////////////////////////////////////////////////////

HBallChain BallChain_Create(HLevel level, HBulletList bulletList) {
    BallChain* chain = HQC_Memory_Allocate(sizeof(*chain));

    chain->level         = level;
    chain->start         = NULL;
    chain->end           = NULL;
    chain->len           = 0;
    chain->speed         = 1.0f;
    chain->ballColors    = 4;
    chain->singleChance  = 0;
    chain->repeatChance  = 50;
    chain->isGenerating  = true;
    chain->isEndReached  = false;
    chain->explodedThisFrame = 0;
    chain->bulletList    = bulletList;

    return chain;
}


void BallChain_Configure(HBallChain hchain, LevelSettings* settings) {
    BallChain* chain = Cast__(hchain);
    if (!chain || !settings) return;

    chain->speed = settings->ballSpd > 0 ? settings->ballSpd : 0.6f;

    chain->ballColors = settings->ballColors;
    if (chain->ballColors < 1) chain->ballColors = 1;
    if (chain->ballColors > 6) chain->ballColors = 6;

    chain->singleChance = settings->singleChance;
    chain->repeatChance = settings->repeatChance;
}


static BallColor _Chain_PickColor(BallChain* chain) {
    int maxColor = chain->ballColors;
    Ball* back = chain->start;
    BallColor last = back ? back->color : BALL_NONE;
    int r = rand() % 100;

    if (back && maxColor > 1 && r < chain->singleChance) {
        // 单色隔断：挑一个与最后球不同的颜色，打断同色串
        BallColor c = last;
        int guard = 0;
        while (c == last && guard++ < 32)
            c = (BallColor)(rand() % maxColor);
        return c;
    }

    if (back && r < chain->singleChance + chain->repeatChance)
        return last;   // 重复上一颗 → 形成同色串（原版 repeat 手感）

    return (BallColor)(rand() % maxColor);
}


HBall BallChain_AppendBall(HBallChain hchain, BallColor color) {
    BallChain* chain = Cast__(hchain);
    if (!chain) return NULL;
    if (chain->len >= BALLCHAIN_MAX_LEN) return NULL;

    Ball* ball = Ball_Create__(chain, color, 0.0f);

    ball->prev = NULL;
    ball->next = chain->start;

    if (chain->start != NULL) chain->start->prev = ball;
    else                      chain->end = ball;

    chain->start = ball;
    chain->len++;

    return ball;
}


HBall BallChain_AddToStart(HBallChain hchain, BallColor color) {
    return BallChain_AppendBall(hchain, color);
}


HBall BallChain_InsertBeforeBall(BallColor color, HBall nextBall, float pos) {
    Ball* ball = _Ball(nextBall);
    if (!ball) return NULL;

    Ball* newBall = Ball_Create__(ball->chain, color, pos);

    newBall->prev = ball->prev;
    newBall->next = ball;

    if (ball->prev != NULL) ball->prev->next = newBall;
    else                    ball->chain->start = newBall;

    ball->prev = newBall;
    ball->chain->len++;

    newBall->isSeparated = (newBall->prev != NULL)
        ? (newBall->pos - newBall->prev->pos > BALLS_CHAIN_PAD_ROUGH) : false;

    return newBall;
}


HBall BallChain_InsertAfterBall(BallColor color, HBall prevBall, float pos) {
    Ball* ball = _Ball(prevBall);
    if (!ball) return NULL;

    Ball* newBall = Ball_Create__(ball->chain, color, pos);

    newBall->prev = ball;
    newBall->next = ball->next;

    if (ball->next != NULL) ball->next->prev = newBall;
    else                    ball->chain->end = newBall;

    ball->next = newBall;
    ball->chain->len++;

    return newBall;
}


////////////////////////////////////////////////////////////////////////////////
// 爆炸
////////////////////////////////////////////////////////////////////////////////

// 找到包含 pivot 的同色连续组（必须真正相邻：沿链方向且间距在接触范围内）
static void _FindSameColorGroup(Ball* pivot, Ball** startOut, Ball** endOut, int* countOut) {
    Ball* s = pivot;
    Ball* e = pivot;
    int count = 1;

    for (Ball* b = pivot->next; b != NULL; b = b->next) {
        if (b->isExploding) break;
        if (b->color != pivot->color) break;
        if (b->pos - b->prev->pos > BALLS_CHAIN_PAD_ROUGH) break;

        e = b;
        count++;
    }

    for (Ball* b = pivot->prev; b != NULL; b = b->prev) {
        if (b->isExploding) break;
        if (b->color != pivot->color) break;
        if (b->next->pos - b->pos > BALLS_CHAIN_PAD_ROUGH) break;

        s = b;
        count++;
    }

    *startOut = s;
    *endOut   = e;
    *countOut = count;
}


static int _ExplodeGroup(BallChain* chain, Ball* s, Ball* e, int isChainReaction) {
    int count = 0;
    for (Ball* b = s; b != NULL; b = b->next) { count++; if (b == e) break; }

    Ball* mid = s;
    for (int i = 0; i < count / 2 && mid->next != NULL; i++) mid = mid->next;

    v2f_t pos = Ball_GetPositionCoords(mid);

    int combo = Statistics_ChainBonus();
    int points = Statistics_RegisterExplosion(count, s->color, combo, isChainReaction);
    Statistics_BuildAndInstantiateFloatingText(pos.x, pos.y, _Ball_GetColorUint32(s->color));

    for (Ball* b = s; b != NULL; b = b->next) {
        b->isExploding = true;
        b->spd         = 0.0f;

        if (b->animation) HQC_Animation_Free(b->animation);
        b->animation = HQC_Animation_Clone(Store_GetAnimationByID(ANIM_BALL_DESTROY));
        HQC_Animation_SetFrame(b->animation, 0);
        HQC_Animation_SetLooping(b->animation, false);
        HQC_Animation_SetSpeed(b->animation, BALL_EXPLODE_SPEED);

        if (b == e) break;
    }

    // 连击越高音调越高（原版手感）
    HQC_DJ_PlaySoundPitch(Store_GetSoundByID(SND_BALLSDESTROYED1), (float)(combo * 2));

    chain->explodedThisFrame = 1;

    AutoTest_Event("EXPLOSION", "balls=%d chain_reaction=%d combo=%d points=%d",
                   count, isChainReaction, combo, points);

    return points;
}


HBall BallChain_ExplodeBalls(HBall hstartBall, int isChainReaction) {
    Ball* ball = _Ball(hstartBall);
    if (!ball || !ball->chain) return hstartBall;

    Ball *s = ball, *e = ball;
    int count = 1;

    _FindSameColorGroup(ball, &s, &e, &count);

    if (count < BALLS_TO_EXPLODE)
        return ball;

    _ExplodeGroup(ball->chain, s, e, isChainReaction);

    return ball;
}


////////////////////////////////////////////////////////////////////////////////
// 链：物理推进
////////////////////////////////////////////////////////////////////////////////

void BallChain_Update(HBallChain hchain) {
    BallChain* chain = Cast__(hchain);
    if (!chain) return;

    chain->explodedThisFrame = 0;

    int curveLen = Level_GetCurveLength(chain->level);
    if (curveLen <= 0) return;

    // ── 1) 速度与位置（从驱动球一侧向最前排）──────────────────────────────
    for (Ball* ball = chain->start; ball != NULL; ball = ball->next) {
        if (ball->isExploding) {
            ball->spd = 0.0f;
            continue;
        }

        if (ball->prev == NULL) {
            // 驱动球（最后排）：加速到链速
            if (chain->isEndReached) {
                ball->spd = ROLLING_TO_PIT_SPEED;      // 已进洞：整链加速坠洞
            } else {
                ball->spd += BALL_ACC;
                if (ball->spd > chain->speed) ball->spd = chain->speed;
            }
        } else {
            float gap = ball->pos - ball->prev->pos;

            if (gap <= BALLS_CHAIN_PAD_ROUGH) {
                // 贴着后球 → 跟随
                if (ball->isSeparated) {
                    ball->isSeparated = false;
                    ball->justClosed  = true;
                }
                ball->spd = ball->prev->spd;
            } else {
                ball->isSeparated = true;

                if (ball->color == ball->prev->color && !chain->isEndReached) {
                    // 同色磁吸：回滚靠近后球 → 合拢后触发连环爆炸
                    ball->spd -= BALL_DECC;
                    if (ball->spd < BALL_MAX_BACK_SPEED) ball->spd = BALL_MAX_BACK_SPEED;
                } else {
                    // 异色：摩擦停住
                    if (ball->spd > 0) {
                        ball->spd -= BALL_FRC;
                        if (ball->spd < 0) ball->spd = 0;
                    } else if (ball->spd < 0) {
                        ball->spd += BALL_FRC;
                        if (ball->spd > 0) ball->spd = 0;
                    }
                }
            }
        }

        ball->pos += ball->spd;
    }

    // ── 1.5) 分离相位：保证相邻球至少一个身位 ───────────────────────────────
    // 生成初期球是密集堆在后排的（原版：出生点固定 pos 0，链尾推进一个身位就补一颗），
    // 靠这个"往前推"把整链撑开 —— 也就是原版的 fastMode 效果。
    // 少了这一步，球会挤在一起、间隙判据全部失效（链断裂、卡死不动）。
    for (Ball* ball = (chain->start ? chain->start->next : NULL); ball != NULL; ball = ball->next) {
        if (ball->isExploding || ball->prev->isExploding) continue;

        if (ball->pos - ball->prev->pos < BALLS_CHAIN_PAD)
            ball->pos = ball->prev->pos + BALLS_CHAIN_PAD;
    }

    // ── 2) 进洞：最前球越过曲线末端 ────────────────────────────────────────
    while (chain->end != NULL && chain->end->pos >= (float)curveLen) {
        if (!chain->isEndReached) {
            chain->isEndReached = true;
            chain->isGenerating = false;
            chain->speed        = ROLLING_TO_PIT_SPEED;   // 余下的球加速坠洞

            HQC_DJ_PlaySound(Store_GetSoundByID(SND_WARNING1));
            AutoTest_Event("BALL_INTO_PIT", "len=%d curve=%d", chain->len, curveLen);
        }

        BallChain_RemoveBall(hchain, chain->end);
    }

    // 出生点回退保护（驱动球不该跑到负位置）
    if (chain->start != NULL && chain->start->pos < 0.0f)
        chain->start->pos = 0.0f;

    // ── 3) 合拢 → 连环爆炸 ────────────────────────────────────────────────
    if (!chain->isEndReached) {
        for (Ball* ball = chain->start; ball != NULL; ball = ball->next) {
            if (!ball->justClosed) continue;
            ball->justClosed = false;

            if (ball->isExploding || ball->prev == NULL) continue;
            if (ball->color != ball->prev->color) continue;

            BallChain_ExplodeBalls(ball, 1);
        }
    }

    // ── 4) 曲线标记（隧道 / 顶层优先级）+ 出生淡入 ─────────────────────────
    for (Ball* ball = chain->start; ball != NULL; ball = ball->next) {
        int isTunnel = 0, isTop = 0;
        Level_GetCurveFlags(chain->level, ball->pos, &isTunnel, &isTop);
        ball->inTunnel      = isTunnel;
        ball->isTopPriority = isTop;

        if (ball->isNew && ball->pos >= BALLS_CHAIN_PAD)
            ball->isNew = false;
    }

    // ── 5) 爆炸动画推进 + 动画结束移除 ─────────────────────────────────────
    for (Ball* ball = chain->start; ball != NULL; ) {
        Ball* next = ball->next;

        if (ball->isExploding) {
            HQC_Animation_Tick(ball->animation);

            if (HQC_Animation_IsEnded(ball->animation))
                BallChain_RemoveBall(hchain, ball);
        }

        ball = next;
    }
}


bool BallChain_IsEmpty(HBallChain hchain) {
    BallChain* chain = Cast__(hchain);
    return chain ? chain->start == NULL : true;
}


HLevel BallChain_GetLevel(HBallChain hchain) {
    BallChain* chain = Cast__(hchain);
    return chain ? chain->level : NULL;
}


////////////////////////////////////////////////////////////////////////////////
// 链：绘制
////////////////////////////////////////////////////////////////////////////////

static void _Ball_Draw(Ball* ball) {
    if (!ball) return;
    if (ball->pos < 0.0f) return;

    v2f_t pos = Level_GetCurveCoords(ball->chain->level, ball->pos);

    if (ball->isExploding) {
        HQC_Artist_SetDrawColorMod(_Ball_GetColorUint32(ball->color));
        HQC_Artist_DrawAnimation(ball->animation, pos.x, pos.y);
        HQC_Artist_SetDrawColorMod(C_WHITE);
        return;
    }

    v2f_t dir = Level_GetCurveDirection(ball->chain->level, ball->pos);
    float angle = HQC_FAtan2(dir.y, dir.x) + M_PI_2;

    // 出生淡入（原版 startAnim 的手感）
    float alpha = 1.0f;
    if (ball->isNew) {
        alpha = HQC_FMin(ball->pos / BALLS_CHAIN_PAD, 1.0f);
        if (alpha < 0.05f) alpha = 0.05f;
    }

    size_t frames = HQC_Animation_FramesCount(ball->animation);
    if (frames > 0)
        HQC_Animation_SetFrame(ball->animation, ((int)ball->pos) % (int)frames);

    HQC_Artist_DrawSetAngle(angle);
    HQC_Artist_DrawSetAlpha(alpha);
    HQC_Artist_DrawAnimation(ball->animation, pos.x, pos.y);
    HQC_Artist_DrawSetAlpha(1.0f);
    HQC_Artist_DrawSetAngle(0.0f);
}


void BallChain_DrawLayer(HBallChain hchain, bool topLayer) {
    BallChain* chain = Cast__(hchain);
    if (!chain) return;

    for (Ball* ball = chain->start; ball != NULL; ball = ball->next) {
        if (ball->isTopPriority != topLayer) continue;
        _Ball_Draw(ball);
    }
}


void BallChain_Draw(HBallChain hchain) {
    BallChain* chain = Cast__(hchain);
    if (!chain) return;

    for (Ball* ball = chain->start; ball != NULL; ball = ball->next)
        _Ball_Draw(ball);
}


void BallChain_Destroy(HBallChain hchain) {
    BallChain* chain = Cast__(hchain);
    if (!chain) return;

    while (chain->start != NULL)
        BallChain_RemoveBall(hchain, chain->start);

    HQC_Memory_Free(chain);
}


////////////////////////////////////////////////////////////////////////////////
// 状态查询
////////////////////////////////////////////////////////////////////////////////

int BallChain_Length(HBallChain hchain) {
    BallChain* chain = Cast__(hchain);
    return chain ? chain->len : 0;
}

int BallChain_ExplodingCount(HBallChain hchain) {
    BallChain* chain = Cast__(hchain);
    if (!chain) return 0;

    int n = 0;
    for (Ball* ball = chain->start; ball != NULL; ball = ball->next)
        if (ball->isExploding) n++;

    return n;
}

float BallChain_GetSpeed(HBallChain hchain) {
    BallChain* chain = Cast__(hchain);
    return chain ? chain->speed : 0.0f;
}

void BallChain_SetSpeed(HBallChain hchain, float speed) {
    BallChain* chain = Cast__(hchain);
    if (chain) chain->speed = speed;
}

float BallChain_FrontProgress(HBallChain hchain) {
    BallChain* chain = Cast__(hchain);
    if (!chain || !chain->end) return 0.0f;

    int curveLen = Level_GetCurveLength(chain->level);
    if (curveLen <= 0) return 0.0f;

    float p = chain->end->pos / (float)curveLen;
    if (p < 0.0f) p = 0.0f;
    if (p > 1.0f) p = 1.0f;

    return p;
}

bool BallChain_IsEndReached(HBallChain hchain) {
    BallChain* chain = Cast__(hchain);
    return chain ? chain->isEndReached : false;
}

bool BallChain_IsGenerating(HBallChain hchain) {
    BallChain* chain = Cast__(hchain);
    return chain ? chain->isGenerating : false;
}

void BallChain_SetGenerating(HBallChain hchain, bool generating) {
    BallChain* chain = Cast__(hchain);
    if (chain) chain->isGenerating = generating;
}

int BallChain_GetBallColors(HBallChain hchain) {
    BallChain* chain = Cast__(hchain);
    return chain ? chain->ballColors : 4;
}

bool BallChain_ColorIsInChain(HBallChain hchain, BallColor color) {
    BallChain* chain = Cast__(hchain);
    if (!chain) return false;

    for (Ball* ball = chain->start; ball != NULL; ball = ball->next)
        if (!ball->isExploding && ball->color == color)
            return true;

    return false;
}

HBall BallChain_GetBallAt(HBallChain hchain, int index) {
    BallChain* chain = Cast__(hchain);
    if (!chain || index < 0 || index >= chain->len) return NULL;

    Ball* ball = chain->start;
    for (int i = 0; i < index && ball != NULL; i++)
        ball = ball->next;

    return ball;
}

HBall BallChain_GetFrontBall(HBallChain hchain) {
    BallChain* chain = Cast__(hchain);
    return chain ? chain->end : NULL;
}

HBall BallChain_GetBackBall(HBallChain hchain) {
    BallChain* chain = Cast__(hchain);
    return chain ? chain->start : NULL;
}


////////////////////////////////////////////////////////////////////////////////
// 生成器
////////////////////////////////////////////////////////////////////////////////

typedef struct Generator {
    BallChain* chain;

    int  initialCount;    // settings->ballStartCount：开局铺满的球数
    int  generated;       // 已生成总数
    bool fastMode;        // 首屏快速铺满
    bool finished;
} Generator;


HBallChainGenerator BallChainGenerator_Create(HBallChain hballChain) {
    Generator* gen = HQC_Memory_Allocate(sizeof(*gen));

    gen->chain        = Cast__(hballChain);
    gen->initialCount = 35;
    gen->generated    = 0;
    gen->fastMode     = true;
    gen->finished     = false;

    return gen;
}


void BallChainGenerator_Destroy(HBallChainGenerator hballChainGenerator) {
    if (hballChainGenerator)
        HQC_Memory_Free(hballChainGenerator);
}


void BallChainGenerator_SetInitialCount(HBallChainGenerator hballChainGenerator, int count) {
    Generator* gen = (Generator*)hballChainGenerator;
    if (!gen) return;

    if (count < 1) count = 1;
    if (count > BALLCHAIN_MAX_LEN) count = BALLCHAIN_MAX_LEN;

    gen->initialCount = count;
}


void BallChainGenerator_GenerateSequence(HBallChainGenerator hballChainGenerator, size_t count) {
    // 原版保留接口：一次性预生成 count 颗（开局铺满）
    Generator* gen = (Generator*)hballChainGenerator;
    if (!gen || !gen->chain) return;

    for (size_t i = 0; i < count; i++) {
        if (BallChain_AppendBall((HBallChain)gen->chain, _Chain_PickColor(gen->chain)) == NULL)
            break;
        gen->generated++;
    }
}


void BallChainGenerator_Stop(HBallChainGenerator hballChainGenerator) {
    Generator* gen = (Generator*)hballChainGenerator;
    if (!gen) return;

    gen->finished = true;
    gen->fastMode = false;
}


bool BallChainGenerator_IsFinished(HBallChainGenerator hballChainGenerator) {
    Generator* gen = (Generator*)hballChainGenerator;
    return gen ? gen->finished : true;
}


int BallChainGenerator_GeneratedCount(HBallChainGenerator hballChainGenerator) {
    Generator* gen = (Generator*)hballChainGenerator;
    return gen ? gen->generated : 0;
}


void BallChainGenerator_Update(HBallChainGenerator hballChainGenerator) {
    Generator* gen = (Generator*)hballChainGenerator;
    if (!gen || !gen->chain) return;

    BallChain* chain = gen->chain;

    if (gen->finished || chain->isEndReached || !chain->isGenerating)
        return;

    if (chain->len >= BALLCHAIN_MAX_LEN) return;

    Ball* back = chain->start;

    if (back == NULL) {
        BallChain_AppendBall((HBallChain)chain, _Chain_PickColor(chain));
        gen->generated++;
        return;
    }

    if (gen->fastMode && gen->generated < gen->initialCount) {
        // 首屏快速铺满：链尾推进就补球（间距由链的"分离相位"撑开到 32）
        if (back->pos >= 0.5f) {
            BallChain_AppendBall((HBallChain)chain, _Chain_PickColor(chain));
            gen->generated++;
        }

        if (gen->generated >= gen->initialCount)
            gen->fastMode = false;

        return;
    }

    // 常规节奏：链尾推进一个身位就补一颗
    if (back->pos >= BALLS_CHAIN_PAD) {
        BallChain_AppendBall((HBallChain)chain, _Chain_PickColor(chain));
        gen->generated++;
    }
}