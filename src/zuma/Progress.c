#include "Progress.h"

#include "../global/HQC.h"
#include "LevelMgr.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define PROG_MAX_STAGES  16
#define PROG_MAX_LEVELS  32

#define PROG_VERSION     1

typedef struct {
    int score;
    int seconds;
} LevelRecord;

static struct {
    LevelRecord records[PROG_MAX_STAGES][PROG_MAX_LEVELS];

    int unlockedStage;
    int unlockedLevel;

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
    mkdir(dir, 0755);
    snprintf(dir, sizeof(dir), "%s/.local/share/zumahd", home);
    mkdir(dir, 0755);

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