# Zuma Deluxe HD — 开发任务清单（接下来的开发任务）

> 生成：2026-09-29。基准 = `master`（v2.0.0 架构重构 + 本次"核心闭环"提交 `9ab38ce`）。
> 每个任务都带**验收标准**与**验证方式**：本项目已具备无头自动测试，
> 任何改动都必须能用 `--autotest` 复现（见文末"怎么跑"）。

---

## 现状（先看清楚哪里断了）

进入 v2.0.0 时项目能载入关卡、能画球链，但**核心玩法是断的**，实测确认的断点：

| 断点 | 症状 | 状态 |
|---|---|---|
| 球链最前球越过曲线末端时 `pos -= curveLen`（回绕回起点） | 链永远循环，关卡永远无法结束 | ✅ 已修（进洞判负） |
| 生成器无上限（每帧无条件补球）、`GenerateSequence` 是空函数 | 球无限生成，无法通关 | ✅ 已修（按 settings->start，过 gauge 停生成） |
| 没有失败条件、没有命、没有 Game Over | 球进洞没反应 | ✅ 已修（3 命 + Game Over） |
| 分数只用于一次爆炸飘字，从不累计 | HUD/结算无从取值 | ✅ 已修（Statistics 记分板） |
| 没有 HUD（分数/命/进度/关卡号） | 玩家不知道进度 | ✅ 已修 |
| 子弹位置每帧直接赋鼠标坐标 | "射出的球"没有弹道，命中判定无意义 | ✅ 已修（真实抛射 + 插入动画） |
| `HQC_DJ_LoadMusic` 是空函数（缺 return）、PlayMusic 空实现 | 全程无声 | ✅ 已修（BASS MOD + order 切曲） |
| 输入边沿检测放在查询函数里（静态变量） | 同帧第二个调用者拿不到 pressed → 点击"没反应" | ✅ 已修（每帧锁存） |
| 青蛙颜色硬编码 4 色、关卡 `colors`/`gx`/`gy`/`TreasurePoint`/`image-top` 不解析 | 难度不生效、青蛙位置不对、没有宝石与隧道层 | ✅ 已修 |
| 主菜单只有 3 个文字按钮（开始/测试/退出） | 不能选关选难度、没有存档 | ✅ 已修（阶段 2：菜单/选关/难度/存档/结算） |
| ESC 直接退出整个程序 | 不能暂停 | ✅ 已修（暂停菜单） |

---

## 阶段 0 — 可验证性基础设施 ✅（已完成）

**为什么先做这个**：GUI 游戏在无头环境无法"手动试玩"，没有自动测试就无法证明任何改动是对的。

- `--autotest`：脚本输入（`HQC_Input_SetScripted`）驱动瞄准/开火/换球，不锁帧、帧上限、优雅退出
- 结构化事件日志 `[TEST] LEVEL_START / EXPLOSION / GAUGE_REACHED / LEVEL_COMPLETE /
  BALL_INTO_PIT / LIFE_LOST / GAME_OVER / TREASURE_COLLECTED ...` + 汇总 `AUTOTEST_REPORT`
- `--screenshot N PATH`：第 N 帧存 BMP；`[TEST] BALL_SAMPLE` 输出球坐标与颜色，
  可做**像素级校验**（画出来的球 = 模拟的球）
- 验收：三条路径跑通（通关 / 掉命 / Game Over），证据见阶段 1

---

## 阶段 1 — 核心玩法闭环 ✅（已完成）

- 球链物理（原版模型）：链尾驱动球加速到链速、其余跟随、同色缝隙磁吸回滚合拢≥3 爆、
  异色摩擦停住、"分离相位"保证身位间距
- 胜负：球进洞（最前球越过曲线末端，整链加速坠洞）= 掉命 → 重开本关；命尽 = Game Over；
  分数过 `gauge` → 停止生成 → 链清空 = 过关 → 下一关 → 13 大关通关
- 分数/命/宝石/连击记账 + HUD（分数、关卡号、命、分数槽进度条、宝石数）
- 宝石（`<TreasurePoint>`）出现/闪烁/消失 + 打中 +500
- 暂停（ESC）+ HUD 菜单按钮；隧道分层绘制（`image-top` + 曲线 t1/t2）
- 实测（无头、软件渲染）：
  - 通关：`--autotest --frames 30000 --levels 2` → 2 关全过、35 次爆炸、coins=2、退出码 0
  - 掉命：`--autotest --no-autoplay` → `BALL_INTO_PIT` → `LIFE_LOST`×2 → 自动重开
  - Game Over：`--lives 1 --no-autoplay` → `LIFE_LOST` → `GAME_OVER`
  - 渲染校验：8/8 球在模拟坐标处按正确颜色绘出

---

