#pragma once

// 自动测试（CI / 无头回归）：
//   * 脚本化输入（HQC_Input_SetScripted）驱动青蛙瞄准/开火/换球
//   * 结构化事件日志（stdout 前缀 [TEST]），供测试脚本断言
//   * 汇总报告 + 退出码（0 正常、3 崩溃兜底、4 卡死/超时）
// 设计见 docs/ROADMAP.md 阶段 0。决策逻辑在各场景（能直接读到游戏状态），
// 本模块只做"设置脚本输入 + 记录/汇总"。

#include "../global/HQC.h"

void AutoTest_Init(void);
int  AutoTest_IsActive(void);
int  AutoTest_MaxFrames(void);

// 脚本输入（下一帧生效；HQC_Input_Update 在帧首锁存）
void AutoTest_SetPointer(int x, int y, int leftDown, int rightDown);
void AutoTest_ReleasePointer(void);

// 脚本按键（脉冲：press 后下一帧 release，由场景自行成对调用）
void AutoTest_KeyDown(HQC_Key key);
void AutoTest_KeyUp(HQC_Key key);

// 结构化事件（stdout: [TEST] <name> <kv...>）
void AutoTest_Event(const char* name, const char* format, ...);

// 汇总（自动测试结束时调用）
void AutoTest_Report(int frames, int exitCode);

// 场景在完成目标（如跑够关数/Game Over）后请求结束整个运行
void AutoTest_RequestStop(void);
int  AutoTest_StopRequested(void);

// 场景可在每帧结束前调用：记录关键状态供断言
void AutoTest_Observe(const char* key, int value);

int  AutoTest_GetObserved(const char* key);