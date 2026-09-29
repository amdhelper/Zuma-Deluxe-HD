#include "Frog.h"

#include "../global/HQC.h"

#include "ResourceStore.h"
#include "BallColors.h"

#define _USE_MATH_DEFINES
#include <math.h>
#include <stdlib.h>

typedef struct Frog {
    HBulletList bulletList;

    HQC_Sprite spr;
    HQC_Sprite sprTongue;
    HQC_Sprite sprPlate;

    HQC_Animation animBlink;
    HQC_Animation animNextBall;

    BallColor  ballColor;
    BallColor  nextBallColor;
    int        colorCount;

    float tongueExpand;
    float ballExpand;

    bool isFire;
    float fireRecoil;

    float angle;

    v2f_t posStart;
    v2f_t pos;  

    v2f_t ballPos;

    float fireRecoilTick;
} Frog; 

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////

static Frog* _Frog(HFrog hfrog) {
    return (Frog*)hfrog;
}

static float _Frog_RandFloat() {
    return (float)rand() / (float)RAND_MAX;
}

static BallColor _Frog_RandomColor(Frog* frog) {
    int n = frog->colorCount;
    if (n < 1) n = 1;
    if (n > 6) n = 6;

    return (BallColor)(rand() % n);
}

static void _Frog_NextBall(Frog* frog) {
    frog->ballColor = frog->nextBallColor;

    frog->nextBallColor = _Frog_RandomColor(frog);

    HQC_Animation_SetFrame(frog->animNextBall, frog->nextBallColor);
}

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////

HFrog Frog_Create(float x, float y, HBulletList bulletList) {
    Frog* frog = HQC_Memory_Allocate(sizeof(*frog));

    frog->angle         = 0.f;
    frog->spr           = Store_GetSpriteByID(SPR_FROG);
    frog->sprTongue     = Store_GetSpriteByID(SPR_FROG_TONGUE);
    frog->sprPlate      = Store_GetSpriteByID(SPR_FROG_PLATE);

    frog->isFire = false;
    frog->fireRecoil = false;

    frog->animBlink = HQC_Animation_Clone(Store_GetAnimationByID(ANIM_FROG_BLINK));
    frog->animNextBall = HQC_Animation_Clone(Store_GetAnimationByID(ANIM_FROG_BALLS));

    HQC_Animation_SetSpeed(frog->animBlink, 0.0);
    HQC_Animation_SetLooping(frog->animBlink, false);
    HQC_Animation_SetSpeed(frog->animNextBall, 0.0);

    frog->tongueExpand = 24;
    frog->ballExpand = 32;

    frog->bulletList    = bulletList;

    frog->colorCount = 4;

    frog->ballColor = BALL_BLUE;
    frog->nextBallColor = BALL_GREEN;

    frog->fireRecoilTick = 0;

    frog->pos.x         = x;
    frog->pos.y         = y;

    frog->posStart.x    = x;
    frog->posStart.y    = y;

    return frog;
}


void Frog_Configure(HFrog hfrog, int colorCount) {
    Frog* frog = _Frog(hfrog);
    if (!frog) return;

    if (colorCount < 1) colorCount = 1;
    if (colorCount > 6) colorCount = 6;

    frog->colorCount = colorCount;

    frog->ballColor     = (BallColor)(rand() % colorCount);
    frog->nextBallColor = (BallColor)(rand() % colorCount);

    HQC_Animation_SetFrame(frog->animNextBall, frog->nextBallColor);
}


BallColor Frog_GetBallColor(HFrog hfrog) {
    Frog* frog = _Frog(hfrog);
    return frog ? frog->ballColor : BALL_NONE;
}


BallColor Frog_GetNextBallColor(HFrog hfrog) {
    Frog* frog = _Frog(hfrog);
    return frog ? frog->nextBallColor : BALL_NONE;
}


void Frog_SetColors(HFrog hfrog, BallColor current, BallColor next) {
    Frog* frog = _Frog(hfrog);
    if (!frog) return;

    if (current != BALL_NONE) frog->ballColor = current;
    if (next != BALL_NONE)    frog->nextBallColor = next;

    HQC_Animation_SetFrame(frog->animNextBall, frog->nextBallColor);
}


void Frog_SwapBalls(HFrog hfrog) {
    Frog* frog = _Frog(hfrog);
    if (!frog) return;

    BallColor tmp = frog->ballColor;
    frog->ballColor = frog->nextBallColor;
    frog->nextBallColor = tmp;

    HQC_Animation_SetFrame(frog->animNextBall, frog->nextBallColor);

    HQC_DJ_PlaySound(Store_GetSoundByID(SND_BUTTON2));
}


v2f_t Frog_GetPos(HFrog hfrog) {
    Frog* frog = _Frog(hfrog);
    return frog ? frog->pos : v2f_t_default;
}


float Frog_GetAngle(HFrog hfrog) {
    Frog* frog = _Frog(hfrog);
    return frog ? frog->angle : 0.0f;
}


