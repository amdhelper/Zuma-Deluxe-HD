#include "Index.h"

#include "../Level.h"
#include "../Frog.h"
#include "../BallChain.h"
#include "../FloatingText.h"
#include "../Statistics.h"
#include "../LevelMgr.h"

#include "../ecs/World.h"

#include "../systems/SpriteDrawSystem.h"
#include "../entities/FrogEntity.h"

// Forward declaration for Level internal structure
typedef struct Level {
    void* settings;
    void* graphics;
    HQC_Texture texture;
    HQC_Texture textureTopLevel;
    void* curveA;
    void* curveB;
} Level;

struct {
    HFrog  frog;
    HLevel level;
    HBallChain chain;
    HBulletList bulletList;
    HBallChainGenerator generator;

    World* world;
    
    HQC_Texture texUI_Working;
    HQC_Texture texUI_Menu;
    HQC_Texture texUI_LevelName;
} game;

static bool levelComplete = false;
static int levelCompleteTimer = 0;

static void GoBack_() {
    Scene_Change(SC_TEST);
}

static void Game_Start_() {
    HQC_Log("Starting game scene...");
    
    levelComplete = false;
    levelCompleteTimer = 0;
    
    // Get current level data from manager
    LevelSettings* levelSettings = LevelMgr_GetCurrentSettings();
    LevelGraphics* levelGx = LevelMgr_GetCurrentGraphics();

    if (!levelSettings || !levelGx) {
        HQC_Log("Failed to get level data from LevelMgr. Using fallback/hardcoded.");
        
        // Fallback or Error
        // Revert to hardcoded for safety if LevelMgr fails or is empty?
        // Or just fail.
        
        // Let's keep the hardcoded as a fallback for now if Mgr returns NULL
        // But Mgr should work if xml is loaded.
        // If fail, return to menu.
        Scene_Change(SC_MENU);
        return;
    }
    
    // Ensure coins pos list is initialized if not (LevelMgr just zeroed it)
    if (!levelGx->coinsPosList) {
         levelGx->coinsPosList = HQC_Container_CreateVector(sizeof(v2f_t));
    }

    game.level = Level_Load(levelSettings, levelGx);
    if (!game.level) {
        HQC_Log("Failed to load level, returning to menu");
        Scene_Change(SC_MENU);
        return;
    }

    game.bulletList = BulletList_Create();
    game.frog       = Frog_Create(levelGx->frogPos.x, levelGx->frogPos.y, game.bulletList);
    game.chain      = BallChain_Create(game.level, game.bulletList);
    game.generator  = BallChainGenerator_Create(game.chain);
    game.world      = World_Create();

    // Start generating balls
    BallChainGenerator_GenerateSequence(game.generator, 50);
    
    game.texUI_Working = HQC_Artist_CreateTextTexture(Store_GetFontByID(0), "Game Scene - Working!", 0xFFFFFF);
    game.texUI_Menu    = HQC_Artist_CreateTextTexture(Store_GetFontByID(0), "Press M to return to menu", 0xFFFFFF);
    game.texUI_LevelName = NULL;
    if (game.level) {
        const char* levelName = Level_GetDisplayName(game.level);
        if (levelName) {
            game.texUI_LevelName = HQC_Artist_CreateTextTexture(Store_GetFontByID(0), levelName, 0xFFFFFF);
        }
    }
    
    HQC_Log("Game scene started successfully (Complex objects enabled)");
}


static void Game_Update__() {
    // Minimal update with safety checks
    if (!game.level) {
        return;
    }
    
    if (game.frog)      Frog_Update(game.frog);
    if (game.chain)     BallChain_Update(game.chain);
    if (game.generator) BallChainGenerator_Update(game.generator);
    if (game.bulletList) BulletList_Update(game.bulletList);
    
    if (game.chain && game.generator && BallChain_IsEmpty(game.chain) && BallChainGenerator_IsFinished(game.generator)) {
        if (!levelComplete) {
            levelComplete = true;
            levelCompleteTimer = 180; // 3 seconds
            HQC_Log("Level Complete!");
        }
    }

    if (levelComplete) {
        levelCompleteTimer--;
        if (levelCompleteTimer <= 0) {
            if (LevelMgr_AdvanceLevel()) {
                HQC_Log("Advancing to next level...");
                Scene_Change(SC_GAME);
            } else {
                HQC_Log("Game Complete! Returning to menu.");
                Scene_Change(SC_MENU);
            }
        }
        return;
    }
    
    // Add any necessary update logic here
}


static void Game_Draw__() {
    // Draw level background if available
    if (game.level) {
        float cx = 1280.f / 2;
        float cy = 720.f  / 2;
        
        // Draw just the background texture
        Level* level = (Level*)game.level;
        if (level && level->texture) {
            HQC_Artist_DrawTexture(level->texture, cx, cy);
        }
    } else {
        // Fallback background
        HQC_Artist_SetColorHex(0x2C3E50);
    }

    if (game.chain) BallChain_Draw(game.chain);
    if (game.frog)  Frog_Draw(game.frog);
    if (game.bulletList) BulletList_Draw(game.bulletList);
    if (game.frog)  Frog_DrawTop(game.frog);

    if (levelComplete) {
        HQC_Artist_SetColorHex(0xFFFF00);
        HQC_Artist_DrawText(Store_GetFontByID(0), "LEVEL COMPLETE!", 640, 360);
    }
    
    // Draw UI text
    if (game.texUI_Working) HQC_Artist_DrawTexture(game.texUI_Working, 640, 100);
    if (game.texUI_Menu)    HQC_Artist_DrawTexture(game.texUI_Menu, 640, 650);
    if (game.texUI_LevelName) HQC_Artist_DrawTexture(game.texUI_LevelName, 640, 140);
}

static void Game_Free_() {
    if (game.generator) {
        BallChainGenerator_Destroy(game.generator);
        game.generator = NULL;
    }
    if (game.chain) {
        BallChain_Destroy(game.chain);
        game.chain = NULL;
    }
    if (game.frog) {
        Frog_Destroy(game.frog);
        game.frog = NULL;
    }
    if (game.bulletList) {
        BulletList_Free(game.bulletList);
        game.bulletList = NULL;
    }
    if (game.level) {
        Level_Free(game.level);
        game.level = NULL;
    }
    if (game.world) {
        World_Destroy(game.world);
        game.world = NULL;
    }
    
    if (game.texUI_Working) { HQC_Artist_FreeTexture(game.texUI_Working); game.texUI_Working = NULL; }
    if (game.texUI_Menu) { HQC_Artist_FreeTexture(game.texUI_Menu); game.texUI_Menu = NULL; }
    if (game.texUI_LevelName) { HQC_Artist_FreeTexture(game.texUI_LevelName); game.texUI_LevelName = NULL; }
}

HScene Scene_Register_Game() {
    return Scene_New("game", Game_Start_, Game_Update__, Game_Draw__, Game_Free_);
}