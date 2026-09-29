#include "Menu.h"

#include "ResourceStore.h"

enum _ButtonState {
    BTN_IDLE,
    BTN_HOVERED,
    BTN_PRESSED
};

typedef struct Button {
    const char* text;

    int scale;

    v2i_t size;
    v2f_t pos;

    int state;

    HQC_Sound sndHover;
    HQC_Sound sndPressed;

    HQC_Sprite spr;
    HQC_Sprite sprHover;
    HQC_Sprite sprPressed;

    void (*onClick)(void);
} Button;


typedef struct Checkbox {
    v2f_t pos;
    int   checked;

    void (*onChecked)(void);
    void (*onUnchecked)(void);
} Checkbox;


typedef struct Slider {
    v2f_t pos;
    float value;          // 0..1
    int   width;          // 轨道宽度（由 SPR_MENU_SLIDER 精灵决定）
    int   dragging;

    void (*onChange)(float);
} Slider;


typedef struct DialogText {
    const char* text;
    float    x;           // 相对对话框中心
    float    y;
    uint32_t color;
} DialogText;


typedef struct Dialogbox {
    v2f_t pos;
    int   width;
    int   height;         // 由内容自动计算

    const char* title;

    HQC_VECTOR(HButton)   buttons;
    HQC_VECTOR(HSlider)   sliders;
    HQC_VECTOR(HCheckbox) checkboxes;
    HQC_VECTOR(DialogText) texts;
} Dialogbox;


static void _Button_UpdateRect(HButton hbutton) {
    Button* btn = (Button*)hbutton;

    if (!btn->spr) {
        // Set default size if sprite is invalid
        btn->size.x = 180;
        btn->size.y = 72;
        HQC_Log("Button_UpdateRect: Invalid sprite, using default size");
        return;
    }

    irect_t rect = HQC_Sprite_GetRect(btn->spr);
    btn->size.x = rect.width;
    btn->size.y = rect.height;
}

static Button* _Btn(HButton hbutton) { return (Button*)hbutton; }

HButton Button_Create(float x, float y) {
    Button* btn = HQC_Memory_Allocate(sizeof(*btn));
    if (!btn) {
        HQC_Log("Button_Create: Failed to allocate memory");
        return NULL;
    }

    btn->pos.x      = x;
    btn->pos.y      = y;

    btn->text       = "button";

    btn->state      = BTN_IDLE;

    btn->sndHover   = NULL;
    btn->sndPressed = Store_GetSoundByID(SND_BUTTON1);

    HQC_Log("Button_Create: Loading sprites...");
    btn->spr        = Store_GetSpriteByID(SPR_MENU_BUTTON);
    btn->sprHover   = Store_GetSpriteByID(SPR_MENU_BUTTON_HOVER);
    btn->sprPressed = Store_GetSpriteByID(SPR_MENU_BUTTON_PRESSED);

    if (!btn->spr) {
        HQC_Log("Button_Create: Failed to load main sprite (ID: %d)", SPR_MENU_BUTTON);
    }

    btn->onClick    = NULL;

    btn->scale      = 1;

    _Button_UpdateRect(btn);

    HQC_Log("Button_Create: Button created at (%.1f, %.1f) with size (%d, %d)", 
            x, y, btn->size.x, btn->size.y);

    return btn;
}


void Button_SetSprite(HButton hbutton, HQC_Sprite sprite) {
    _Btn(hbutton)->spr = sprite;
    _Button_UpdateRect(_Btn(hbutton));
}


void Button_SetSpriteHover(HButton hbutton, HQC_Sprite sprHover) {
    _Btn(hbutton)->sprHover = sprHover;
}


void Button_SetSpritePressed(HButton hbutton, HQC_Sprite sprPressed) {
    _Btn(hbutton)->sprPressed = sprPressed;
}


void Button_SetSoundHover(HButton hbutton, HQC_Sound sndHover) {
    _Btn(hbutton)->sndHover = sndHover;
}


void Button_SetSoundPressed(HButton hbutton, HQC_Sound sndPressed) {
    _Btn(hbutton)->sndHover = sndPressed;
}


void Button_SetText(HButton hbutton, const char* text) {
    _Btn(hbutton)->text = text;
}


void Button_OnClick(HButton hbutton, void (*onButtonClickHandle)(void)) {
    _Btn(hbutton)->onClick = onButtonClickHandle;
}


