#include "../HQC.h"

#include <SDL.h>
#include <SDL_image.h>
#include <SDL_ttf.h>

static struct {
    SDL_Window*     window;
    SDL_Renderer* 	render;
} graphics;

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

void HQC_CreateWindow(const char* caption, int width, int height) {
    // ⚠️ 不再请求 SDL_WINDOW_OPENGL：无头（SDL_VIDEODRIVER=dummy）下 dummy 驱动
    // 不支持该 flag，窗口创建直接失败 → 自动测试跑不起来（2026-09-29）。
    // 渲染走 SDL_Renderer（GPU 加速由渲染器自己选），不需要 GL 窗口标志。
    Uint32 windowFlags = SDL_WINDOW_RESIZABLE;

    const char* videoDriver = SDL_GetCurrentVideoDriver();
    (void)videoDriver;

    graphics.window = SDL_CreateWindow(
        caption, 
        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 
        width, height, 
        windowFlags
    );

    if (!graphics.window)
        HQC_RaiseErrorHeaderFormat(
            "SDL Error", "Window creation fail [%s]", SDL_GetError());

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "2");

    graphics.render = SDL_CreateRenderer(
        graphics.window, 
        -1, 
        SDL_RENDERER_ACCELERATED | 
        SDL_RENDERER_PRESENTVSYNC 
    );

    // 无 GPU/无头（SDL_VIDEODRIVER=dummy）时加速渲染器会创建失败 →
    // 回退软件渲染器，而不是致命退出（2026-09-29：自动测试/CI 必需）
    if (!graphics.render) {
        HQC_Log("Renderer: accelerated/vsync unavailable (%s), falling back to software",
                SDL_GetError());
        graphics.render = SDL_CreateRenderer(graphics.window, -1, SDL_RENDERER_SOFTWARE);
    }

    if (!graphics.render)
        HQC_RaiseErrorHeaderFormat(
            "SDL Error", "Rendere creation fail [%s]", SDL_GetError());

    SDL_RenderSetLogicalSize(graphics.render, width, height);
}


///////////////////////////////////////////////////////////////////////
// 输入层：每帧锁存一次（HQC_Input_Update），查询函数无副作用
///////////////////////////////////////////////////////////////////////

typedef struct InputState {
    v2i_t   mousePos;
    bool    left;
    bool    right;

    bool    prevLeft;
    bool    prevRight;

    bool    keys[HQC_NUM_SCANCODES];
    bool    prevKeys[HQC_NUM_SCANCODES];
} InputState;

static InputState in;

// 脚本输入（自动测试/无头）：SetScripted 打开后覆盖鼠标，SetScriptedKey 叠加按键
static bool  scriptedEnabled  = false;
static v2i_t scriptedMousePos = { 0, 0 };
static bool  scriptedLeft     = false;
static bool  scriptedRight    = false;
static bool  scriptedKeys[HQC_NUM_SCANCODES] = { false };


static bool RawSDLKeyDown__(HQC_Key key) {
    const Uint8* st = SDL_GetKeyboardState(NULL);
    return st[key] != 0;
}

static void RawSDLMouse__(v2i_t* pos, bool* left, bool* right) {
    int x = 0, y = 0;
    Uint32 mask = SDL_GetMouseState(&x, &y);
    pos->x = x;
    pos->y = y;
    *left  = (mask & SDL_BUTTON_LMASK) != 0;
    *right = (mask & SDL_BUTTON_RMASK) != 0;
}


void HQC_Input_Update() {
    in.prevLeft  = in.left;
    in.prevRight = in.right;

    for (int i = 0; i < HQC_NUM_SCANCODES; i++)
        in.prevKeys[i] = in.keys[i];

    if (scriptedEnabled) {
        in.mousePos = scriptedMousePos;
        in.left     = scriptedLeft;
        in.right    = scriptedRight;

        for (int i = 0; i < HQC_NUM_SCANCODES; i++)
            in.keys[i] = scriptedKeys[i];
    } else {
        RawSDLMouse__(&in.mousePos, &in.left, &in.right);

        for (int i = 0; i < HQC_NUM_SCANCODES; i++)
            in.keys[i] = RawSDLKeyDown__((HQC_Key)i);
    }
}


