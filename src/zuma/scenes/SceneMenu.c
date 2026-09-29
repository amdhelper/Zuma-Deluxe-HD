#include "Index.h"

#include "../Menu.h"
#include "../ResourceStore.h"
#include "../LevelMgr.h"
#include "../GameOptions.h"
#include "../AutoTest.h"
#include "../Statistics.h"
#include "../Progress.h"

#include <stdlib.h>
#include <stdio.h>

// ═══════════════════════════════════════════════════════════════════════════════
// 主菜单 / 选关 / 设置（2026-09-29 重做）
//   旧版只有 3 个文字按钮（Start Game / Test Scene / Quit），menu.png 里注册的
//   40+ 张菜单精灵全部闲置：不能选关、不能选难度、没有进度概念。
//   现在：Adventure（选关：关卡预览 + 关卡名 + 难度档）→ Play 开打；
//   Options 调难度与音量；Gauntlet 未实现（置灰不响应，见 ROADMAP 3.6）。
//
// ⚠️ 关卡预览用"关卡背景图"而不是 content/images/thumbnails/thumb_N.jpg：
//   实测那 18 张缩略图是同一张 1280x720 的近乎全黑图（md5 全同、mean≈5/255），
//   既不是每关预览、1:1 画上去还会盖满整个屏幕（选关界面一片黑）。
// ═══════════════════════════════════════════════════════════════════════════════

#define private static

#define PANE_MAIN       0
#define PANE_ADVENTURE  1
#define PANE_OPTIONS    2
#define PANE_GAUNTLET   3

private struct {
    int pane;
    int hoverId;        // 当前悬停的按钮精灵 id（-1 = 无）——悬停音用（3.5）

    int stage;          // 0-based
    int selLevel;       // 0-based
    int difficulty;
    int volume;         // 0..10

    int frame;
    int clicked;        // 本帧是否已消费点击（防一次点击触发多个按钮）

    // 当前大关的关卡预览（加载好的关卡背景纹理，按大关缓存）
    HQC_VECTOR(HQC_Texture) previews;
    int previewStage;
} menu;


////////////////////////////////////////////////////////////////////////////////
// 通用：图片按钮（直接用 menu.png 的整张按钮精灵 + 悬停态）
////////////////////////////////////////////////////////////////////////////////

private bool _MouseInRect(int x, int y, int w, int h) {
    v2i_t m = HQC_Input_MouseGetPosition();

    return m.x >= x - w / 2 && m.x <= x + w / 2 &&
           m.y >= y - h / 2 && m.y <= y + h / 2;
}

private bool _ImageButton(int sprId, int sprHoverId, int x, int y, float scale) {
    HQC_Sprite spr = Store_GetSpriteByID(sprId);
    if (!spr) return false;

    irect_t rect = HQC_Sprite_GetRect(spr);

    bool inside = _MouseInRect(x, y, (int)(rect.width * scale), (int)(rect.height * scale));

    int drawId = (inside && sprHoverId >= 0) ? sprHoverId : sprId;

    HQC_Artist_DrawSetScale(scale);
    HQC_Artist_DrawSprite(Store_GetSpriteByID(drawId), (float)x, (float)y);
    HQC_Artist_DrawSetScale(1);

    if (inside && !menu.clicked && HQC_Input_MouseLeftPressed()) {
        menu.clicked = 1;
        HQC_DJ_PlaySound(Store_GetSoundByID(SND_BUTTON1));
        return true;
    }

    // 悬停音（3.5）：鼠标"刚进入"这个按钮时响一次（不是每帧都响）
    if (inside && menu.hoverId != sprId) {
        menu.hoverId = sprId;
        HQC_DJ_PlaySound(Store_GetSoundByID(SND_BUTTON2));
    } else if (!inside && menu.hoverId == sprId) {
        menu.hoverId = -1;
    }

    return false;
}


private void _SetPane(int pane) {
    menu.pane = pane;
    menu.clicked = 1;   // 切换面板当帧不再响应点击
}


////////////////////////////////////////////////////////////////////////////////
// 关卡预览纹理（按大关缓存）
////////////////////////////////////////////////////////////////////////////////

