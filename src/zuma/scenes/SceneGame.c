#include "Index.h"

#include "../Level.h"
#include "../Frog.h"
#include "../BallChain.h"
#include "../FloatingText.h"
#include "../Statistics.h"

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
} game;

static void GoBack_() {
    Scene_Change(SC_TEST);
}

static void Game_Start_() {
    HQC_Log("Starting game scene...");
    
    // Create a simple level with just the background
    LevelSettings levelSettings;
    levelSettings.id = "test";

    LevelGraphics* levelGx = HQC_Memory_Allocate(sizeof(*levelGx));
    if (!levelGx) {
        HQC_Log("Failed to allocate memory for level graphics");
        Scene_Change(SC_MENU);
        return;
    }
    
    levelGx->dispName            = "Test Level";
    levelGx->coinsPosList        = HQC_Container_CreateVector(sizeof(v2f_t));
    levelGx->frogPos.x           = 640.0f;
    levelGx->frogPos.y           = 360.0f;
    levelGx->id                  = "longrange";
    levelGx->textureFile         = "levels/longrange/longrange.jpg";
    levelGx->textureTopLayerFile = NULL;
    levelGx->curveAFile          = "levels/longrange/longrange.dat";
    levelGx->curveBFile          = NULL;                                        

    game.level = Level_Load(&levelSettings, levelGx);
    if (!game.level) {
        HQC_Log("Failed to load level, returning to menu");
        HQC_Memory_Free(levelGx);
        Scene_Change(SC_MENU);
        return;
    }

    // Skip complex object creation for now
    game.bulletList = NULL;
    game.frog = NULL;
    game.chain = NULL;
    game.generator = NULL;
    game.world = NULL;
    
    HQC_Log("Game scene started successfully (background only mode)");
}


static void Game_Update__() {
    // Minimal update with safety checks
    if (!game.level) {
        return;
    }
    
    // Add any necessary update logic here
}


static void Game_Draw__() {
    // Draw level background if available
    if (game.level) {
        float cx = 1280.f / 2;
        float cy = 720.f  / 2;
        
        // Draw just the background texture, not the curve (which was causing crashes)
        Level* level = (Level*)game.level;
        if (level && level->texture) {
            HQC_Artist_DrawTexture(level->texture, cx, cy);
        }
    } else {
        // Fallback background
        HQC_Artist_SetColorHex(0x2C3E50);
    }
    
    // Draw UI text
    HQC_Artist_SetColorHex(0xFFFFFF);
    HQC_Font font = Store_GetFontByID(0);
    if (font) {
        HQC_Artist_DrawText(
            font, 
            "Game Scene - Working!", 
            640, 100
        );
        
        HQC_Artist_DrawText(
            font, 
            "Press M to return to menu", 
            640, 650
        );
        
        if (game.level) {
            const char* levelName = Level_GetDisplayName(game.level);
            if (levelName) {
                HQC_Artist_DrawText(
                    font, 
                    levelName, 
                    640, 140
                );
            }
        }
    }
}

static void Game_Free_() {
    if (game.world) {
        World_Destroy(game.world);
    }
}

HScene Scene_Register_Game() {
    return Scene_New("game", Game_Start_, Game_Update__, Game_Draw__, Game_Free_);
}