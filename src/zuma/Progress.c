#include "Progress.h"

// 建目录：POSIX mkdir(path, mode) / Windows _mkdir(path)（参数个数不同）
#ifdef _WIN32
    #include <direct.h>
    #define _Progress_MkDir(p, mode) _mkdir(p)
#else
    #include <sys/stat.h>
    #define _Progress_MkDir(p, mode) mkdir((p), (mode))
#endif

#include "../global/HQC.h"
#include "LevelMgr.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define PROG_MAX_STAGES  16
#define PROG_MAX_LEVELS  32

#define PROG_VERSION     2

typedef struct {
    int score;
    int seconds;
} LevelRecord;

static struct {
    LevelRecord records[PROG_MAX_STAGES][PROG_MAX_LEVELS];

    int unlockedStage;
    int unlockedLevel;

    unsigned char stars[PROG_MAX_STAGES][PROG_MAX_LEVELS];   // 0..3

    // Gauntlet 排行榜（分数降序，最多 PROG_BOARD_SIZE 条）
    int boardCount;
    ProgressGauntletEntry board[PROG_BOARD_SIZE];

    int currentStage;
    int currentLevel;

    int loaded;
    char path[512];
} prog;


static void _Progress_EnsurePath() {
    if (prog.path[0]) return;

    const char* override = getenv("ZUMA_PROGRESS_FILE");
    if (override && override[0]) {
        snprintf(prog.path, sizeof(prog.path), "%s", override);
        return;
    }

    const char* home = getenv("HOME");
    if (!home || !home[0]) home = ".";

    char dir[512];
    snprintf(dir, sizeof(dir), "%s/.local/share", home);
    _Progress_MkDir(dir, 0755);
    snprintf(dir, sizeof(dir), "%s/.local/share/zumahd", home);
    _Progress_MkDir(dir, 0755);

    snprintf(prog.path, sizeof(prog.path), "%s/.local/share/zumahd/progress.dat", home);
}


const char* Progress_FilePath() {
    _Progress_EnsurePath();
    return prog.path;
}


void Progress_Load() {
    _Progress_EnsurePath();

    prog.unlockedStage = 0;
    prog.unlockedLevel = 0;
    prog.currentStage  = 0;
    prog.currentLevel  = 0;
    memset(prog.records, 0, sizeof(prog.records));

    FILE* f = fopen(prog.path, "r");

    if (!f) {
        HQC_Log("Progress: no save file at %s (new player)", prog.path);
        prog.loaded = 1;
        return;
    }

    char line[256];
    int  version = 0;
    int  bestCount = 0;

    while (fgets(line, sizeof(line), f)) {
        int s, l, score, sec;

        if (sscanf(line, "version %d", &version) == 1)
            continue;

        if (sscanf(line, "unlocked %d %d", &s, &l) == 2) {
            if (s >= 0 && s < PROG_MAX_STAGES && l >= 0 && l < PROG_MAX_LEVELS) {
                prog.unlockedStage = s;
                prog.unlockedLevel = l;
            }
            continue;
        }

        if (sscanf(line, "last %d %d", &s, &l) == 2) {
            if (s >= 0 && s < PROG_MAX_STAGES && l >= 0 && l < PROG_MAX_LEVELS) {
                prog.currentStage = s;
                prog.currentLevel = l;
            }
            continue;
        }

        if (sscanf(line, "best %d %d %d %d", &s, &l, &score, &sec) == 4) {
            if (s >= 0 && s < PROG_MAX_STAGES && l >= 0 && l < PROG_MAX_LEVELS) {
                prog.records[s][l].score   = score;
                prog.records[s][l].seconds = sec;
                bestCount++;
            }
            continue;
        }

        // stars <stage> <level> <0..3>
        if (sscanf(line, "stars %d %d %d", &s, &l, &score) == 3) {
            if (s >= 0 && s < PROG_MAX_STAGES && l >= 0 && l < PROG_MAX_LEVELS)
                prog.stars[s][l] = (unsigned char)(score < 0 ? 0 : (score > 3 ? 3 : score));
            continue;
        }

        // gauntlet <score> <wave> <difficulty> <seconds>
        if (sscanf(line, "gauntlet %d %d %d %d", &score, &s, &l, &sec) == 4) {
            // 文件里的顺序可能是任意的（手改过也算）→ 走同一套插入逻辑保持有序
            Progress_GauntletSubmit(score, s, l, sec);
            continue;
        }
    }

    fclose(f);

    prog.loaded = 1;

    HQC_Log("Progress: loaded %s (version %d, unlocked %d-%d, %d records, current %d-%d)",
            prog.path, version, prog.unlockedStage + 1, prog.unlockedLevel + 1,
            bestCount, prog.currentStage + 1, prog.currentLevel + 1);
}