## 阶段 2 — 主菜单与进度（2.1-2.3 ✅ 已完成；2.4-2.5 待办）

### 2.1 主菜单重做 ✅
- `SceneMenu` 现在有 Adventure / Gauntlet（未实现，置灰）/ Options / Quit 四个大按钮，
  直接使用 `menu.png` 里注册的按钮精灵（含悬停态）+ 天空/太阳/标题背景
- 验收：`--autotest --start-menu` 下脚本点击 Adventure 能进选关（见 `MENU_ENTER_ADVENTURE`）

### 2.2 关卡选择界面 ✅
- 按大关列出关卡：**关卡预览图**（用关卡背景图，按格子缩放）+ 关卡号 + 关卡名/settings id，
  上一关/下一关/开打按钮
- ⚠️ 不要用 `content/images/thumbnails/thumb_1..18.jpg`：实测 18 张是**同一张 1280x720
  近全黑图**，1:1 画会盖满屏幕（旧实现的选关界面因此整片黑）
- 顺带修好 `StageProgression` 解析翻倍 bug（stage1 从 10 关 → 正确的 5 关）
- 验收：`MENU_ENTER_ADVENTURE levels=5`；选关界面全屏非零像素 921600/921600

### 2.3 难度选择（4 档）✅
- 移植 v0.1.0 `Game_Init` 的四档预设：`ballColors` 4/5/6/6、`partTime` 70/100/120/150、
  `ballStartCount` 40/50/60/60（难度 2 另加 `repeatChance=25`）
- 实现为 settings 副本上的覆盖（`GameDifficulty_Apply`），不污染 LevelMgr 注册表
- 验收：`--difficulty 2` → `LEVEL_START ... colors=6 start=60`

### 2.4 进度存档 ⬜（待办）
- 每关最高分/最高分用时 + 已解锁到第几关，落盘（建议 `~/.local/share/zumahd/progress.dat`，
  别写进 `content/` 以免污染仓库）
- 验收：跑完一关 → 重启进程 → 选关界面显示最高分非 0

### 2.5 结算界面 ⬜（待办）
- 过关/Game Over 用菜单对话框精灵（`SPR_MENU_DIALOG_BOX_RECT_*` 九宫格）显示：
  分数 / 宝石数 / 最大连击 / 最大链式连击 / 用时（`partTime` 内绿色）/ 历史最高
- 按钮：重试 / 下一关 / 返回菜单（现在只是屏幕上几行文字 + 自动跳转）
- 验收：`--autotest --levels 1` 结算数据与 `OBSERVED` 一致

---

## 阶段 3 — 玩法深度（3.1/3.2/3.3/3.4/3.6 ✅ 已完成；3.5 部分、3.7 待办）

### 3.1 道具球（power-ups）✅
- 素材说明：`content/images/gameobjects.png` 的球体条**只有纯色球**（蓝 47 帧、其余 50 帧，
  尾部 3 帧空白），没有道具图标帧 → 图标改用**绘图原语程序化画**（橙方块=炸弹、
  青横条=减速、白双竖条=暂停、浅黄十字=精准），音效用现成的
  （`SND_BOMBEXPLODE / SND_SLOWDOWN1 / SND_CHIME1 / SND_ACCURACY3`）
- 生成：每颗新球 7% 概率带道具（**开局铺满阶段也 roll**——只给常规阶段 roll 时实测一局 0 个）
- 生效：该球被炸掉时触发 —— 炸弹=炸掉曲线距离 180px 内的球（无视颜色，走
  `Statistics_RegisterExplosion` 记分）、减速=链速 ×0.35 持续 180 帧、暂停=链速 ×0
  持续 90 帧、精准=+1000 分；效果倍率由 `BallChain_GetSpeedMultiplier` 倒计时，
  SceneGame 每帧乘进链速
- 事件：`POWERUP_SPAWNED name=..` / `POWERUP_USED name=.. used=N` /
  `EFFECT_START|EFFECT_ACTIVE|EFFECT_END mul=.. speed=..`；结算界面显示 `Powerups used N`
- 实测：8000 帧一局 5~7 次道具生效（四种都出现过）；
  效果证据 `EFFECT_START mul=0.35 speed=0.175 base=0.50` → `EFFECT_END frames=180`（减速）、
  `mul=0.00 speed=0.000` → `EFFECT_END frames=90`（暂停）；
  截图里能定位到青色图标块（x 643~842 / y 375~674，即减速道具球所在链段）

### 3.2 关卡开场动画 ✅
- 开场：火花沿曲线跑一遍（`Level_GetCurveCoords` + `ANIM_SPARKLE`）+ 关卡名缩放淡入，
  期间球链静止；点击/空格可跳过；自动测试里缩短到 20 帧（保证帧预算）