private void _FreePreviews() {
    if (!menu.previews) return;

    size_t n = HQC_Container_VectorCount(menu.previews);
    for (size_t i = 0; i < n; i++) {
        HQC_Texture* t = HQC_Container_VectorGet(menu.previews, i);
        if (*t) HQC_Artist_FreeTexture(*t);
    }

    HQC_Container_FreeVector(menu.previews);
    menu.previews = NULL;
    menu.previewStage = -1;
}


private void _EnsurePreviews(int stage) {
    if (menu.previews && menu.previewStage == stage)
        return;

    _FreePreviews();

    menu.previews = HQC_Container_CreateVector(sizeof(HQC_Texture));
    menu.previewStage = stage;

    int count = LevelMgr_GetLevelCount(stage);

    for (int i = 0; i < count; i++) {
        LevelGraphics* gx = LevelMgr_GetLevelGraphics(stage, i);
        HQC_Texture tex = NULL;

        // 预览 = 关卡背景图（content/levels/<id>/<id>.jpg）
        if (gx && gx->textureFile)
            tex = HQC_Artist_LoadTexture(gx->textureFile);

        HQC_Container_VectorAdd(menu.previews, &tex);
    }

    HQC_Log("SceneMenu: previews for stage %d loaded (%d levels)", stage + 1, count);
}


// 按格子尺寸缩放整张背景图（原图 1280x720，1:1 画会盖满整个屏幕）
private void _DrawPreview(HQC_Texture tex, int cx, int cy, int cellW, int cellH, bool highlight) {
    if (!tex) {
        HQC_Artist_SetColorHex(highlight ? C_YELLOW : 0x404040);
        HQC_Artist_FillRect(cx - cellW / 2, cy - cellH / 2, cellW, cellH);
        return;
    }

    int tw = 0, th = 0;
    HQC_Artist_GetTextureSize(tex, &tw, &th);
    if (tw <= 0 || th <= 0) return;

    float scale = (float)cellW / (float)tw;
    if ((float)cellH / (float)th < scale)
        scale = (float)cellH / (float)th;

    HQC_Artist_DrawSetScale(highlight ? scale * 1.06f : scale);
    HQC_Artist_DrawTexture(tex, (float)cx, (float)cy);
    HQC_Artist_DrawSetScale(1);
}


////////////////////////////////////////////////////////////////////////////////
// 主菜单
////////////////////////////////////////////////////////////////////////////////

private void _StartAdventure() {
    LevelMgr_ClampProgress(&menu.stage, &menu.selLevel);
    _SetPane(PANE_ADVENTURE);

    // 让自动测试能断言"菜单读到的星级与存档一致"
    AutoTest_Event("MENU_STARS", "stage=%d stars=%d,%d,%d,%d,%d sel=%d",
                   menu.stage,
                   Progress_GetStars(menu.stage, 0), Progress_GetStars(menu.stage, 1),
                   Progress_GetStars(menu.stage, 2), Progress_GetStars(menu.stage, 3),
                   Progress_GetStars(menu.stage, 4), menu.selLevel);
}

private void _StartGauntletPane() {
    _SetPane(PANE_GAUNTLET);

    // 让自动测试能断言"排行榜面板被打开、榜上有几条"
    AutoTest_Event("GAUNTLET_BOARD_VIEW", "entries=%d best=%d",
                   Progress_GauntletCount(), Progress_GauntletBest());
}

private void _StartGauntletMode(int difficulty) {
    // Gauntlet：无限球流 + 每周目提速（ROADMAP 3.6）
    GameOptions_StartGauntlet(difficulty);

    Statistics_Init();
    Statistics_SetLives(gGameOptions.lives);

    // 从当前选中的关卡开始（地图随机切换发生在每一目结束时）
    LevelMgr_ClampProgress(&menu.stage, &menu.selLevel);
    LevelMgr_SetProgress(menu.stage, menu.selLevel);

    AutoTest_Event("MENU_GAUNTLET", "difficulty=%d stage=%d level=%d",
                   difficulty, menu.stage + 1, menu.selLevel + 1);

    Scene_Change(SC_GAME);
}

private void _StartGauntlet() { _StartGauntletPane(); }

private void _OpenOptions() { _SetPane(PANE_OPTIONS); }

private void _QuitGame() { exit(0); }