void HQC_Input_SetScripted(bool enabled, int mouseX, int mouseY, bool leftDown, bool rightDown) {
    scriptedEnabled  = enabled;
    scriptedMousePos.x = mouseX;
    scriptedMousePos.y = mouseY;
    scriptedLeft     = leftDown;
    scriptedRight    = rightDown;
}


void HQC_Input_SetScriptedKey(HQC_Key key, bool down) {
    if (key > HQC_KEY_UNKNOWN && key < HQC_NUM_SCANCODES)
        scriptedKeys[key] = down;
}


void HQC_Input_ClearScriptedKeys() {
    for (int i = 0; i < HQC_NUM_SCANCODES; i++)
        scriptedKeys[i] = false;
}


v2i_t HQC_Input_MouseGetPosition() {
    return in.mousePos;
}


bool HQC_Input_MouseLeft() {
    return in.left;
}


bool HQC_Input_MouseRight() {
    return in.right;
}


bool HQC_Input_MouseLeftPressed() {
    return in.left && !in.prevLeft;
}


bool HQC_Input_MouseRightPressed() {
    return in.right && !in.prevRight;
}


bool HQC_Input_IsKeyDown(HQC_Key key) {
    if (key <= HQC_KEY_UNKNOWN || key >= HQC_NUM_SCANCODES)
        return false;
    return in.keys[key];
}


bool HQC_Input_KeyPressed(HQC_Key key) {
    if (key <= HQC_KEY_UNKNOWN || key >= HQC_NUM_SCANCODES)
        return false;
    return in.keys[key] && !in.prevKeys[key];
}


bool HQC_Window_PollEvent(HQC_Event* event) {
    SDL_Event sdlEvent;
    bool res = SDL_PollEvent(&sdlEvent);

    *event = (int)sdlEvent.type;

    return res;
};

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

static float _drawAngle = 0;
static float _drawScale = 1;
static float _drawAlpha = 1;
static uint32_t _drawTextureColorMod = C_WHITE;

void HQC_Artist_DrawSetAlpha(float alpha) {
    _drawAlpha = alpha;
}


void HQC_Artist_DrawSetScale(float scale) {
    _drawScale = scale;
}


void HQC_Artist_DrawSetAngle(float angle) {
    _drawAngle = angle;
}


void HQC_Artist_DrawSetAngleDegrees(float angleInDegrees) {
    HQC_Artist_DrawSetAngle(HQC_DegreesToRadian(angleInDegrees));
}


HQC_Texture HQC_Artist_LoadTexture(const char* texfile) {
    SDL_Surface* loadedSurface = IMG_Load(texfile);

    if (!loadedSurface) {
        HQC_Log("Failed to load image %s: %s", texfile, IMG_GetError());
        return NULL;
    }
        
    SDL_Texture* newTexture = SDL_CreateTextureFromSurface(graphics.render, loadedSurface);
    if (!newTexture) {
        HQC_Log("Failed to create texture %s: %s", texfile, SDL_GetError());
        SDL_FreeSurface(loadedSurface);
        return NULL;
    }

    SDL_FreeSurface( loadedSurface );
    
    return newTexture;
}

void HQC_Artist_FreeTexture(HQC_Texture texture) {
    if (!texture) return;
    SDL_DestroyTexture((SDL_Texture*)texture);
}

void HQC_Artist_GetTextureSize(HQC_Texture texture, int* width, int* height) {
    if (width)  *width  = 0;
    if (height) *height = 0;

    if (!texture) return;

    int w = 0, h = 0;
    SDL_QueryTexture((SDL_Texture*)texture, NULL, NULL, &w, &h);

    if (width)  *width  = w;
    if (height) *height = h;
}


void HQC_Artist_DrawTexture(HQC_Texture texture, float x, float y) {
    if (!texture) 
        HQC_RaiseErrorHeaderFormat(
            "HQC_Artist_DrawTexture", 
            "Bad texture [%s]", 
            SDL_GetError());

    SDL_Texture* sdlTexture = (SDL_Texture*)texture;

    int w, h;
    SDL_QueryTexture(sdlTexture, NULL, NULL, &w, &h);
    SDL_FRect rect = { x - w/2, y - h/2, w, h };

    // HQC_Artist_DrawTextureRect(texture, x, y, rect);

    SDL_RenderCopyF(graphics.render, sdlTexture, NULL, &rect);
}


