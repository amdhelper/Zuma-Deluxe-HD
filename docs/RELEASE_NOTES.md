# Zuma Deluxe HD @VERSION@

开源复刻版祖玛（Zuma Deluxe HD），C + SDL2，跨平台（Linux 已出包，Windows 可自行用 MinGW 构建）。

## 下载哪个？

| 文件 | 适合谁 | 怎么用 |
|---|---|---|
| `zuma-deluxe-hd-@VERSION@-linux-x86_64.tar.gz` | 任何 Linux | 解包 → `./zuma.sh`（免安装，BASS/SDL_ttf 已随包） |
| `zuma-deluxe-hd_@VERSION@_amd64.deb` | Debian/Ubuntu | `sudo apt install ./zuma-deluxe-hd_@VERSION@_amd64.deb` → 应用菜单里出现「祖玛高清重制版」 |
| `sha256sums.txt` | 想验完整性 | `sha256sum -c sha256sums.txt` |

依赖（发行版包即可）：`libsdl2` `libsdl2-image` `libexpat1`。

## 安装后想要桌面快捷方式

```bash
./scripts/install_desktop.sh     # 装到 ~/.local/share/zuma-deluxe-hd 并生成桌面/菜单快捷方式
```

## 玩法

- 鼠标瞄准，**左键**发射，**右键**换球，**ESC** 暂停，**F11 / Alt+Enter** 全屏
- 打满分数槽 + 清空球链 = 过关；球进洞扣命，命尽 = Game Over
- 13 大关 / 4 档难度 / Gauntlet 无限模式（每周目提速）
- 道具球：炸弹（炸一片）、减速、暂停、精准奖励
- 连击 / 链式连击 / GAP BONUS / 宝石收集，结算界面显示最高分与最佳用时
- 进度自动存档（`~/.local/share/zumahd/progress.dat`）

## 这一版做了什么

- 完整可玩闭环：主菜单 / 选关（预览 + 最高分 + 未解锁锁定）/ 难度 / 结算对话框 / 存档
- 玩法深度：道具球、GAP BONUS、开场动画、接近洞紧张感、Gauntlet
- 工程：无头自动测试 7 条路径（`--autotest`）、ASan/LSan 零泄漏、CI、出包脚本
- 修复：球链回绕导致无法过关、输入边沿丢失导致点击无反应、选关界面全黑、
  LevelMgr 1-based/0-based 混用、vector 释放泄漏等

## 开发者

```bash
cmake -S . -B build && cmake --build build -j          # 构建
cd build/bin && ./ZumaHD --autotest --frames 30000 --levels 2   # 无头自动测试
scripts/package_linux.sh                                # 出包（tar.gz + deb + 校验和）
```

详细任务清单与验收标准见仓库内 `docs/ROADMAP.md`。