void Button_SetScale(HButton hbutton, int scale) {
    if (scale < 0) HQC_RaiseErrorFormat("Button scale cannot be %d", scale);

    _Btn(hbutton)->scale = scale;
}

static int _Button_GetSlice(Button* btn) {
    if (!btn || btn->size.x <= 0) {
        HQC_Log("Button_GetSlice: Invalid button or size");
        return 60; // Default slice size
    }
    return btn->size.x / 3;
}

static irect_t _Button_GetRect(Button* btn) {
    if (!btn) {
        HQC_Log("Button_GetRect: NULL button");
        irect_t defaultRect = {0, 0, 180, 72};
        return defaultRect;
    }
    
    int slice = _Button_GetSlice(btn);

    int rx = ((slice * (2+btn->scale)) / 2);
    int ry = btn->size.y / 2;

    irect_t rect = {
        btn->pos.x - rx,
        btn->pos.y - ry,
        rx*2,
        ry*2
    };

    return rect;
}

void Button_Update(HButton hbutton) {
    if (!hbutton) {
        return;
    }
    
    Button* btn = _Btn(hbutton);
    if (!btn || btn->size.x <= 0 || btn->size.y <= 0) {
        return;
    }

    // Get mouse position
    v2i_t mpos = HQC_Input_MouseGetPosition();
    irect_t rect = _Button_GetRect(btn);

    bool mouseInside = (mpos.x > rect.x) && (mpos.y > rect.y) && 
                      (mpos.x < (rect.x+rect.width)) && (mpos.y < (rect.y+rect.height));

    if (mouseInside) {
        bool mouseDown = HQC_Input_MouseLeft();
        bool mousePressed = HQC_Input_MouseLeftPressed();
        
        if (mousePressed) {
            // Mouse just pressed down
            btn->state = BTN_PRESSED;
            if (btn->sndPressed)
                HQC_DJ_PlaySound(btn->sndPressed);
        } else if (btn->state == BTN_PRESSED && !mouseDown) {
            // Mouse was pressed and now released - trigger click
            if (btn->onClick != NULL) {
                HQC_Log("Button clicked! Executing callback...");
                btn->onClick();
            }
            btn->state = BTN_HOVERED;
        } else if (btn->state != BTN_PRESSED) {
            // Mouse hovering
            if (btn->state != BTN_HOVERED && btn->sndHover != NULL)
                HQC_DJ_PlaySound(btn->sndHover);
            btn->state = BTN_HOVERED;
        }
    } else {
        // Mouse outside button
        btn->state = BTN_IDLE;
    }
}


void Button_Draw(HButton hbutton) {
    Button* btn = _Btn(hbutton);

    int slice    = _Button_GetSlice(btn);
    irect_t rect = _Button_GetRect(btn);

    HQC_Texture tex = HQC_Sprite_GetTexture(btn->spr);

    irect_t texRect = HQC_Sprite_GetRect(
        (btn->sprPressed && btn->state == BTN_PRESSED) ? btn->sprPressed :
        (btn->sprHover   && btn->state == BTN_HOVERED) ? btn->sprHover   : btn->spr);
    texRect.width = slice;

    HQC_Artist_DrawTextureRectLeft(tex, rect.x, rect.y, texRect);
    texRect.x   += slice;
    rect.x      += slice;
    for (int i = 0; i < btn->scale; i++) {
        HQC_Artist_DrawTextureRectLeft(tex, rect.x, rect.y, texRect);
        rect.x += slice;
    }
    texRect.x += slice;
    HQC_Artist_DrawTextureRectLeft(tex, rect.x, rect.y, texRect);

    //////////////////////////////////////////////////////////////////////////////////

    if (btn->text != NULL) {
        int shift = (btn->state == BTN_PRESSED) ? 4 : 0;

        HQC_Artist_SetColorHex(C_YELLOW);
        HQC_Artist_DrawTextShadow(
            Store_GetFontByID(FONT_CANCUN_8),
            btn->text, btn->pos.x - shift, btn->pos.y-10 + shift
        );
        HQC_Artist_SetColorHex(C_WHITE);
    }

}


void Button_Destroy(HButton hbutton) {
    HQC_Memory_Free(hbutton);
    hbutton = NULL;
}


