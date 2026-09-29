#include <math.h>
#include "Bullets.h"
#include "BallChain.h"
#include "Statistics.h"
#include "AutoTest.h"

// 子弹（2026-09-29 改为真实抛射）：
// 旧实现每帧把子弹位置直接赋成鼠标坐标（"跟着鼠标的幽灵球"），
// 于是"射出去的球"根本不存在弹道，命中判定也就没有意义。
// 现在：发射时从青蛙口部沿瞄准方向飞出（BULLET_SPEED/帧），
// 命中球链后进入插入动画（把球补进链里），飞出屏幕则作废。

#define ARR_SIZE      8
#define INSERT_TIME   10
#define BULLET_SPEED  15.0f
#define DISTANCE_TO_COLLIDE 2300.0f   // ≈48px（平方距离）

typedef struct Bullet {
	v2f_t		pos;
	v2f_t		dir;
	float		spd;
	float		direction;

	void*		insertionBall;
	bool		isShoudInsertBallToFront;

	void*		insertionBallChain;

	int			insertTimer;

	// 飞出屏幕后的"漏球"结算（GAP BONUS，ROADMAP 3.3）：
	// 出屏不当场销毁，留给 BulletList_UpdateChainCollisions 按当时球链的缝隙大小计分
	int			escaped;
	float		closestBallDist;   // 飞行过程中离最近的球有多近
	float		gapSize;           // 最近球两侧在曲线上的缝隙（已扣掉紧贴间距）

	HBulletList	bulletList;
	BallColor	color;

	HQC_Animation anim;
} Bullet;


BallColor Bullet_GetColor(HBullet hbullet) {
	return ((Bullet*)hbullet)->color;
}

v2f_t Bullet_GetPosition(HBullet hbullet) {
	return ((Bullet*)hbullet)->pos;
}

void Bullet_SetPosition(HBullet hbullet, v2f_t position) {
	Bullet* bullet = (Bullet*)hbullet;
	bullet->pos = position;
}

void Bullet_SetDirection(HBullet hbullet, float direction) {
	Bullet* bullet = (Bullet*)hbullet;
	bullet->direction = direction;
	bullet->dir.x = HQC_FCos(direction);
	bullet->dir.y = HQC_FSin(direction);
}