private void _DrawMainPane() {
    // 背景（天空 + 主屏 + 太阳 + 标题）
    HQC_Sprite sky = Store_GetSpriteByID(SPR_MENU_SCREEN_MAIN_SKY);
    if (sky) HQC_Artist_DrawSprite(sky, 640, 600);

    HQC_Sprite screen = Store_GetSpriteByID(SPR_MENU_SCREEN_MAIN);
    if (screen) HQC_Artist_DrawSprite(screen, 640, 360);

    HQC_Sprite sunLight = Store_GetSpriteByID(SPR_MENU_MAIN_SUN_LIGHT);
    if (sunLight) {
        HQC_Artist_DrawSetAlpha(0.2f);
        HQC_Artist_DrawSprite(sunLight, 320, 200);
        HQC_Artist_DrawSetAlpha(1.0f);
    }

    HQC_Sprite sun = Store_GetSpriteByID(SPR_MENU_MAIN_SUN);
    if (sun) HQC_Artist_DrawSprite(sun, 320, 200);

    HQC_Sprite title = Store_GetSpriteByID(SPR_MENU_HEAD);
    if (title) HQC_Artist_DrawSprite(title, 640, 150);

    // 四个大按钮（Adventure / Gauntlet / Options / Quit）
    if (_ImageButton(SPR_MENU_MAIN_BTN_ADVENTURE, SPR_MENU_MAIN_BTN_ADVENTURE_HOVER, 640, 320, 1.0f))
        _StartAdventure();

    if (_ImageButton(SPR_MENU_MAIN_BTN_GAUNTLET, SPR_MENU_MAIN_BTN_GAUNTLET_HOVER, 400, 480, 1.0f))
        _StartGauntlet();

    if (_ImageButton(SPR_MENU_MAIN_BTN_OPTIONS, SPR_MENU_MAIN_BTN_OPTIONS_HOVER, 880, 480, 1.0f))
        _OpenOptions();

    if (_ImageButton(SPR_MENU_MAIN_BTN_QUIT, SPR_MENU_MAIN_BTN_QUIT_HOVER, 640, 620, 0.8f))
        _QuitGame();

    HQC_Artist_SetColorHex(C_WHITE);
    HQC_Artist_DrawText(Store_GetFontByID(FONT_CANCUN_8),
                        "Zuma Deluxe HD remake — WIP (see docs/ROADMAP.md)", 640, 692);
}


////////////////////////////////////////////////////////////////////////////////
// 选关
////////////////////////////////////////////////////////////////////////////////