///////////////////////////////////////////////////////////////////////
// Checkbox / Slider / Dialogbox
//
// ⚠️ 2026-09-29：这三个控件的函数**在 Menu.h 里声明了但从来没实现**（v2.0.0 重构的
//    遗留：任何调用都会在链接期炸掉）。结算界面需要 Dialogbox，故在此补齐。
///////////////////////////////////////////////////////////////////////

static Checkbox* _Chk(HCheckbox h) { return (Checkbox*)h; }
static Slider*   _Sld(HSlider h)   { return (Slider*)h; }
static Dialogbox* _Dlg(HDialogbox h) { return (Dialogbox*)h; }


HCheckbox Checkbox_Create(float x, float y) {
    Checkbox* chk = HQC_Memory_Allocate(sizeof(*chk));

    chk->pos.x        = x;
    chk->pos.y        = y;
    chk->checked      = 0;
    chk->onChecked    = NULL;
    chk->onUnchecked  = NULL;

    return chk;
}

void Checkbox_OnChecked(HCheckbox hcheckbox, void (*onCheckedFunction)(void)) {
    _Chk(hcheckbox)->onChecked = onCheckedFunction;
}

void Checkbox_OnUnchecked(HCheckbox hcheckbox, void (*onUncheckedFunction)(void)) {
    _Chk(hcheckbox)->onUnchecked = onUncheckedFunction;
}

void Checkbox_Update(HCheckbox hcheckbox) {
    Checkbox* chk = _Chk(hcheckbox);
    if (!chk) return;

    HQC_Sprite spr = Store_GetSpriteByID(SPR_MENU_CHECKBOX);
    irect_t rect = spr ? HQC_Sprite_GetRect(spr) : (irect_t){ 0, 0, 32, 32 };

    v2i_t m = HQC_Input_MouseGetPosition();

    bool inside = m.x >= chk->pos.x - rect.width / 2 && m.x <= chk->pos.x + rect.width / 2 &&
                  m.y >= chk->pos.y - rect.height / 2 && m.y <= chk->pos.y + rect.height / 2;

    if (inside && HQC_Input_MouseLeftPressed()) {
        chk->checked = !chk->checked;

        HQC_DJ_PlaySound(Store_GetSoundByID(SND_BUTTON1));

        if (chk->checked && chk->onChecked)         chk->onChecked();
        else if (!chk->checked && chk->onUnchecked) chk->onUnchecked();
    }
}

void Checkbox_Draw(HCheckbox hcheckbox) {
    Checkbox* chk = _Chk(hcheckbox);
    if (!chk) return;

    HQC_Sprite spr = Store_GetSpriteByID(chk->checked ? SPR_MENU_CHECKBOX_CHECKED : SPR_MENU_CHECKBOX);
    if (spr) HQC_Artist_DrawSprite(spr, chk->pos.x, chk->pos.y);
}

void Checkbox_Destroy(HCheckbox hcheckbox) {
    HQC_Memory_Free(hcheckbox);
}


///////////////////////////////////////////////////////////////////////

HSlider Slider_Create(float x, float y) {
    Slider* sld = HQC_Memory_Allocate(sizeof(*sld));

    sld->pos.x     = x;
    sld->pos.y     = y;
    sld->value     = 1.0f;
    sld->onChange  = NULL;
    sld->dragging  = 0;

    HQC_Sprite spr = Store_GetSpriteByID(SPR_MENU_SLIDER);
    irect_t rect = spr ? HQC_Sprite_GetRect(spr) : (irect_t){ 0, 0, 200, 16 };
    sld->width = rect.width;

    return sld;
}

void Slider_OnChange(HSlider hslider, void (*onChange)(float)) {
    _Sld(hslider)->onChange = onChange;
}

void Slider_Update(HSlider hslider) {
    Slider* sld = _Sld(hslider);
    if (!sld) return;

    v2i_t m = HQC_Input_MouseGetPosition();

    int halfW = sld->width / 2;
    int left  = (int)sld->pos.x - halfW;

    bool inside = m.x >= left - 8 && m.x <= left + sld->width + 8 &&
                  m.y >= sld->pos.y - 20 && m.y <= sld->pos.y + 20;

    if (HQC_Input_MouseLeftPressed() && inside)
        sld->dragging = 1;

    if (!HQC_Input_MouseLeft())
        sld->dragging = 0;

    if (sld->dragging) {
        float v = (float)(m.x - left) / (float)sld->width;
        if (v < 0.0f) v = 0.0f;
        if (v > 1.0f) v = 1.0f;

        if (v != sld->value) {
            sld->value = v;
            if (sld->onChange) sld->onChange(v);
        }
    }
}