void Bullet_SetInsertion(HBullet hbullet, void* ball, bool isInsertingRight) {
	if (!ball) return;
	
	Bullet* bullet = (Bullet*)hbullet;

	bullet->insertionBall = ball;
	bullet->insertionBallChain = Ball_GetChain(ball);
	bullet->isShoudInsertBallToFront = isInsertingRight;
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

typedef struct BulletList {
	Bullet* arr[ARR_SIZE];
} BulletList;


HBulletList BulletList_Create() {
	BulletList* list = HQC_Memory_Allocate(sizeof(*list));

	for (int i = 0; i < ARR_SIZE; list->arr[i] = NULL, i++);

	return list;
}

static void _BulletList_DestroyBullet(HBulletList bulletList, int index) {
	BulletList* bl = (BulletList*)bulletList;

    if (bl->arr[index]) {
        HQC_Animation_Free(bl->arr[index]->anim);
        HQC_Memory_Free(bl->arr[index]);
        bl->arr[index] = NULL;
    }
}

static Bullet* _Bullet_CollisionBullet(Bullet* bullet) {
	for (int i = 0; i < ARR_SIZE; i++) {
		Bullet* b = ((BulletList*)(bullet->bulletList))->arr[i];

		if (b == NULL || b == bullet)
			continue;

		float dist = HQC_PointDistance(bullet->pos.x, bullet->pos.y, b->pos.x, b->pos.y);

		if (dist <= 48)
			return b;
	}
	
	return NULL;
}

void* Bullet_GetInsertionBall(HBullet hbullet) {
	Bullet* bil = (Bullet*)hbullet;

	return bil->insertionBall;
}

static void _Bullet_UpdateInserting(Bullet* bullet, int index) {
	if (!bullet->insertionBallChain)
		return;

	// 目标球可能已经被炸掉/进洞了
	bullet->insertionBall = BallChain_HasBall(bullet->insertionBallChain, bullet->insertionBall);

	if (!bullet->insertionBall) {
		_BulletList_DestroyBullet(bullet->bulletList, index);
		return;
	}

	Bullet* otherBullet = _Bullet_CollisionBullet(bullet);
	if (otherBullet) {
		otherBullet->insertTimer = 0;
	}

	// 计算插入点
	HLevel hlvl = BallChain_GetLevel(bullet->insertionBallChain);

    HBall shiftingChainBall;
	float insertCurvePos;
    if (bullet->isShoudInsertBallToFront) {
        insertCurvePos = Ball_GetPositionOnCurve(bullet->insertionBall) + BALLS_CHAIN_PAD;
        shiftingChainBall = Ball_Next(bullet->insertionBall);
    } else {
        HBall insertionBallPrev = Ball_Previous(bullet->insertionBall);

        float distance = Ball_GetDistanceBetweenBalls(insertionBallPrev, bullet->insertionBall);

        if (insertionBallPrev != NULL && distance < BALLS_CHAIN_PAD * 2) {
            insertCurvePos = Ball_GetPositionOnCurve(insertionBallPrev) + BALLS_CHAIN_PAD;
            shiftingChainBall = bullet->insertionBall;
        } else {
            insertCurvePos = Ball_GetPositionOnCurve(bullet->insertionBall) - BALLS_CHAIN_PAD;
            shiftingChainBall = NULL;
        }
    }

	v2f_t insertPos		 = Level_GetCurveCoords(hlvl, insertCurvePos);
	v2f_t insertDirPoint = Level_GetCurveCoords(hlvl, insertCurvePos + 1);

	// 插入动画朝向
	bullet->direction = HQC_FAtan2(insertDirPoint.y - bullet->pos.y, insertDirPoint.x - bullet->pos.x);

	// 🔴 取模必须"取正"：insertCurvePos 可能是负数（目标球贴链头时 - BALLS_CHAIN_PAD），
	//    C 的 (-30) % 50 = -30 → 帧号 -30 → 渲染时 FATAL（CI/Gauntlet 实测踩到过）
	{
		int frames = (int)HQC_Animation_FramesCount(bullet->anim);
		int frame  = frames > 0 ? ((int)insertCurvePos % frames) : 0;

		if (frame < 0) frame += frames;

		HQC_Animation_SetFrame(bullet->anim, frame);
	}

    // 向插入点靠拢
    bullet->pos.x = HQC_Lerp(bullet->pos.x, insertPos.x, 0.35f);
    bullet->pos.y = HQC_Lerp(bullet->pos.y, insertPos.y, 0.35f);

    // 腾位：把插入点前面的那一串球往前推
    if (shiftingChainBall != NULL) {
        v2f_t ballRightPosCoords = Ball_GetPositionCoords(shiftingChainBall);

        int guard = 0;
        while (HQC_PointDistance(bullet->pos.x, bullet->pos.y, ballRightPosCoords.x, ballRightPosCoords.y) < BALLS_CHAIN_PAD * 1.5f
               && guard++ < 64) {
            Ball_MoveSubChainFrom(shiftingChainBall, 1);
            ballRightPosCoords = Ball_GetPositionCoords(shiftingChainBall);
        }
    }

	// 动画结束 → 真正插入链中
	if (bullet->insertTimer == 0) {
		HBall newBall;

		if (bullet->isShoudInsertBallToFront) {
			newBall = BallChain_InsertAfterBall(
                    bullet->color,
                    bullet->insertionBall,
                    insertCurvePos
            );
		}
		else {
			newBall = BallChain_InsertBeforeBall(
                    bullet->color,
                    bullet->insertionBall,
                    insertCurvePos
            );
		}

		Ball_BulletInsertDone(bullet->insertionBall);

		BallChain_ExplodeBalls(newBall, 0);

		_BulletList_DestroyBullet(bullet->bulletList, index);

		return;
	}

	bullet->insertTimer--;
}


static void _Bullet_Update(Bullet* bullet, int index) {
	if (bullet == NULL) return;

	if (bullet->escaped) return;          // 等 chain 那边结算完再销毁

	if (bullet->insertionBall != NULL) {
		_Bullet_UpdateInserting(bullet, index);
		return;
	}

	// 真实弹道：沿瞄准方向匀速飞行
	bullet->pos.x += bullet->dir.x * bullet->spd;
	bullet->pos.y += bullet->dir.y * bullet->spd;

	HQC_Animation_Tick(bullet->anim);   // 飞行时的滚动效果

	if (bullet->pos.x < -50 || bullet->pos.x > 1330 ||
	    bullet->pos.y < -50 || bullet->pos.y > 770) {
		// 飞出屏幕：这一发打空了，连击链断开（原版行为）
		// 不在这里销毁：出屏结算（GAP BONUS）需要球链信息，交给
		// BulletList_UpdateChainCollisions 处理（它拿得到 chain）
		bullet->escaped = 1;
		Statistics_BreakChain();
	}
}


// 命中球链检测（2026-09-29 从 BallChain 迁到这里：链只负责自己的物理，
// 命中判定属于子弹）。命中后把子弹标成"插入中"，下一帧开始插入动画。
static bool _Bullet_IsInsertInFront(Bullet* bullet, HBall ball, HLevel level) {
	float pos = Ball_GetPositionOnCurve(ball);

	v2f_t front = Level_GetCurveCoords(level, pos + BALLS_CHAIN_PAD);
	v2f_t back  = Level_GetCurveCoords(level, pos - BALLS_CHAIN_PAD);

	float distFront = HQC_PointDistance(bullet->pos.x, bullet->pos.y, front.x, front.y);
	float distBack  = HQC_PointDistance(bullet->pos.x, bullet->pos.y, back.x, back.y);

	return distFront < distBack;
}


void BulletList_UpdateChainCollisions(HBulletList bulletList, HBallChain chain) {
	BulletList* bl = (BulletList*)bulletList;
	if (!bl || !chain) return;

	HLevel level = BallChain_GetLevel(chain);
	int len = BallChain_Length(chain);

	for (int i = 0; i < ARR_SIZE; i++) {
		Bullet* bullet = bl->arr[i];

		if (bullet == NULL)
			continue;

		// ── 出屏结算（GAP BONUS）──────────────────────────────────────────
		// 子弹从球链的缝隙里钻出去 = 奖励（原版行为）；缝隙越大分越多。
		if (bullet->escaped) {
			float gap = bullet->gapSize;
			float dist = bullet->closestBallDist;

			// 判据：确实是贴着球链从缝里钻过去的（不是飞到空地上），且两球之间真有缝
			if (gap > 12.0f && dist > 26.0f && dist < 220.0f) {
				Statistics_AddBulletGap(gap);

				HQC_DJ_PlaySound(Store_GetSoundByID(SND_GAPBONUS1));

				AutoTest_Event("GAP_BONUS", "gap=%.0f dist=%.0f shots=%d",
				               gap, dist, Statistics_GapCount());
				AutoTest_Observe("gaps", Statistics_GapCount());
			} else {
				AutoTest_Event("GAP_MISS", "gap=%.0f dist=%.0f", gap, dist);
			}

			_BulletList_DestroyBullet(bulletList, i);
			continue;
		}

		if (bullet->insertionBall != NULL)
			continue;

		HBall hit = NULL;
		float best = DISTANCE_TO_COLLIDE;

		// 全局最近球（不受命中阈值限制）：用来衡量这一发"擦得多近"
		float closest = 1e9f;
		int   closestIdx = -1;

		for (int j = 0; j < len; j++) {
			HBall ball = BallChain_GetBallAt(chain, j);

			if (!ball || Ball_IsExploding(ball) || Ball_IsInTunnel(ball))
				continue;

			v2f_t bp = Ball_GetPositionCoords(ball);

			float d2 = (bullet->pos.x - bp.x) * (bullet->pos.x - bp.x)
			         + (bullet->pos.y - bp.y) * (bullet->pos.y - bp.y);

			if (d2 < closest) {
				closest = d2;
				closestIdx = j;
			}

			if (d2 < best) {
				best = d2;
				hit = ball;
			}
		}

		// 记录"最贴近球链时离多近"与"那一刻的缝隙"（出屏时用来判 GAP BONUS）
		// ⚠️ 必须是整段飞行的**最小**距离：只看出屏那一帧会得到"在屏幕边缘时
		//    离球多远"，那不是穿缝的证据（实测会给出 dist=438 的假阳性）
		if (closestIdx >= 0) {
			float d = HQC_FSqrt(closest);

			if (d < bullet->closestBallDist) {
				bullet->closestBallDist = d;

				HBall prev = BallChain_GetBallAt(chain, closestIdx - 1);
				HBall next = BallChain_GetBallAt(chain, closestIdx + 1);
				HBall cur  = BallChain_GetBallAt(chain, closestIdx);

				float curPos = Ball_GetPositionOnCurve(cur);

				float gapPrev = prev ? (curPos - Ball_GetPositionOnCurve(prev) - BALLS_CHAIN_PAD) : 0.0f;
				float gapNext = next ? (Ball_GetPositionOnCurve(next) - curPos - BALLS_CHAIN_PAD) : 0.0f;

				float gap = gapPrev > gapNext ? gapPrev : gapNext;
				if (gap < 0.0f) gap = 0.0f;

				bullet->gapSize = gap;
			}
		}

		if (hit != NULL) {
			bool insertFront = _Bullet_IsInsertInFront(bullet, hit, level);
			Bullet_SetInsertion(bullet, hit, insertFront);
		}
	}
}



bool BulletList_TryHitPoint(HBulletList bulletList, v2f_t point, float radius) {
	BulletList* bl = (BulletList*)bulletList;
	if (!bl) return false;

	for (int i = 0; i < ARR_SIZE; i++) {
		Bullet* bullet = bl->arr[i];

		if (bullet == NULL || bullet->insertionBall != NULL)
			continue;

		float d2 = (bullet->pos.x - point.x) * (bullet->pos.x - point.x)
		         + (bullet->pos.y - point.y) * (bullet->pos.y - point.y);

		if (d2 <= radius * radius) {
			_BulletList_DestroyBullet(bulletList, i);
			return true;
		}
	}

	return false;
}


void BulletList_Update(HBulletList bulletList) {
	BulletList* bl = (BulletList*)bulletList;

	for (int i = 0; i < ARR_SIZE; i++) {
		Bullet* bullet = bl->arr[i];

		if (bullet == NULL)
			continue;

		_Bullet_Update(bullet, i);
	}
}


void BulletList_Draw(HBulletList bulletList) {
	BulletList* bl = (BulletList*)bulletList;

	for (int i = 0; i < ARR_SIZE; i++) {
		Bullet* bullet = bl->arr[i];

		if (bullet == NULL)
			continue;

		HQC_Artist_DrawSetAngle(bullet->direction + M_PI_2);
		HQC_Artist_DrawAnimation(bullet->anim, (bullet->pos).x, (bullet->pos).y);
		HQC_Artist_DrawSetAngle(0);
	}
}


void BulletList_Add(
	HBulletList bulletList, BallColor bulletColor, v2f_t bulletPosition, float bulletSpd, float bulletDirection
) {
	// 同时在场的球上限（原版 8）
	BulletList* bl = (BulletList*)bulletList;

	for (int i = 0; i < ARR_SIZE; i++) {
		if (bl->arr[i] != NULL)
			continue;

		Bullet* bullet = HQC_Memory_Allocate(sizeof * bullet);

		bullet->pos				= bulletPosition;
		bullet->color			= bulletColor;
		bullet->direction		= bulletDirection;
		bullet->dir.x			= HQC_FCos(bulletDirection);
		bullet->dir.y			= HQC_FSin(bulletDirection);
		bullet->spd				= bulletSpd > 0 ? bulletSpd : BULLET_SPEED;

		bullet->insertionBall				= NULL;
		bullet->isShoudInsertBallToFront	= false;
		bullet->insertionBallChain			= NULL;

		bullet->insertTimer		= INSERT_TIME;
		bullet->bulletList		= bl;

		// GAP BONUS（出屏结算）字段
		bullet->escaped			= 0;
		bullet->closestBallDist	= 9999.0f;
		bullet->gapSize			= 0.0f;

		bullet->anim = HQC_Animation_Clone(Store_GetAnimationByID(ANIM_BALL_BLUE + bullet->color));

		bl->arr[i] = bullet;

		break;
	}
}


HBullet BulletList_GetBullet(HBulletList bulletList, int index) {
	if (index < 0 || index >= ARR_SIZE) {
		return NULL;
	}
	
	BulletList* bl = (BulletList*)bulletList;
	
	return bl->arr[index];
}


void BulletList_Free(HBulletList bulletList) {
    BulletList* bl = (BulletList*)bulletList;
    if (!bl) return;

    for (int i = 0; i < ARR_SIZE; i++) {
        if (bl->arr[i] != NULL) {
            _BulletList_DestroyBullet(bulletList, i);
        }
    }
    
    HQC_Memory_Free(bl);
}