private void _DrawAdventurePane() {
    HQC_Sprite screen = Store_GetSpriteByID(SPR_MENU_SCREEN_GAUNTLET);
    if (screen) HQC_Artist_DrawSprite(screen, 640, 360);

    char buff[128];

    _EnsurePreviews(menu.stage);

    int stageCount = LevelMgr_GetStageCount();
    int levelCount = LevelMgr_GetLevelCount(menu.stage);

    snprintf(buff, sizeof(buff), "Stage %d / %d", menu.stage + 1, stageCount);
    HQC_Artist_SetColorHex(C_WHITE);
    HQC_Artist_DrawText(Store_GetFontByID(FONT_CANCUN_13), buff, 640, 56);

    // 关卡格子（每行 4 个，预览按格子缩放）
    const int cellW = 250, cellH = 141;
    const int gapX = 26, gapY = 48;
    const int cols = 4;
    const int rows = (levelCount + cols - 1) / cols;
    const int gridW = cols * cellW + (cols - 1) * gapX;
    const int gridX0 = 640 - gridW / 2 + cellW / 2;
    const int gridY0 = 240;

    if (rows == 1 || rows == 2)
        (void)gridY0;   // 行数少时保持顶部对齐

    for (int i = 0; i < levelCount; i++) {
        int col = i % cols;
        int row = i / cols;

        int x = gridX0 + col * (cellW + gapX);
        int y = gridY0 + row * (cellH + gapY);

        bool unlocked = Progress_IsUnlocked(menu.stage, i);
        bool selected = (i == menu.selLevel) && unlocked;
        bool hovered  = unlocked && _MouseInRect(x, y, cellW, cellH);

        HQC_Texture tex = NULL;
        if (menu.previews && (size_t)i < HQC_Container_VectorCount(menu.previews))
            tex = *(HQC_Texture*)HQC_Container_VectorGet(menu.previews, i);

        if (!unlocked) {
            // 未解锁：压暗预览 + LOCKED 标签，且不可点
            HQC_Artist_DrawSetAlpha(0.30f);
            _DrawPreview(tex, x, y, cellW, cellH, false);
            HQC_Artist_DrawSetAlpha(1.0f);

            HQC_Artist_SetColorHex(0x808080);
            HQC_Artist_DrawText(Store_GetFontByID(FONT_CANCUN_10), "LOCKED", (float)x, (float)y);
            continue;
        }

        _DrawPreview(tex, x, y, cellW, cellH, selected || hovered);

        // 关卡号
        snprintf(buff, sizeof(buff), "%d-%d", menu.stage + 1, i + 1);
        HQC_Artist_SetColorHex((selected || hovered) ? C_YELLOW : C_WHITE);
        HQC_Artist_DrawText(Store_GetFontByID(FONT_CANCUN_10), buff,
                            (float)x, (float)(y + cellH / 2 + 20));

        // 该关成绩（最高分 / 最佳用时）
        int best = Progress_GetBestScore(menu.stage, i);
        int bestSecs = Progress_GetBestSeconds(menu.stage, i);

        if (best > 0) {
            if (bestSecs > 0)
                snprintf(buff, sizeof(buff), "best %d  %d:%02d", best, bestSecs / 60, bestSecs % 60);
            else
                snprintf(buff, sizeof(buff), "best %d", best);

            HQC_Artist_SetColorHex(0xB0FFB0);
            HQC_Artist_DrawText(Store_GetFontByID(FONT_CANCUN_8), buff,
                                (float)x, (float)(y + cellH / 2 + 38));
        }

        // 星级（ROADMAP 3.10）：格子正上方三颗方块，拿到=金色，没拿到=暗灰
        // ⚠️ 必须显式把 alpha 拉回 1.0：预览/未解锁格子的绘制会留下 0.x 的 alpha，
        //    否则这三颗方块会"画了但看不见"（FillRect 用的是当前 alpha）
        HQC_Artist_DrawSetAlpha(1.0f);

        int stars = Progress_GetStars(menu.stage, i);

        // 用文字星（*）而不是色块：文字走字体渲染路径，最稳
        {
            char starBuf[8];
            int  k = 0;

            for (; k < stars && k < 3; k++)
                starBuf[k] = '*';

            starBuf[k] = 0;

            HQC_Artist_SetColorHex(stars > 0 ? 0xFFD24A : 0x9A9A9A);
            HQC_Artist_DrawText(Store_GetFontByID(FONT_CANCUN_10),
                                stars > 0 ? starBuf : ". . .",
                                (float)x, (float)(y - cellH / 2 - 16));
            HQC_Artist_SetColorHex(C_WHITE);
        }

        for (int s = 0; s < 3; s++) {
            float sx = (float)(x - 26 + s * 26);
            float sy = (float)(y - cellH / 2 - 16);

            HQC_Artist_SetColorHex(s < stars ? 0xFFD24A : 0x4A4A4A);
            HQC_Artist_FillRect(sx - 9, sy - 9, 18, 18);
        }

        HQC_Artist_SetColorHex(C_WHITE);

        if (hovered && !menu.clicked && HQC_Input_MouseLeftPressed()) {
            menu.clicked = 1;
            menu.selLevel = i;
            HQC_DJ_PlaySound(Store_GetSoundByID(SND_BUTTON1));
        }
    }

    // 选中关卡名 + 设置 id
    LevelGraphics* gx = LevelMgr_GetLevelGraphics(menu.stage, menu.selLevel);
    LevelSettings* st = LevelMgr_GetLevelSettings(menu.stage, menu.selLevel);

    if (gx && st) {
        snprintf(buff, sizeof(buff), "%s   (%s)", gx->dispName ? gx->dispName : gx->id,
                 st->id ? st->id : "?");
        HQC_Artist_SetColorHex(C_WHITE);
        HQC_Artist_DrawText(Store_GetFontByID(FONT_CANCUN_12), buff, 640, 500);
    }

    // 难度（点击循环切换 4 档）
    snprintf(buff, sizeof(buff), "Difficulty:  < %s >", GameDifficulty_Name(menu.difficulty));
    bool diffHover = _MouseInRect(640, 545, 460, 44);
    HQC_Artist_SetColorHex(diffHover ? C_YELLOW : C_WHITE);
    HQC_Artist_DrawText(Store_GetFontByID(FONT_CANCUN_12), buff, 640, 545);

    if (diffHover && !menu.clicked && HQC_Input_MouseLeftPressed()) {
        menu.clicked = 1;
        menu.difficulty = (menu.difficulty + 1) % 4;
        gGameOptions.difficulty = menu.difficulty;
        HQC_DJ_PlaySound(Store_GetSoundByID(SND_BUTTON1));
    }

    // 上一关 / 下一关 / 开打
    if (_ImageButton(SPR_MENU_GAUNT_BTN_BACK, SPR_MENU_GAUNT_BTN_BACK_HOVER, 300, 635, 1.0f)) {
        if (menu.stage > 0) { menu.stage--; menu.selLevel = 0; _EnsurePreviews(menu.stage); }
        else                { _SetPane(PANE_MAIN); }
    }

    if (_ImageButton(SPR_MENU_GAUNT_BTN_NEXT, SPR_MENU_GAUNT_BTN_NEXT_HOVER, 980, 635, 1.0f)) {
        if (menu.stage + 1 < LevelMgr_GetStageCount()) {
            menu.stage++;
            menu.selLevel = 0;
            _EnsurePreviews(menu.stage);
        }
    }

    if (_ImageButton(SPR_MENU_GAUNT_BTN_PLAY, SPR_MENU_GAUNT_BTN_PLAY_HOVER, 640, 635, 1.2f)) {
        gGameOptions.difficulty = menu.difficulty;
        gGameOptions.gauntlet   = 0;    // Adventure 模式（清掉可能残留的 Gauntlet 标记）

        LevelMgr_SetProgress(menu.stage, menu.selLevel);

        // 从菜单开打 = 新的一局：分数/命/连击全部重置（否则上一局的命不会补回来）
        Statistics_Init();
        Statistics_SetLives(gGameOptions.lives);

        Progress_SetCurrent(menu.stage, menu.selLevel);

        AutoTest_Event("MENU_PLAY", "stage=%d level=%d difficulty=%d",
                       menu.stage + 1, menu.selLevel + 1, menu.difficulty);

        Scene_Change(SC_GAME);
    }

    HQC_Artist_SetColorHex(C_WHITE);
}