void Slider_Draw(HSlider hslider) {
    Slider* sld = _Sld(hslider);
    if (!sld) return;

    HQC_Sprite track = Store_GetSpriteByID(SPR_MENU_SLIDER);
    if (track) HQC_Artist_DrawSprite(track, sld->pos.x, sld->pos.y);

    HQC_Sprite thumb = Store_GetSpriteByID(SPR_MENU_SLIDER_THUMB);
    if (thumb) {
        int halfW = sld->width / 2;
        HQC_Artist_DrawSprite(thumb, (float)((int)sld->pos.x - halfW) + sld->value * sld->width,
                              sld->pos.y);
    }
}

void Slider_Destroy(HSlider hslider) {
    HQC_Memory_Free(hslider);
}


///////////////////////////////////////////////////////////////////////

HDialogbox Dialogbox_Create(float x, float y) {
    Dialogbox* dlg = HQC_Memory_Allocate(sizeof(*dlg));

    dlg->pos.x  = x;
    dlg->pos.y  = y;
    dlg->width  = 560;
    dlg->height = 0;            // 0 = 按内容自动算（见 _Dialogbox_ComputeSize）
    dlg->title  = NULL;

    dlg->buttons    = HQC_Container_CreateVector(sizeof(HButton));
    dlg->sliders    = HQC_Container_CreateVector(sizeof(HSlider));
    dlg->checkboxes = HQC_Container_CreateVector(sizeof(HCheckbox));
    dlg->texts      = HQC_Container_CreateVector(sizeof(DialogText));

    return dlg;
}

void Dialogbox_AddButton(HDialogbox hdialogbox, HButton hbutton) {
    Dialogbox* dlg = _Dlg(hdialogbox);
    if (!dlg || !hbutton) return;

    HQC_Container_VectorAdd(dlg->buttons, &hbutton);
}

void Dialogbox_AddSlider(HDialogbox hdialogbox, HSlider hslider) {
    Dialogbox* dlg = _Dlg(hdialogbox);
    if (!dlg || !hslider) return;

    HQC_Container_VectorAdd(dlg->sliders, &hslider);
}

void Dialogbox_AddCheckbox(HDialogbox hdialogbox, HCheckbox hcheckbox) {
    Dialogbox* dlg = _Dlg(hdialogbox);
    if (!dlg || !hcheckbox) return;

    HQC_Container_VectorAdd(dlg->checkboxes, &hcheckbox);
}

void Dialogbox_SetTitle(HDialogbox hdialogbox, const char* title) {
    _Dlg(hdialogbox)->title = title;
}

void Dialogbox_AddText(HDialogbox hdialogbox, const char* text, float x, float y) {
    Dialogbox_AddTextEx(hdialogbox, text, x, y, C_WHITE);
}

// 扩展：带颜色（本文件新增，已在 Menu.h 声明）
void Dialogbox_AddTextEx(HDialogbox hdialogbox, const char* text, float x, float y, uint32_t colorHex) {
    Dialogbox* dlg = _Dlg(hdialogbox);
    if (!dlg) return;

    DialogText t;
    t.text  = text;             // 调用方保证字符串生命周期（通常传字符串字面量）
    t.x     = x;                // 相对对话框中心
    t.y     = y;
    t.color = colorHex;

    HQC_Container_VectorAdd(dlg->texts, &t);
}


// 对话框尺寸：宽度固定，高度按标题 + 按钮行数算（最小 320）
// ⚠️ 文本行由调用方给相对坐标，布局契约是"文本区在对话框上半部、按钮从底部往上排"，
//    高度公式留够空隙（否则文本会被第一个按钮压住，实测过）。
static void _Dialogbox_ComputeSize(Dialogbox* dlg) {
    int rows = (int)HQC_Container_VectorCount(dlg->buttons);

    int height = 210 + rows * 92;

    if (dlg->title)
        height += 40;

    if (height < 320) height = 320;

    dlg->height = height;
}