void HQC_Artist_DrawTextureRectLeft(HQC_Texture texture, float x, float y, irect_t rect) {
    if (!texture) 
        HQC_RaiseErrorHeaderFormat(
            "HQC_Artist_DrawTextureRect", 
            "Bad texture [%s]", 
            SDL_GetError());

    SDL_Texture* sdlTexture = (SDL_Texture*)texture;

    SDL_Rect* sdlRect = (SDL_Rect*)&rect;
    SDL_FRect posRect = {x, y, rect.width * _drawScale, rect.height * _drawScale};

    SDL_SetTextureAlphaMod(sdlTexture, (Uint8)(_drawAlpha * 255));
    SDL_SetTextureColorMod(
            sdlTexture,
            (_drawTextureColorMod >> 16) & 0xFF,
            (_drawTextureColorMod >> 8) & 0xFF,
            (_drawTextureColorMod >> 0) & 0xFF);

    SDL_RenderCopyExF(
        graphics.render, 
        sdlTexture, 
        sdlRect, 
        &posRect, 
        HQC_RadianToDegrees(_drawAngle),
        NULL,
        SDL_FLIP_NONE
    );

    //SDL_SetTextureAlphaMod(sdlTexture, 255);

}

void HQC_Artist_DrawTextureRect(HQC_Texture texture, float x, float y, irect_t rect) {
    HQC_Artist_DrawTextureRectLeft(
        texture, 
        x - (rect.width * _drawScale) / 2,
        y - (rect.height * _drawScale) / 2,
        rect
    );
}


void HQC_Artist_DrawLine(float x1, float y1, float x2, float y2) {
    SDL_RenderDrawLineF(graphics.render, x1, y1, x2, y2);
}


void HQC_Artist_DrawPoint(float x, float y) {
    SDL_RenderDrawPointF(graphics.render, x, y);
}

void HQC_Artist_FillRect(float x, float y, float width, float height) {
    SDL_FRect rect = { x, y, width, height };
    SDL_RenderFillRectF(graphics.render, &rect);
}

void HQC_Artist_SetDrawColorMod(uint32_t color) {
    _drawTextureColorMod = color;
}

void HQC_Artist_SetColor(HQC_Color color) {
    SDL_SetRenderDrawColor(
        graphics.render, color.R, color.G, color.B, color.A
    );
}


HQC_Color HQC_Artist_GetColor() {
    HQC_Color color;
    SDL_GetRenderDrawColor(graphics.render, &color.R, &color.G, &color.B, &color.A);
    return color;
}


void HQC_Artist_SetColorHex(uint32_t color) {
    HQC_Color hqcColor;

    hqcColor.R = (color  & C_RED     )  >> 16;
    hqcColor.G = (color  & C_GREEN   )  >> 8;
    hqcColor.B = (color  & C_BLUE    );
    hqcColor.A = (~color & 0xFF000000)  >> 24;

    HQC_Artist_SetColor(hqcColor);
}


uint32_t HQC_Artist_GetColorHex() {
    HQC_Color hqcColor = HQC_Artist_GetColor();

    uint32_t res = 
        ((uint32_t)hqcColor.A << 24) | 
        (hqcColor.R << 16) | 
        (hqcColor.G << 8) | 
        hqcColor.B; 

    return res;
}


void HQC_Artist_Clear() {
    SDL_RenderClear(graphics.render);
}


void HQC_Artist_Display() {
    SDL_RenderPresent(graphics.render);
}


// 画面存盘（自动测试取证）：SDL_RenderReadPixels 对软件渲染器同样有效
bool HQC_Artist_SaveScreenshot(const char* filepath, int width, int height) {
    if (!graphics.render || !filepath)
        return false;

    int winW = 0, winH = 0, outW = 0, outH = 0;
    float sX = 1, sY = 1;
    SDL_Rect vp = { 0, 0, 0, 0 };

    SDL_GetWindowSize(graphics.window, &winW, &winH);
    SDL_GetRendererOutputSize(graphics.render, &outW, &outH);
    SDL_RenderGetScale(graphics.render, &sX, &sY);
    SDL_RenderGetViewport(graphics.render, &vp);

    HQC_Log("[screenshot] window=%dx%d output=%dx%d scale=%.2fx%.2f viewport=(%d,%d,%dx%d) read=%dx%d",
            winW, winH, outW, outH, sX, sY, vp.x, vp.y, vp.w, vp.h, width, height);

    SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(
        0, width, height, 32, SDL_PIXELFORMAT_ARGB8888);

    if (!surface)
        return false;

    if (SDL_RenderReadPixels(graphics.render, NULL, SDL_PIXELFORMAT_ARGB8888,
                             surface->pixels, surface->pitch) != 0) {
        SDL_FreeSurface(surface);
        return false;
    }

    bool ok = (SDL_SaveBMP(surface, filepath) == 0);

    SDL_FreeSurface(surface);

    return ok;
}


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////