////////////////////////////////////////////////////////////////////////////////
// Gauntlet：选难度（兔 / 鹰 / 豹 / 太阳神）→ 无限模式
////////////////////////////////////////////////////////////////////////////////

static const struct {
    int  spr, sprHover;
    const char* name;
    const char* desc;
} kGauntletModes[4] = {
    { SPR_MENU_GAUNT_BTN_RABBIT,  SPR_MENU_GAUNT_BTN_RABBIT_HOVER,  "Rabbit",   "slower balls, 4 colors" },
    { SPR_MENU_GAUNT_BTN_EAGLE,   SPR_MENU_GAUNT_BTN_EAGLE_HOVER,   "Eagle",    "5 colors" },
    { SPR_MENU_GAUNT_BTN_JAGUAR,  SPR_MENU_GAUNT_BTN_JAGUAR_HOVER,  "Jaguar",   "6 colors, faster" },
    { SPR_MENU_GAUNT_BTN_SUN_GOD, SPR_MENU_GAUNT_BTN_SUN_GOD_HOVER, "Sun God",  "6 colors, fastest" },
};


private void _DrawGauntletPane() {
    HQC_Sprite screen = Store_GetSpriteByID(SPR_MENU_SCREEN_GAUNTLET);
    if (screen) HQC_Artist_DrawSprite(screen, 640, 360);

    char buff[160];

    HQC_Artist_SetColorHex(C_WHITE);
    HQC_Artist_DrawText(Store_GetFontByID(FONT_CANCUN_13), "GAUNTLET", 640, 60);
    HQC_Artist_DrawText(Store_GetFontByID(FONT_CANCUN_10),
                        "endless balls, each wave is faster — choose your difficulty", 640, 96);

    const int xs[2] = { 470, 810 };
    const int ys[2] = { 250, 430 };

    for (int i = 0; i < 4; i++) {
        int x = xs[i % 2];
        int y = ys[i / 2];

        if (_ImageButton(kGauntletModes[i].spr, kGauntletModes[i].sprHover, x, y, 1.0f))
            _StartGauntletMode(i);

        HQC_Artist_SetColorHex(C_YELLOW);
        HQC_Artist_DrawText(Store_GetFontByID(FONT_CANCUN_12), kGauntletModes[i].name, (float)x, (float)(y + 70));

        HQC_Artist_SetColorHex(C_WHITE);
        HQC_Artist_DrawText(Store_GetFontByID(FONT_CANCUN_8), kGauntletModes[i].desc, (float)x, (float)(y + 92));
    }

    // ── 排行榜（ROADMAP 3.11）：前 5 名，分数降序（右侧一列）──────────────
    HQC_Artist_SetColorHex(C_YELLOW);
    HQC_Artist_DrawText(Store_GetFontByID(FONT_CANCUN_10), "BEST 5", 1140, 200);

    int boardN = Progress_GauntletCount();

    if (boardN == 0) {
        HQC_Artist_SetColorHex(0x909090);
        HQC_Artist_DrawText(Store_GetFontByID(FONT_CANCUN_8), "no runs yet", 1140, 240);
    } else {
        for (int i = 0; i < boardN; i++) {
            const ProgressGauntletEntry* e = Progress_GauntletEntryAt(i);
            if (!e) continue;

            snprintf(buff, sizeof(buff), "%d. %d  W%d  %s", i + 1, e->score, e->wave,
                     GameDifficulty_Name(e->difficulty));

            HQC_Artist_SetColorHex(i == 0 ? C_YELLOW : C_WHITE);
            HQC_Artist_DrawText(Store_GetFontByID(FONT_CANCUN_8), buff, 1140,
                                (float)(240 + i * 42));
        }
    }

    if (_ImageButton(SPR_MENU_GAUNT_BTN_BACK, SPR_MENU_GAUNT_BTN_BACK_HOVER, 640, 600, 1.2f))
        _SetPane(PANE_MAIN);

    HQC_Artist_SetColorHex(C_WHITE);
}