void Progress_Save() {
    _Progress_EnsurePath();

    char tmp[600];
    snprintf(tmp, sizeof(tmp), "%s.tmp", prog.path);

    FILE* f = fopen(tmp, "w");
    if (!f) {
        HQC_Log("Progress: cannot write %s (progress will not be saved)", tmp);
        return;
    }

    fprintf(f, "version %d\n", PROG_VERSION);
    fprintf(f, "unlocked %d %d\n", prog.unlockedStage, prog.unlockedLevel);
    fprintf(f, "last %d %d\n", prog.currentStage, prog.currentLevel);

    for (int s = 0; s < PROG_MAX_STAGES; s++) {
        for (int l = 0; l < PROG_MAX_LEVELS; l++) {
            if (prog.records[s][l].score <= 0)
                continue;

            fprintf(f, "best %d %d %d %d\n", s, l,
                    prog.records[s][l].score, prog.records[s][l].seconds);
        }
    }

    // 星级
    for (int s = 0; s < PROG_MAX_STAGES; s++) {
        for (int l = 0; l < PROG_MAX_LEVELS; l++) {
            if (prog.stars[s][l] > 0)
                fprintf(f, "stars %d %d %d\n", s, l, (int)prog.stars[s][l]);
        }
    }

    // Gauntlet 排行榜
    for (int i = 0; i < prog.boardCount; i++) {
        fprintf(f, "gauntlet %d %d %d %d\n", prog.board[i].score, prog.board[i].wave,
                prog.board[i].difficulty, prog.board[i].seconds);
    }

    fclose(f);

    // 先写临时文件再改名：中途崩溃不会破坏已有存档
    if (rename(tmp, prog.path) != 0)
        HQC_Log("Progress: rename %s -> %s failed", tmp, prog.path);
}


int Progress_IsUnlocked(int stage, int level) {
    if (stage < prog.unlockedStage) return 1;
    if (stage > prog.unlockedStage) return 0;

    return level <= prog.unlockedLevel;
}


void Progress_Unlock(int stage, int level) {
    if (stage > prog.unlockedStage ||
        (stage == prog.unlockedStage && level > prog.unlockedLevel)) {
        prog.unlockedStage = stage;
        prog.unlockedLevel = level;

        HQC_Log("Progress: unlocked %d-%d", stage + 1, level + 1);
    }
}


int Progress_GetCurrentStage() { return prog.currentStage; }
int Progress_GetCurrentLevel() { return prog.currentLevel; }


void Progress_SetCurrent(int stage, int level) {
    prog.currentStage = stage;
    prog.currentLevel = level;
}


int Progress_GetBestScore(int stage, int level) {
    if (stage < 0 || stage >= PROG_MAX_STAGES || level < 0 || level >= PROG_MAX_LEVELS)
        return 0;

    return prog.records[stage][level].score;
}


int Progress_GetBestSeconds(int stage, int level) {
    if (stage < 0 || stage >= PROG_MAX_STAGES || level < 0 || level >= PROG_MAX_LEVELS)
        return 0;

    return prog.records[stage][level].seconds;
}


