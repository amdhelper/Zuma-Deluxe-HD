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

const char* Progress_FilePath(void);

#endif // ZUMAHD_PROGRESS_H