- 事件：`INTRO_DONE frames=20`
- ⚠️ 原版的"过关收尾（沿曲线一路炸到洞）"在本实现里**不适用**：胜利条件是链已清空。
  球链坠洞的收尾由失败路径的"整链加速入洞"体现。

### 3.3 GAP BONUS ✅
- 子弹飞出屏幕 → 不当场销毁（`bullet->escaped`），交给 `BulletList_UpdateChainCollisions`
  按"整段飞行的最小贴近距离 + 那一刻的球链缝隙"结算（`Statistics_AddBulletGap`）
- 判据：`gap > 12px && 26 < 贴近距离 < 220px`（远离球链的飞行不算穿缝）
- 事件：`GAP_BONUS gap=.. dist=..` / `GAP_MISS`（未达标的也记，便于回归观察）
- 实测：8000 帧里 8 次 GAP_BONUS（gap 17~264px、dist 61~160px），
  全是"贴着球链钻过去"的合理样本；修正前只看"出屏那一帧距离"会给出假阳性

### 3.4 接近洞的紧张感 ✅
- `front_progress > 80%` → 音乐切 `MUS_NEAR_HOLE`，离开后切回 `MUS_GAME`
  （Gauntlet 局切回 `MUS_GAUNTLET`）；期间每 50 帧一次 `SND_WARNING1`
- 事件：`MUSIC near_hole=1 front=81%`

### 3.5 音乐/音效状态机 ✅
- 已接：菜单 `MUS_MAIN_MENU`、关卡 `MUS_GAME`、接近洞 `MUS_NEAR_HOLE`、通关 `MUS_WIN`、
  失败 `MUS_GAME_OVER`、Gauntlet `MUS_GAUNTLET`
- 音效：按钮悬停（`SND_BUTTON2`，进入时响一次）、按钮按下（`SND_BUTTON1`）、
  开火（`SND_FIREBALL1`）、爆炸（按连击变调 `PlaySoundPitch`）、宝石出现/消失/拾取、
  警告（接近洞，每 50 帧）、道具（炸弹/减速/暂停/精准各自的音）、GAP BONUS、胜负吟唱
- ⚠️ 无头环境（`ZUMA_NO_AUDIO`）听不到，只能做代码级核对 + 音效 id 是否注册/加载

### 3.6 Gauntlet 模式 ✅
- 主菜单 Gauntlet → 4 档难度（兔/鹰/豹/太阳神，用已注册的按钮精灵），
  或命令行 `--gauntlet 0..3`
- 规则：球流不停，分数打过"本目目标"→ 目数 +1、目标分 +1000+250×难度、
  球速 ×1.06、随机换一张地图继续；球进洞照常掉命，命尽 = Game Over
- HUD：显示 `wave N  target M`
- 事件：`GAUNTLET_START` / `GAUNTLET_WAVE wave=2 score=.. nextGauge=.. speedMul=..`
- ⚠️ 双曲线（`LevelGraphics.curveBFile` 已解析但渲染/物理仍只用 A）⬜ 待办

### 3.7 Cutout 图层 ✅（代码完成；素材缺失，补图即生效）
- `<Cutout image="left|right|tunnel.." pri=".." x=".." y=".."/>` 现在会解析（坐标与
  TreasurePoint 同一换算 `(x+104)*1.5, y*1.5`），关卡加载时载入贴图，按 pri 从小到大
  画在球链**之上**（`Level_DrawCutouts`，SceneGame 在球链/顶层贴图之后调用）
- ⚠️ **素材现状**：`content/levels/*/` 里没有 `left.png / right.png / tunnel*.png /
  serpentsT|M|B.png`（9 个关卡共声明 17 处 Cutout）→ 载入失败只记日志并跳过，不报错；
  补上对应 png 即生效（锚点目前按"贴图中心 = x,y"处理，与 TreasurePoint 一致）
- 验证（合成素材）：临时在 `build/bin/levels/underover/left.png` 放一张 200x200 半透明红 →
  `CUTOUT_LAYERS declared=2 loaded=1 missing=1`，截图里 Cutout 区域平均 RGB (201,32,18)、
  偏红像素 100%（全屏均值 (115,90,55)）→ 位置换算与绘制都正确（临时文件已删）

### 3.6 双曲线关卡 ✅（走法已实现）
- 解析：`<Graphics ... curve="serpents-1" curve2="serpents-2">` → curveAFile/curveBFile
- **走法**：全局球链坐标 `pos` 现在覆盖"合并曲线"——`0..lenA-1` 在 A 上、
  `lenA..lenA+lenB-1` 在 B 上（`Level_GetCurveLength` 返回 A+B，
  `Level_GetCurveCoords/_GetCurveFlags` 用 `_MapCurve` 做映射），
  球链物理/隧道标记/进洞判定都不用改：**进洞点自动落在曲线 B 的末端**，这就是
  Mirror Serpent 的"走完一条再拐到镜像路径"的手感