void Progress_ReportLevel(int stage, int level, int score, int seconds, int completed) {
    if (stage < 0 || stage >= PROG_MAX_STAGES || level < 0 || level >= PROG_MAX_LEVELS)
        return;

    LevelRecord* rec = &prog.records[stage][level];

    if (score > rec->score)
        rec->score = score;

    // 最佳用时只记"过关"的那次（没过关的用时没有意义）
    if (completed) {
        if (rec->seconds <= 0 || (seconds > 0 && seconds < rec->seconds))
            rec->seconds = seconds;
    }

    if (completed) {
        // 解锁本关的下一关（同大关内顺延，到末尾则进下一大关第 1 关）
        int stageCount = LevelMgr_GetStageCount();
        int levelCount = LevelMgr_GetLevelCount(stage);

        if (level + 1 < levelCount) {
            Progress_Unlock(stage, level + 1);
        } else if (stage + 1 < stageCount) {
            Progress_Unlock(stage + 1, 0);
        }
    }

    Progress_SetCurrent(stage, level);

    Progress_Save();

    HQC_Log("Progress: report %d-%d score=%d (%s) time=%ds (best score %d, best time %ds)",
            stage + 1, level + 1, score, completed ? "completed" : "failed", seconds,
            rec->score, rec->seconds);
}


////////////////////////////////////////////////////////////////////////////////
// 星级（ROADMAP 3.10）
////////////////////////////////////////////////////////////////////////////////

int Progress_GetStars(int stage, int level) {
    if (stage < 0 || stage >= PROG_MAX_STAGES || level < 0 || level >= PROG_MAX_LEVELS)
        return 0;

    return (int)prog.stars[stage][level];
}


int Progress_ReportStars(int stage, int level, int score, int gaugeScore) {
    if (stage < 0 || stage >= PROG_MAX_STAGES || level < 0 || level >= PROG_MAX_LEVELS)
        return 0;

    if (gaugeScore <= 0) gaugeScore = 1000;   // 兜底，免得除 0

    int stars = 0;

    if (score >= (gaugeScore * 140) / 100)      stars = 3;   // 过线 140%
    else if (score >= (gaugeScore * 115) / 100) stars = 2;   // 过线 115%
    else if (score >= gaugeScore)               stars = 1;   // 刚好过线

    // 只升不降：以前拿过 3 星，这局打差了不该退回 2 星
    if (stars > (int)prog.stars[stage][level])
        prog.stars[stage][level] = (unsigned char)stars;

    HQC_Log("Progress: stars %d-%d = %d (score %d vs gauge %d, best %d)",
            stage + 1, level + 1, stars, score, gaugeScore,
            (int)prog.stars[stage][level]);

    // ⚠️ 必须落盘（prog.loaded 为 0 说明还在 Load 里，不能反向写文件）
    if (prog.loaded)
        Progress_Save();

    return stars;
}


////////////////////////////////////////////////////////////////////////////////
// Gauntlet 排行榜（ROADMAP 3.11）
////////////////////////////////////////////////////////////////////////////////

int Progress_GauntletCount() {
    return prog.boardCount;
}


const ProgressGauntletEntry* Progress_GauntletEntryAt(int index) {
    if (index < 0 || index >= prog.boardCount)
        return NULL;

    return &prog.board[index];
}


int Progress_GauntletBest() {
    return prog.boardCount > 0 ? prog.board[0].score : 0;
}


void Progress_GauntletClear() {
    prog.boardCount = 0;
    memset(prog.board, 0, sizeof(prog.board));
}


int Progress_GauntletSubmit(int score, int wave, int difficulty, int seconds) {
    if (score <= 0)
        return 0;

    // 找插入位置：分数降序；同分则目数多的排前面
    int pos = prog.boardCount;

    for (int i = 0; i < prog.boardCount; i++) {
        if (score > prog.board[i].score ||
            (score == prog.board[i].score && wave > prog.board[i].wave)) {
            pos = i;
            break;
        }
    }

    // 进不了前 5 名
    if (pos >= PROG_BOARD_SIZE)
        return 0;

    int last = prog.boardCount < PROG_BOARD_SIZE ? prog.boardCount : PROG_BOARD_SIZE - 1;

    for (int i = last; i > pos; i--)
        prog.board[i] = prog.board[i - 1];

    prog.board[pos].score      = score;
    prog.board[pos].wave       = wave;
    prog.board[pos].difficulty = difficulty;
    prog.board[pos].seconds    = seconds;

    if (prog.boardCount < PROG_BOARD_SIZE)
        prog.boardCount++;

    // 落盘（Load 期间 prog.loaded 还是 0 → 不会在读取时写回文件）
    if (prog.loaded)
        Progress_Save();

    return pos + 1;   // 1-based 名次
}