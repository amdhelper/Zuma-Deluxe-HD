#pragma once

#include "../global/HQC.h"
#include "ResourceStore.h"
#include "BallColors.h"
//#include "BallChain.h"

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

typedef void* HBullet;

#ifndef HBALLCHAIN_TYPEDEF
#define HBALLCHAIN_TYPEDEF
typedef void* HBallChain;   // 前向声明（BallChain.h 也有，用宏避免重复）
#endif

#define BULLET_SPEED 15.0f    // 子弹每帧飞行距离（原版手感）

BallColor Bullet_GetColor(HBullet hbullet);
v2f_t Bullet_GetPosition(HBullet hbullet);

void Bullet_SetPosition(HBullet hbullet, v2f_t position);
void Bullet_SetDirection(HBullet hbullet, float direction);

void Bullet_SetInsertion(HBullet hbullet, void* ball, bool isInsertingRight);

void* Bullet_GetInsertionBall(HBullet hbullet);

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

typedef void* HBulletList;

HBulletList BulletList_Create();
void BulletList_Update(HBulletList bulletList);
void BulletList_UpdateChainCollisions(HBulletList bulletList, HBallChain chain);
// 宝石/金币碰撞：半径内命中就消耗掉那颗子弹并返回 true
bool BulletList_TryHitPoint(HBulletList bulletList, v2f_t point, float radius);
void BulletList_Draw(HBulletList bulletList);
void BulletList_Add(
	HBulletList bulletList, 
	BallColor bulletColor, v2f_t bulletPosition, float bulletSpd, float bulletDirection
);

HBullet BulletList_GetBullet(HBulletList bulletList, int index);

void BulletList_Free(HBulletList bulletList);


