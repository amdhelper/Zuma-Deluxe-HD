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
| 主菜单只有 3 个文字按钮（开始/测试/退出） | 不能选关选难度、没有存档 | ⬜ 阶段 2 |
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

## 阶段 2 — 主菜单与进度（P0，下一批要做的）

### 2.1 主菜单重做（Adventure / Gauntlet / Options / Quit）
- 现状：`SceneMenu` 只有 3 个文字按钮；`menu.png` 里 **40+ 张按钮/背景精灵已经在 ResourceStore 注册**（`SPR_MENU_MAIN_BTN_*`），只差接线
- 目标：太阳/天空背景 + 4 个大按钮（悬停/按下三态）+ 音效；Gauntlet 与 Options 未实现前禁用置灰
- 涉及：`src/zuma/scenes/SceneMenu.c`、`src/zuma/Menu.c`（Button 已支持三态精灵，但 `Button_Draw` 的九宫格切图要用菜单按钮的矩形）
- 验收：`--autotest` 下用脚本点击"Adventure"能进选关界面

### 2.2 关卡选择界面
- 按大关（13 个）列出小关，缩略图 `thumb_1..18`（已加载）+ 关卡名 + 最高分/最高分时间
- 上一关/下一关/开始（三个按钮精灵已注册：`SPR_MENU_GAUNT_BTN_BACK/NEXT/PLAY`）
- 验收：选中第 N 关 → `--autotest` 断言 `LEVEL_START stage=x level=y` 与所选一致

### 2.3 难度选择（4 档）
- 原版实现（v0.1.0 `Game_Init`，可在 git 历史 `c53e82e2:src/gameplay/Game.c` 查到）：
  难度 0/1/2/3 → `ballColors` 4/5/6/6、`partTime` 70/100/120/150、`ballStartCount` 40/50/60/60
- 目标：难度覆盖 `LevelSettings` 的对应字段（新增 `LevelMgr` 的难度覆盖层，别直接改 settings 本体）
- 验收：`--difficulty 2` 时 `LEVEL_START` 事件的 `colors=6`

### 2.4 进度存档
- 每关最高分/最高分用时 + 已解锁到第几关，落盘（建议 `~/.local/share/zumahd/progress.dat`，
  纯文本/二进制均可；不要写进 content/ 以免污染仓库）
- 验收：跑完一关 → 重启进程 → 选关界面显示最高分非 0

### 2.5 结算界面
- 过关/Game Over 用菜单对话框精灵（`SPR_MENU_DIALOG_BOX_RECT_*` 九宫格）显示：
  分数 / 宝石数 / 最大连击 / 最大链式连击 / 用时（`partTime` 内绿色）/ 历史最高
- 按钮：重试 / 下一关 / 返回菜单
- 验收：`--autotest --levels 1` 后事件里出现结算数据且数值与 OBSERVED 一致

---

## 阶段 3 — 玩法深度（P1）

### 3.1 道具球（power-ups）
- `BallBonus`（accuracy / explosion / roll-back / pause）在 `BallChain.c` 里已有枚举，但**从未生成、从未生效**
- 生成：按 `repeat`/`single` 之外的独立概率生成带 bonus 的球（原版在 `BallChain_Append` 附近）
- 生效：球被爆掉时触发（倒退 = 链速反向；炸弹 = 炸掉周围一圈；暂停 = 冻结链 N 秒；accuracy = 分数加成）
- 验收：`--autotest` 事件里出现 `POWERUP_USED name=reverse` 且行为可见（链速/位置变化）

### 3.2 关卡开场/收尾动画
- 开场：火花沿曲线跑一遍 + 关卡名/`LEVEL x-y` 缩放淡入（原版 `Game_UpdateIntro`，参考 git 历史）
- 收尾（过关）：从最前球位置沿曲线一路炸到洞，每段 +100（原版 `Game_UpdateOutro`）
- 验收：`--autotest` 下开场帧数 >0 且收尾期间 `EXPLOSION`/分数递增事件可断言

### 3.3 GAP BONUS
- `Statistics_AddBulletGap` 已实现但**没有调用点**：子弹穿过球链缝隙飞出屏幕时按缝隙大小给分
- 触发点：`Bullets.c` 的飞出屏幕分支（先算子弹路径上最近的球间距再计分）
- 验收：连续两次穿缝 → `points` 里出现 GAP BONUS 加成

