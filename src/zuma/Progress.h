#ifndef ZUMAHD_PROGRESS_H
#define ZUMAHD_PROGRESS_H

// ═══════════════════════════════════════════════════════════════════════════════
// 进度存档（ROADMAP 2.4）
//   旧版本没有任何持久化：关掉程序，解锁进度、最高分、最佳用时全丢。
//   落盘位置：$ZUMA_PROGRESS_FILE（测试用）或 ~/.local/share/zumahd/progress.dat
//   文本格式（便于人工查看/测试）：
//     unlocked <stage> <level>        当前解锁到的关（0-based，即"最远可玩的一关"）
//     last     <stage> <level>        上次玩的关（选关界面默认选中）
//     best     <stage> <level> <score> <seconds>   该关最高分与最佳用时（秒）
// ═══════════════════════════════════════════════════════════════════════════════

void Progress_Load(void);
void Progress_Save(void);

int  Progress_IsUnlocked(int stage, int level);
void Progress_Unlock(int stage, int level);          // 解锁至少到 (stage, level)

int  Progress_GetCurrentStage(void);
int  Progress_GetCurrentLevel(void);
void Progress_SetCurrent(int stage, int level);

int  Progress_GetBestScore(int stage, int level);    // 0 = 无记录
int  Progress_GetBestSeconds(int stage, int level);  // 0 = 无记录

// 一关结束时调用（胜负都算成绩）：更新最高分/最佳用时；过关则解锁下一关
void Progress_ReportLevel(int stage, int level, int score, int seconds, int completed);

// 星级：0..3（0 = 没记录 / 未过关）
int  Progress_GetStars(int stage, int level);

// 过关时结算星级：1 星=过线，2 星=过线 115%，3 星=过线 140%
// （gaugeScore 来自关卡设置，已随难度不同；返回本次拿到的星数 1..3）
int  Progress_ReportStars(int stage, int level, int score, int gaugeScore);

// ── Gauntlet 排行榜（前 5 名，分数降序，同分比目数）─────────────────────────
#define PROG_BOARD_SIZE 5

typedef struct {
    int score;
    int wave;
    int difficulty;   // 0..3
    int seconds;
} ProgressGauntletEntry;

int  Progress_GauntletCount(void);                                     // 榜上条目数（<=5）
const ProgressGauntletEntry* Progress_GauntletEntryAt(int index);      // NULL = 越界
int  Progress_GauntletSubmit(int score, int wave, int difficulty, int seconds);
                                                                       // 返回名次 1..5；未上榜 0
int  Progress_GauntletBest(void);                                      // 榜首分数，0 = 空榜
void Progress_GauntletClear(void);                                     // 清空榜单

const char* Progress_FilePath(void);

#endif // ZUMAHD_PROGRESS_H