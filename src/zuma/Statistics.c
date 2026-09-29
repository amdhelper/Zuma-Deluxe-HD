#include <stdio.h>
#include "Statistics.h"

#include "FloatingText.h"

static int score_        = 0;      // 本关分数
static int lives_        = 3;
static int coins_        = 0;      // 累计宝石
static int ballsDestroyed_ = 0;
static int comboCount_   = 0;      // 连续链式反应次数
static int maxCombo_     = 0;
static int totalCombos_  = 0;
static int chainBonus_   = 0;      // 当前链式连击（不中断累计）
static int maxChainBonus_ = 0;
static int levelsCompleted_ = 0;

static int gapPoints_ = 0;
static int gapCount_  = 0;
static int lastExplosionBalls_ = 0;
static int lastExplosionPoints_ = 0;
static BallColor prevBallColor = BALL_NONE;
static BallColor curBallColor  = BALL_NONE;


void Statistics_Init() {
    score_        = 0;
    lives_        = 3;
    coins_        = 0;
    ballsDestroyed_ = 0;
    comboCount_   = 0;
    maxCombo_     = 0;
    totalCombos_  = 0;
    chainBonus_   = 0;
    maxChainBonus_ = 0;
    levelsCompleted_ = 0;

    gapPoints_ = 0;
    gapCount_  = 0;
    lastExplosionBalls_ = 0;
    lastExplosionPoints_ = 0;
    prevBallColor = BALL_NONE;
    curBallColor  = BALL_NONE;
}


void Statistics_ResetLevel() {
    score_        = 0;
    gapPoints_    = 0;
    gapCount_     = 0;
    chainBonus_   = 0;
    comboCount_   = 0;
    lastExplosionBalls_ = 0;
    lastExplosionPoints_ = 0;
}


///////////////////////////////////////////////////////////////////////////////
// 命 / 分数 / 累计
///////////////////////////////////////////////////////////////////////////////

int  Statistics_Lives() { return lives_; }
void Statistics_SetLives(int lives) { lives_ = lives; }

int Statistics_LoseLife() {
    if (lives_ > 0) lives_--;
    return lives_;
}

void Statistics_AddLives(int lives) { lives_ += lives; }

int  Statistics_Score() { return score_; }

void Statistics_AddScore(int points) {
    if (points <= 0) return;
    score_ += points;
}

int  Statistics_Coins() { return coins_; }
void Statistics_AddCoin() { coins_++; }

int  Statistics_BallsDestroyed() { return ballsDestroyed_; }
int  Statistics_MaxCombo() { return maxCombo_; }
int  Statistics_TotalCombos() { return totalCombos_; }
int  Statistics_ChainBonus() { return chainBonus_; }
int  Statistics_MaxChainBonus() { return maxChainBonus_; }
int  Statistics_LevelsCompleted() { return levelsCompleted_; }
void Statistics_AddLevelCompleted() { levelsCompleted_++; }


///////////////////////////////////////////////////////////////////////////////
// 爆炸记账
///////////////////////////////////////////////////////////////////////////////

// 计分规则（沿用原版 Zuma 手感）：
//   基础 = 每球 10 分；链式反应每级 +100；链式连击 ≥5 额外 100 + 10*(n-5)
int Statistics_RegisterExplosion(int explodedBalls, BallColor color,
                                 int comboLevel, int isChainReaction) {
    ballsDestroyed_ += explodedBalls;
    lastExplosionBalls_ = explodedBalls;

    if (isChainReaction) {
        comboCount_++;
        if (comboCount_ > maxCombo_) maxCombo_ = comboCount_;
        totalCombos_++;
        chainBonus_++;
        if (chainBonus_ > maxChainBonus_) maxChainBonus_ = chainBonus_;
    } else {
        comboCount_ = 0;
        chainBonus_ = 1;
    }

    int points = explodedBalls * 10;

    if (isChainReaction) {
        points += comboLevel * 100;
    }

    if (chainBonus_ >= 5)
        points += 100 + 10 * (chainBonus_ - 5);

    points += gapPoints_;

    lastExplosionPoints_ = points;
    score_ += points;

    prevBallColor = curBallColor;
    curBallColor  = color;

    return points;
}


void Statistics_BreakChain() {
    comboCount_ = 0;
    chainBonus_ = 0;
}


void Statistics_BuildAndInstantiateFloatingText(float x, float y, uint32_t color) {
    char comboText[48] = {0};
    if (comboCount_ > 0)
        snprintf(comboText, sizeof(comboText), "COMBO X%d\n", comboCount_ + 1);

    char gapText[48] = {0};
    if (gapCount_ > 0)
        snprintf(gapText, sizeof(gapText), "%sGAP BONUS\n",
                 gapCount_ == 3 ? "TRIPLE" : gapCount_ == 2 ? "DOUBLE" : "");

    char chainText[48] = {0};
    if (chainBonus_ >= 5)
        snprintf(chainText, sizeof(chainText), "CHAIN BONUS X%d\n", chainBonus_);

    FloatingTextFactory_Instantiate(
            x, y, color,
            "+%d\n%s%s%s", lastExplosionPoints_, comboText, gapText, chainText);

    gapCount_ = 0;
    gapPoints_ = 0;
}


void Statistics_AddBulletGap(float distance) {
    // 子弹穿过球链缝隙飞出场外 = GAP BONUS（缝隙越大分越高）
    const float GAP_MAX = 260.0f;

    if (distance < 0) distance = 0;
    if (distance > GAP_MAX) distance = GAP_MAX;

    int bonus = 50 + (int)(450.0f * distance / GAP_MAX);

    if (gapCount_ > 1)
        bonus *= 2;

    gapPoints_ += bonus;
    gapCount_  += 1;
}


int Statistics_GapCount(void) { return gapCount_; }