// 按钮自下而上排在对话框底部（最后一个按钮最靠下，符合"主按钮在下"的习惯）
static void _Dialogbox_Layout(Dialogbox* dlg) {
    _Dialogbox_ComputeSize(dlg);

    int bot = (int)dlg->pos.y + dlg->height / 2 - 70;

    size_t n = HQC_Container_VectorCount(dlg->buttons);

    for (size_t i = 0; i < n; i++) {
        HButton* hbtn = HQC_Container_VectorGet(dlg->buttons, i);
        Button*  btn  = (Button*)*hbtn;

        int slotFromBottom = (int)(n - 1 - i);

        btn->pos.x = dlg->pos.x;
        btn->pos.y = (float)(bot - slotFromBottom * 92);
    }
}


// 九宫格背景：四角 + 四边平铺 + 中心平铺（精灵来自 menu.png，尺寸未知，一律按矩形平铺）
static void _Dialogbox_DrawBox(Dialogbox* dlg) {
    HQC_Sprite lt = Store_GetSpriteByID(SPR_MENU_DIALOG_BOX_RECT_LT);
    HQC_Sprite ct = Store_GetSpriteByID(SPR_MENU_DIALOG_BOX_RECT_CT);
    HQC_Sprite rt = Store_GetSpriteByID(SPR_MENU_DIALOG_BOX_RECT_RT);
    HQC_Sprite lc = Store_GetSpriteByID(SPR_MENU_DIALOG_BOX_RECT_LC);
    HQC_Sprite cc = Store_GetSpriteByID(SPR_MENU_DIALOG_BOX_RECT_CC);
    HQC_Sprite rc = Store_GetSpriteByID(SPR_MENU_DIALOG_BOX_RECT_RC);
    HQC_Sprite lb = Store_GetSpriteByID(SPR_MENU_DIALOG_BOX_RECT_LB);
    HQC_Sprite cb = Store_GetSpriteByID(SPR_MENU_DIALOG_BOX_RECT_CB);
    HQC_Sprite rb = Store_GetSpriteByID(SPR_MENU_DIALOG_BOX_RECT_RB);

    int x0 = (int)dlg->pos.x - dlg->width / 2;
    int y0 = (int)dlg->pos.y - dlg->height / 2;
    int x1 = x0 + dlg->width;
    int y1 = y0 + dlg->height;

    if (!lt || !cc) {
        // 精灵缺失兜底：半透明底 + 描边，保证内容仍可读
        HQC_Color dim = { 20, 24, 32, 235 };
        HQC_Artist_SetColor(dim);
        HQC_Artist_FillRect((float)x0, (float)y0, (float)dlg->width, (float)dlg->height);
        HQC_Artist_SetColorHex(C_WHITE);
        return;
    }

    irect_t rl = HQC_Sprite_GetRect(lt);
    irect_t rc_ = HQC_Sprite_GetRect(cc);

    int cw = rl.width, ch = rl.height;
    int tw = rc_.width, th = rc_.height;

    if (cw <= 0 || ch <= 0 || tw <= 0 || th <= 0) return;

    // 中心
    for (int y = y0 + ch; y < y1 - ch; y += th) {
        for (int x = x0 + cw; x < x1 - cw; x += tw) {
            HQC_Artist_DrawSprite(cc, (float)(x + tw / 2), (float)(y + th / 2));
        }
    }

    // 上下边
    for (int x = x0 + cw; x < x1 - cw; x += tw) {
        HQC_Artist_DrawSprite(ct, (float)(x + tw / 2), (float)(y0 + ch / 2));
        HQC_Artist_DrawSprite(cb, (float)(x + tw / 2), (float)(y1 - ch / 2));
    }

    // 左右边
    for (int y = y0 + ch; y < y1 - ch; y += th) {
        HQC_Artist_DrawSprite(lc, (float)(x0 + cw / 2), (float)(y + th / 2));
        HQC_Artist_DrawSprite(rc, (float)(x1 - cw / 2), (float)(y + th / 2));
    }

    // 四角
    HQC_Artist_DrawSprite(lt, (float)(x0 + cw / 2), (float)(y0 + ch / 2));
    HQC_Artist_DrawSprite(rt, (float)(x1 - cw / 2), (float)(y0 + ch / 2));
    HQC_Artist_DrawSprite(lb, (float)(x0 + cw / 2), (float)(y1 - ch / 2));
    HQC_Artist_DrawSprite(rb, (float)(x1 - cw / 2), (float)(y1 - ch / 2));
}