bool Frog_IsFiring(HFrog hfrog) {
    Frog* frog = _Frog(hfrog);
    return frog ? frog->isFire : false;
}


static v2f_t _Frog_GetBallPos(Frog* frog) {
    float fcos = HQC_FCos(frog->angle);
    float fsin = HQC_FSin(frog->angle);

    v2f_t pos = {
        frog->pos.x + frog->ballExpand * fcos,
        frog->pos.y + frog->ballExpand * fsin
    };

    return pos;
}


v2f_t Frog_GetBallPos(HFrog hfrog) {
    Frog* frog = _Frog(hfrog);
    if (!frog) return v2f_t_default;

    return _Frog_GetBallPos(frog);
}


void Frog_Update(HFrog hfrog) {
    Frog* frog = _Frog(hfrog);
    if (!frog) return;

    v2i_t mousePos = HQC_Input_MouseGetPosition();

    frog->angle = HQC_FAtan2(mousePos.y - frog->pos.y, mousePos.x - frog->pos.x);

    // 右键：交换当前/下一颗球（原版行为，鼠标右键）
    if (HQC_Input_MouseRightPressed() && !frog->isFire)
        Frog_SwapBalls(hfrog);

    // 左键：发射
    if (HQC_Input_MouseLeftPressed() && !frog->isFire) {
        HQC_DJ_PlaySound(Store_GetSoundByID(SND_FIREBALL1));

        HQC_Animation_SetSpeed(frog->animBlink, 0.5);

        v2f_t ballPos = _Frog_GetBallPos(frog);
        BulletList_Add(frog->bulletList, frog->ballColor, ballPos, BULLET_SPEED, frog->angle);
        _Frog_NextBall(frog);

        frog->ballExpand = 0;

        frog->fireRecoilTick = 0;
        frog->isFire = true;
    }

    // 发射后坐力/舌头回缩动画
    if (frog->isFire) {
        frog->fireRecoilTick += 0.25;

        float fcos = HQC_FCos(frog->angle);
        float fsin = HQC_FSin(frog->angle);

        float recoil = HQC_FSin(frog->fireRecoilTick);

        if (recoil <= 0.0) {
            recoil = 0.0;
            frog->ballExpand = 32;
            frog->isFire = false;
        }

        frog->tongueExpand = 24 - recoil * 24;

        if (frog->ballExpand < 32)
            frog->ballExpand += 2.5;

        frog->pos.x = frog->posStart.x - fcos * recoil * 8;
        frog->pos.y = frog->posStart.y - fsin * recoil * 8;
    } else {
        frog->tongueExpand = 24;
        frog->pos.x = frog->posStart.x;
        frog->pos.y = frog->posStart.y;
    }

    HQC_Animation_Tick(frog->animBlink);

    if (HQC_Animation_IsEnded(frog->animBlink)) {
        HQC_Animation_SetSpeed(frog->animBlink, 0);
        HQC_Animation_SetFrame(frog->animBlink, 0);
    }
}


void Frog_Draw(HFrog hfrog) {
    Frog* frog = _Frog(hfrog);
    if (!frog) return;

    float fcos = HQC_FCos(frog->angle);
    float fsin = HQC_FSin(frog->angle);

    HQC_Artist_DrawSprite(frog->sprPlate, frog->pos.x, frog->pos.y);
    
    HQC_Artist_DrawSetAngle(frog->angle - M_PI_2);

    HQC_Artist_DrawSprite(frog->spr, frog->pos.x, frog->pos.y);
    HQC_Artist_DrawSprite(frog->sprTongue, 
        frog->pos.x + frog->tongueExpand * fcos, frog->pos.y + frog->tongueExpand * fsin);

    HQC_Artist_DrawSetAngleDegrees(0);
}

void Frog_DrawTop(HFrog hfrog) {
    Frog* frog = _Frog(hfrog);
    if (!frog) return;

    float fcos = HQC_FCos(frog->angle);
    float fsin = HQC_FSin(frog->angle);

    HQC_Artist_DrawSetAngle(frog->angle - M_PI_2);

    v2f_t ballPos = _Frog_GetBallPos(frog);

    HQC_Artist_DrawAnimation(Store_GetAnimationByID(ANIM_BALL_BLUE + (int)frog->ballColor), ballPos.x, ballPos.y);

    HQC_Artist_DrawSetScale(1.5f);
    HQC_Artist_DrawAnimation(frog->animNextBall, frog->pos.x - 40 * fcos, frog->pos.y - 40 * fsin);
    HQC_Artist_DrawSetScale(1);
    HQC_Artist_DrawSetAlpha(1);

    HQC_Artist_DrawAnimation(frog->animBlink, frog->pos.x, frog->pos.y);

    HQC_Artist_DrawSetAngleDegrees(0);
}


void Frog_Destroy(HFrog hfrog) {
    Frog* frog = _Frog(hfrog);
    if (!frog) return;

    if (frog->animBlink) HQC_Animation_Free(frog->animBlink);
    if (frog->animNextBall) HQC_Animation_Free(frog->animNextBall);

    HQC_Memory_Free(frog);
}