////////////////////////////////////////////////////////////////////////////////
// 设置
////////////////////////////////////////////////////////////////////////////////

private void _DrawOptionsPane() {
    HQC_Sprite screen = Store_GetSpriteByID(SPR_MENU_SCREEN_GAUNTLET);
    if (screen) HQC_Artist_DrawSprite(screen, 640, 360);

    char buff[128];

    HQC_Artist_SetColorHex(C_WHITE);
    HQC_Artist_DrawText(Store_GetFontByID(FONT_CANCUN_13), "Options", 640, 90);

    snprintf(buff, sizeof(buff), "Difficulty:  < %s >", GameDifficulty_Name(menu.difficulty));
    bool diffHover = _MouseInRect(640, 260, 460, 44);
    HQC_Artist_SetColorHex(diffHover ? C_YELLOW : C_WHITE);
    HQC_Artist_DrawText(Store_GetFontByID(FONT_CANCUN_12), buff, 640, 260);
    if (diffHover && !menu.clicked && HQC_Input_MouseLeftPressed()) {
        menu.clicked = 1;
        menu.difficulty = (menu.difficulty + 1) % 4;
        gGameOptions.difficulty = menu.difficulty;
    }

    snprintf(buff, sizeof(buff), "Master volume:  < %d%% >", menu.volume * 10);
    bool volHover = _MouseInRect(640, 340, 460, 44);
    HQC_Artist_SetColorHex(volHover ? C_YELLOW : C_WHITE);
    HQC_Artist_DrawText(Store_GetFontByID(FONT_CANCUN_12), buff, 640, 340);
    if (volHover && !menu.clicked && HQC_Input_MouseLeftPressed()) {
        menu.clicked = 1;
        menu.volume = (menu.volume + 1) % 11;
        HQC_DJ_SetMasterVolume(menu.volume / 10.0f);
        HQC_DJ_PlaySound(Store_GetSoundByID(SND_BUTTON1));
    }

    HQC_Artist_SetColorHex(C_WHITE);
    HQC_Artist_DrawText(Store_GetFontByID(FONT_CANCUN_8),
                        "Gauntlet / More Games: not implemented yet", 640, 420);

    if (_ImageButton(SPR_MENU_GAUNT_BTN_BACK, SPR_MENU_GAUNT_BTN_BACK_HOVER, 640, 560, 1.2f))
        _SetPane(PANE_MAIN);
}