typedef struct Font {
    TTF_Font* ttf;
} Font;


HQC_Texture HQC_Artist_CreateTextTexture(HQC_Font hfont, const char* text, uint32_t color) {
    Font* font = (Font*)hfont;
    if (font->ttf == NULL) return NULL;

    HQC_Color c;
    c.R = (color  & C_RED     )  >> 16;
    c.G = (color  & C_GREEN   )  >> 8;
    c.B = (color  & C_BLUE    );
    c.A = (~color & 0xFF000000)  >> 24;
    
    SDL_Color sdlColor = { c.R, c.G, c.B, c.A };

    TTF_SetFontWrappedAlign(font->ttf, TTF_WRAPPED_ALIGN_CENTER);
    SDL_Surface* surface = TTF_RenderText_Solid_Wrapped(font->ttf, text, sdlColor, 0);
    if (!surface) return NULL;

    SDL_Texture* texture = SDL_CreateTextureFromSurface(graphics.render, surface);
    SDL_FreeSurface(surface);
    
    return texture;
}

HQC_Font HQC_Font_LoadTrueType(const char* filepath, int size) {
    Font* font = HQC_Memory_Allocate(sizeof(*font));

    font->ttf = TTF_OpenFont(filepath, size);
    if (!font->ttf)
        HQC_RaiseErrorHeaderFormat(
            "HQC_Font_LoadTrueType",
            "Cannot load font %s with size %d",
            filepath, size
        );

    

    return font;
}

void HQC_Artist_SetFontOutline(HQC_Font hfont, int outlineSize) {
    Font* font = (Font*)hfont;

    if (font->ttf == NULL) return;

    TTF_SetFontOutline(font->ttf, outlineSize);
}

void HQC_Artist_DrawText(HQC_Font hfont, const char* text, float x, float y) {
    Font* font = (Font*)hfont;
    
    if (font->ttf == NULL)
        return;
    
    HQC_Color color = HQC_Artist_GetColor();
    SDL_Color sdlColor = { color.R, color.G, color.B, color.A };

    TTF_SetFontWrappedAlign(font->ttf, TTF_WRAPPED_ALIGN_CENTER);
    SDL_Surface* surface = TTF_RenderText_Solid_Wrapped(font->ttf, text, sdlColor, 0);
    if (!surface) {
        const char* err = SDL_GetError();
        HQC_RaiseErrorFormat("Cannot draw text of font (%p) with text \"%s\" at (%lf, %lf).\n"
                             "SDL_Error - %s", hfont, text, x, y, err);
    }

    SDL_Texture* texture = SDL_CreateTextureFromSurface(graphics.render, surface);

    HQC_Artist_DrawTexture(texture, x, y);

    SDL_FreeSurface(surface);
    SDL_DestroyTexture(texture);
}

void HQC_Artist_DrawTextF(HQC_Font hfont, float x, float y, const char* format, ...) {
    va_list args;
    va_start(args, format);

    int len = vsnprintf(NULL, 0, format, args);
    va_end(args);

    // format message
    char* msg = malloc(len + 1);
    va_start(args, format);
    vsnprintf(msg, len + 1, format, args);
    va_end(args);

    HQC_Artist_DrawText(hfont, msg, x, y);

    free(msg);
}

void HQC_Artist_DrawTextShadow(HQC_Font hfont, const char* text, float x, float y) {
    uint32_t pcolor = HQC_Artist_GetColorHex();

    HQC_Artist_SetColorHex(C_BLACK);
    HQC_Artist_DrawText(hfont, text, x+2, y+2);
    HQC_Artist_SetColorHex(pcolor);

    HQC_Artist_DrawText(hfont, text, x, y);
}