### 3.4 接近洞的紧张感
- 目前只有 `slowFactor` 减速（>80% 处）；补：`SND_WARNING1` 循环警告音 + 骷髅按进度张口（已实现）+ 背景音乐切 `MUS_NEAR_HOLE`
- 验收：`front_progress > 0.8` 时音乐 order 变化（`--autotest` 打点）

### 3.5 音乐/音效状态机
- 音乐已能按 order 切曲：菜单 `MUS_MAIN_MENU`、关卡 `MUS_GAME`、接近洞 `MUS_NEAR_HOLE`、
  胜利 `MUS_WIN`、失败 `MUS_GAME_OVER`（后三者已接；菜单未接）
- 音效：连击/链式连击变调已做（`HQC_DJ_PlaySoundPitch`）；补按钮悬停音、宝石音、结束音
- 验收：切场景时日志/主观听感（或 BASS 当前 order 打点）

### 3.6 Gauntlet 模式与双曲线关卡
- Gauntlet：无限生成 + 难度递增 + 独立的 4 个难度档（`SPR_MENU_GAUNT_BTN_RABBIT/EAGLE/JAGUAR/SUN_GOD` 已在）
- 双曲线：`LevelGraphics.curveBFile` 已解析但**没用**（`Level.c` 加载了 `curveB`，渲染/物理只用 A）
- 验收：Gauntlet 跑 5000 帧不崩且分数持续增长

### 3.7 Cutout 图层
- `levels.xml` 有 17 个 `<Cutout image="left|right|tunnel" ...>`（遮挡/隧道口），当前**不解析不绘制**
- 8 个关卡需要（underover/inversespiral/tunnellevel/overunder…），素材在 `content/levels/<id>/` 下找同名 png
- 验收：这几个关卡的球在"隧道段"被遮挡（截图 + 像素校验）

---

## 阶段 4 — 工程化与收尾（P2）

- **4.1 内存**：ASan/LSan 跑 `--autotest` 三路径全绿；球链每帧增删节点是泄漏高发区
- **4.2 CI**：GitHub Actions（ubuntu + SDL2/expat/BASS）→ 构建 + `--autotest` 三路径断言 + 截图产物
- **4.3 `TODO.txt` 遗留**：HQC 容器补齐（VECTOR 删除元素 / 链表 / 字典）、
  精灵映射外置成数据文件 + 解析器、ResourceStore 并入 HQC 框架
- **4.4 ECS 迁移收尾**：`src/zuma/ecs`、`entities/`、`systems/`（FrogSystem 等）是**半成品死代码**
  （`SceneGame` 创建了 `World` 但从不跑系统）；要么接进主循环要么删除，别留两套并行实现
- **4.5 窗口/缩放**：逻辑分辨率固定 1280×720，非 16:9 窗口会被拉伸；补 letterbox 与全屏切换
- **4.6 打包**：Linux（deb/AppImage）+ Windows(MinGW) 一键出包脚本（当前只有 `1.bat/2.bat` 与 `run_zuma.sh`）

---

## 怎么跑（开发循环）

```bash
# 构建
cd ~/pj/Zuma-Deluxe-HD/build && cmake .. && make -j8

# 无头自动测试（三路径）
cd bin
./ZumaHD --autotest --frames 30000 --levels 2      # 通关路径 + 宝石 + 连击
./ZumaHD --autotest --no-autoplay --frames 9000    # 掉命路径（球进洞）
./ZumaHD --autotest --lives 1 --no-autoplay        # Game Over 路径

# 画面取证（第 N 帧存 BMP）
./ZumaHD --autotest --frames 640 --screenshot 620 /tmp/shot.bmp

# 人工试玩（有显示器时）
./ZumaHD                 # 主菜单；ESC 暂停；鼠标瞄准/左键发射/右键换球
```

> 无头运行依赖：`SDL_VIDEODRIVER=dummy`（`--autotest` 自动设置）+ 软件渲染回退 +
> `ZUMA_NO_AUDIO`（`--autotest` 自动设置）。真实机器上不需要任何环境变量。