void Dialogbox_Update(HDialogbox hdialogbox) {
    Dialogbox* dlg = _Dlg(hdialogbox);
    if (!dlg) return;

    _Dialogbox_Layout(dlg);

    size_t n = HQC_Container_VectorCount(dlg->buttons);
    for (size_t i = 0; i < n; i++) {
        HButton* btn = HQC_Container_VectorGet(dlg->buttons, i);
        Button_Update(*btn);
    }

    n = HQC_Container_VectorCount(dlg->sliders);
    for (size_t i = 0; i < n; i++) {
        HSlider* sld = HQC_Container_VectorGet(dlg->sliders, i);
        Slider_Update(*sld);
    }

    n = HQC_Container_VectorCount(dlg->checkboxes);
    for (size_t i = 0; i < n; i++) {
        HCheckbox* chk = HQC_Container_VectorGet(dlg->checkboxes, i);
        Checkbox_Update(*chk);
    }
}


void Dialogbox_Draw(HDialogbox hdialogbox) {
    Dialogbox* dlg = _Dlg(hdialogbox);
    if (!dlg) return;

    _Dialogbox_Layout(dlg);

    _Dialogbox_DrawBox(dlg);

    if (dlg->title) {
        HQC_Artist_SetColorHex(C_WHITE);
        HQC_Artist_DrawTextShadow(Store_GetFontByID(FONT_CANCUN_13), dlg->title,
                                  dlg->pos.x, dlg->pos.y - dlg->height / 2 + 50);
    }

    size_t n = HQC_Container_VectorCount(dlg->texts);
    for (size_t i = 0; i < n; i++) {
        DialogText* t = HQC_Container_VectorGet(dlg->texts, i);

        HQC_Artist_SetColorHex(t->color);
        HQC_Artist_DrawText(Store_GetFontByID(FONT_CANCUN_12), t->text,
                            dlg->pos.x + t->x, dlg->pos.y + t->y);
    }

    HQC_Artist_SetColorHex(C_WHITE);

    n = HQC_Container_VectorCount(dlg->sliders);
    for (size_t i = 0; i < n; i++) {
        HSlider* sld = HQC_Container_VectorGet(dlg->sliders, i);
        Slider_Draw(*sld);
    }

    n = HQC_Container_VectorCount(dlg->checkboxes);
    for (size_t i = 0; i < n; i++) {
        HCheckbox* chk = HQC_Container_VectorGet(dlg->checkboxes, i);
        Checkbox_Draw(*chk);
    }

    n = HQC_Container_VectorCount(dlg->buttons);
    for (size_t i = 0; i < n; i++) {
        HButton* btn = HQC_Container_VectorGet(dlg->buttons, i);
        Button_Draw(*btn);
    }
}


void Dialogbox_GetButtonPos(HDialogbox hdialogbox, int index, float* x, float* y) {
    Dialogbox* dlg = _Dlg(hdialogbox);
    if (!dlg) return;

    _Dialogbox_Layout(dlg);

    if (index < 0 || (size_t)index >= HQC_Container_VectorCount(dlg->buttons)) return;

    HButton* hbtn = HQC_Container_VectorGet(dlg->buttons, index);
    Button*  btn  = (Button*)*hbtn;

    if (x) *x = btn->pos.x;
    if (y) *y = btn->pos.y;
}


void Dialogbox_Destroy(HDialogbox hdialogbox) {
    Dialogbox* dlg = _Dlg(hdialogbox);
    if (!dlg) return;

    size_t n = HQC_Container_VectorCount(dlg->buttons);
    for (size_t i = 0; i < n; i++) {
        HButton* btn = HQC_Container_VectorGet(dlg->buttons, i);
        Button_Destroy(*btn);
    }
    HQC_Container_FreeVector(dlg->buttons);

    n = HQC_Container_VectorCount(dlg->sliders);
    for (size_t i = 0; i < n; i++) {
        HSlider* sld = HQC_Container_VectorGet(dlg->sliders, i);
        Slider_Destroy(*sld);
    }
    HQC_Container_FreeVector(dlg->sliders);

    n = HQC_Container_VectorCount(dlg->checkboxes);
    for (size_t i = 0; i < n; i++) {
        HCheckbox* chk = HQC_Container_VectorGet(dlg->checkboxes, i);
        Checkbox_Destroy(*chk);
    }
    HQC_Container_FreeVector(dlg->checkboxes);

    HQC_Container_FreeVector(dlg->texts);

    HQC_Memory_Free(hdialogbox);
}