- 实测：serpents 两条曲线都加载（2459 + 2493 dots）；无头跑 4000 帧
  `front_progress_x1000=671`（67.1%）——单曲线时进度最多只能到 49.6%，
  证明球链确实走上了曲线 B
- ⬜ 美术：`serpentsT/M/B.png` 遮挡贴图缺失（见 3.7 说明）

---

## 阶段 4 — 工程化与收尾（4.1/4.2/4.4/4.5/4.6 ✅；4.3 部分）

- **4.1 内存 ✅**：ASan/LSan（`build-asan`，`-fsanitize=address,undefined`）跑两条自动测试路径
  **零泄漏零报错**。修复了两个真泄漏：`HQC_Container_FreeVector` 漏 free Vector 结构体本身
  （7790B/259 处）、ResourceStore 里 `HQC_StringConcat` 的路径串没释放（60 处）
- **4.2 CI ✅**：`.github/workflows/ci.yml` — 构建 + 5 条自动测试路径断言（通关/掉命/Game Over/
  菜单链路/Gauntlet）+ 截图产物上传。仓库没有 `.gitmodules`（`external/SDL_ttf` 是 gitlink），
  工作流里显式 clone `release-2.25.0`；`CMakeLists.txt` 也支持退回系统 SDL_ttf
  （`TTF_SetFontWrappedAlign` 已用 `SDL_TTF_VERSION_ATLEAST` 兜底，旧系统库也能编）
- **4.3 TODO.txt 遗留 🟡**：HQC 容器补齐（VECTOR 删除元素/链表/字典）、精灵映射外置+解析器、
  ResourceStore 并入 HQC 框架 —— 未做（都是框架级重构，不影响可玩性）
- **4.4 ECS 收尾 ✅**：`src/zuma/{ecs,systems,entities,components}`（14 个文件、540 行）
  实测**没有任何地方引用**（`World_Create`/`FrogSystem`/`SpriteDrawSystem`/`HudSystem` 全零引用），
  是 v2.0.0 重构留下的半成品 → 已删除（`gio trash` 可恢复，git 历史亦保留），
  并从 `CMakeLists.txt` 的显式源列表移除。现在只有一套实现：`scenes/` + `Frog/BallChain/Bullets`
- **4.5 窗口/缩放 ✅**：逻辑分辨率固定 1280×720 + `SDL_RenderSetLogicalSize`，
  非 16:9 窗口/屏幕由 SDL 自动加黑边（letterbox，不用自己算视口）；
  新增全屏切换 `F11` / `Alt+Enter`（`HQC_Window_ToggleFullscreen`）
- **4.6 打包 ✅**：`scripts/package_linux.sh` → `dist/zuma-deluxe-hd-<版本>-linux-x86_64.tar.gz`
  （含 bin + share/content + lib 里的 BASS 动态库 + 启动脚本 + RPATH `$ORIGIN/../lib`）

---

## 怎么跑（开发循环）

```bash
# 构建
cd ~/pj/Zuma-Deluxe-HD/build && cmake .. && make -j8

# 无头自动测试（五条路径）
cd bin
./ZumaHD --autotest --frames 30000 --levels 2 --stage 1 --level 1   # 通关路径（含结算对话框）
./ZumaHD --autotest --no-autoplay --frames 9000 --stage 1 --level 1 # 掉命路径（球进洞）
./ZumaHD --autotest --lives 1 --no-autoplay --stage 1 --level 1     # Game Over 路径
./ZumaHD --autotest --start-menu --frames 6000 --levels 1           # 菜单 → 选关 → 开打
./ZumaHD --autotest --gauntlet 0 --frames 9000 --levels 2           # Gauntlet 无限模式

# 画面取证（第 N 帧存 BMP / 结算对话框弹出那一帧存 BMP）
./ZumaHD --autotest --frames 640 --screenshot 620 /tmp/shot.bmp
./ZumaHD --autotest --frames 4000 --levels 1 --screenshot-result /tmp/result.bmp

# 人工试玩（有显示器时）
./ZumaHD                 # 主菜单；ESC 暂停；鼠标瞄准/左键发射/右键换球；F11 全屏

# 出包
scripts/package_linux.sh
```

> 无头运行依赖：`SDL_VIDEODRIVER=dummy`（`--autotest` 自动设置）+ 软件渲染回退 +
> `ZUMA_NO_AUDIO`（`--autotest` 自动设置）。真实机器上不需要任何环境变量。
> 存档：跑自动测试时写到 `/tmp/zumahd-autotest-progress.dat`（不碰玩家真实存档），
> 真实存档在 `~/.local/share/zumahd/progress.dat`；可用 `ZUMA_PROGRESS_FILE` 覆盖。