////////////////////////////////////////////////////////////////////////////////
// 自动测试：脚本化点击（菜单链路取证）
////////////////////////////////////////////////////////////////////////////////

private void _MenuAutoplay() {
    if (!AutoTest_IsActive() || !gGameOptions.startAtMenu)
        return;

    int f = menu.frame;

    // ── Gauntlet 链路（--start-menu --gauntlet N）：点 Gauntlet → 点难度 ────
    if (gGameOptions.gauntlet) {
        if (f == 30) AutoTest_SetPointer(400, 480, 0, 0);       // 主菜单 Gauntlet 按钮
        if (f == 32) AutoTest_SetPointer(400, 480, 1, 0);
        if (f == 34) AutoTest_SetPointer(400, 480, 0, 0);

        if (f == 40) AutoTest_Event("MENU_ENTER_GAUNTLET", "pane=%d (3=gauntlet)", menu.pane);

        // 按难度下标点对应按钮：0/1 在第一行，2/3 在第二行
        int idx = gGameOptions.gauntletDifficulty;
        int bx = (idx % 2 == 0) ? 470 : 810;
        int by = (idx / 2 == 0) ? 250 : 430;

        if (f == 60) AutoTest_SetPointer(bx, by, 0, 0);
        if (f == 62) AutoTest_SetPointer(bx, by, 1, 0);
        if (f == 64) AutoTest_SetPointer(bx, by, 0, 0);
        return;
    }

    // 30: 悬停 Adventure，32: 点击 → 进选关；50: 悬停 Play，52: 点击 → 开打
    if (f == 30) { AutoTest_SetPointer(640, 320, 0, 0); AutoTest_Event("MENU_HOVER_ADVENTURE", ""); }
    if (f == 32) { AutoTest_SetPointer(640, 320, 1, 0); }
    if (f == 34) { AutoTest_SetPointer(640, 320, 0, 0); }

    if (f == 36) { AutoTest_Event("MENU_ENTER_ADVENTURE", "pane=%d levels=%d",
                                  menu.pane, LevelMgr_GetLevelCount(menu.stage)); }

    if (f == 50) { AutoTest_SetPointer(640, 635, 0, 0); AutoTest_Event("MENU_HOVER_PLAY", ""); }
    if (f == 52) { AutoTest_SetPointer(640, 635, 1, 0); }
    if (f == 54) { AutoTest_SetPointer(640, 635, 0, 0); }
}


////////////////////////////////////////////////////////////////////////////////
// 场景回调
////////////////////////////////////////////////////////////////////////////////

private void _Update() {
    menu.frame++;
    menu.clicked = 0;

    _MenuAutoplay();

    // ESC：主菜单退出，其它面板返回主菜单
    if (HQC_Input_KeyPressed(HQC_KEY_ESCAPE)) {
        if (menu.pane == PANE_MAIN) exit(0);
        _SetPane(PANE_MAIN);
    }
}


private void _Draw() {
    switch (menu.pane) {
        case PANE_ADVENTURE: _DrawAdventurePane(); break;
        case PANE_GAUNTLET:  _DrawGauntletPane();  break;
        case PANE_OPTIONS:   _DrawOptionsPane();   break;
        default:             _DrawMainPane();      break;
    }
}


private void _Load() {
    HQC_Log("SceneMenu: loading");

    menu.pane         = PANE_MAIN;
    menu.hoverId      = -1;
    menu.stage        = Progress_GetCurrentStage();
    menu.selLevel     = Progress_GetCurrentLevel();
    menu.difficulty   = gGameOptions.difficulty;
    menu.volume       = 10;
    menu.frame        = 0;
    menu.clicked      = 0;
    menu.previews     = NULL;
    menu.previewStage = -1;

    LevelMgr_ClampProgress(&menu.stage, &menu.selLevel);

    HQC_DJ_SetMasterVolume(menu.volume / 10.0f);

    Store_PlayMusic(MUS_MAIN_MENU);
}


private void _Free() {
    _FreePreviews();

    AutoTest_Event("MENU_UNLOAD", "pane=%d", menu.pane);
}


HScene Scene_Register_Menu() {
    return Scene_New("menu", _Load, _Update, _Draw, _Free);
}