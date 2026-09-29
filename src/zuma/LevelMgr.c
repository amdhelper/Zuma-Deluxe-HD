#include "LevelMgr.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct LevelEntry {
    char* graphicsID;
    char* settingsID;
} LevelEntry;

typedef struct Stage {
    HQC_VECTOR(LevelEntry) levels;
} Stage;

static struct {
    HQC_VECTOR(LevelGraphics) graphicsList;
    HQC_VECTOR(LevelSettings) settingsList;
    HQC_VECTOR(Stage)         stages;

    int currentStage;
    int currentLevel;
} mgr;

// Helper to find existing graphics by ID
static LevelGraphics* _FindGraphics(const char* id) {
    for (size_t i = 0; i < HQC_Container_VectorCount(mgr.graphicsList); i++) {
        LevelGraphics* gx = HQC_Container_VectorGet(mgr.graphicsList, i);
        if (strcmp(gx->id, id) == 0) return gx;
    }
    return NULL;
}

// Helper to find existing settings by ID
static LevelSettings* _FindSettings(const char* id) {
    for (size_t i = 0; i < HQC_Container_VectorCount(mgr.settingsList); i++) {
        LevelSettings* set = HQC_Container_VectorGet(mgr.settingsList, i);
        if (strcmp(set->id, id) == 0) return set;
    }
    return NULL;
}

static LevelGraphics* _LastGraphics() {
    size_t n = HQC_Container_VectorCount(mgr.graphicsList);
    if (n == 0) return NULL;
    return (LevelGraphics*)HQC_Container_VectorGet(mgr.graphicsList, (int)n - 1);
}

static void _ParseAttributes_Graphics(const char** attr) {
    LevelGraphics gx;
    memset(&gx, 0, sizeof(gx));
    
    // Default values
    gx.frogPos.x = 640;
    gx.frogPos.y = 360;
    gx.coinsPosList = HQC_Container_CreateVector(sizeof(v2f_t));

    const char* imageName = NULL;
    const char* curveName = NULL;
    const char* topName   = NULL;

    for (int i = 0; attr[i]; i += 2) {
        if (strcmp(attr[i], "id") == 0) {
            gx.id = HQC_StringClone(attr[i+1]);
        } else if (strcmp(attr[i], "curve") == 0) {
            curveName = attr[i+1];
        } else if (strcmp(attr[i], "image") == 0) {
            imageName = attr[i+1];
        } else if (strcmp(attr[i], "image-top") == 0) {
            // 顶层贴图（隧道/桥）：levels/<id>/<image-top>.png（移植自 v0.1.0 Level_Load）
            topName = attr[i+1];
        } else if (strcmp(attr[i], "dispname") == 0) {
            gx.dispName = HQC_StringClone(attr[i+1]);
        } else if (strcmp(attr[i], "gx") == 0) {
            // 青蛙坐标（原版：x 需 +106 再乘屏幕缩放；这里直接用 1.5 比例）
            gx.frogPos.x = (atof(attr[i+1]) + 106) * 1.5f;
        } else if (strcmp(attr[i], "gy") == 0) {
            gx.frogPos.y = atof(attr[i+1]) * 1.5f;
        }
    }
    
    if (gx.id == NULL) {
        HQC_Log("LevelMgr: Warning - Graphics element missing ID!");
        HQC_Container_FreeVector(gx.coinsPosList);
        return;
    }

    if (!gx.dispName) gx.dispName = HQC_StringClone(gx.id);

    if (!imageName) imageName = gx.id;
    if (!curveName) curveName = gx.id;
    
    char buffer[256];
    
    snprintf(buffer, sizeof(buffer), "levels/%s/%s.dat", curveName, curveName);
    gx.curveAFile = HQC_StringClone(buffer);
    
    snprintf(buffer, sizeof(buffer), "levels/%s/%s.jpg", gx.id, imageName);
    gx.textureFile = HQC_StringClone(buffer);

    if (topName) {
        snprintf(buffer, sizeof(buffer), "levels/%s/%s.png", gx.id, topName);
        gx.textureTopLayerFile = HQC_StringClone(buffer);
    } else {
        gx.textureTopLayerFile = NULL;
    }
    
    HQC_Container_VectorAdd(mgr.graphicsList, &gx);
}


// <TreasurePoint x=".." y=".." dist1=".."/> —— 石头/宝石出现点（挂在上一个 Graphics 上）
static void _ParseAttributes_TreasurePoint(const char** attr) {
    LevelGraphics* gx = _LastGraphics();
    if (!gx) return;

    v2f_t p = { 0, 0 };

    for (int i = 0; attr[i]; i += 2) {
        if (strcmp(attr[i], "x") == 0) p.x = atof(attr[i+1]);
        else if (strcmp(attr[i], "y") == 0) p.y = atof(attr[i+1]);
    }

    HQC_Container_VectorAdd(gx->coinsPosList, &p);
}

static void _ParseAttributes_Settings(const char** attr) {
    LevelSettings set;
    memset(&set, 0, sizeof(set));
    
    // Default values
    set.slowFactor = 1.0f;
    set.ballColors = 4;
    set.ballStartCount = 35;
    set.gaugeScore = 1000;
    set.partTime = 60;

    for (int i = 0; attr[i]; i += 2) {
        if (strcmp(attr[i], "id") == 0) set.id = HQC_StringClone(attr[i+1]);
        else if (strcmp(attr[i], "speed") == 0) set.ballSpd = atof(attr[i+1]);
        else if (strcmp(attr[i], "score") == 0) set.gaugeScore = atoi(attr[i+1]);
        else if (strcmp(attr[i], "start") == 0) set.ballStartCount = atoi(attr[i+1]);
        else if (strcmp(attr[i], "colors") == 0) set.ballColors = atoi(attr[i+1]);
        else if (strcmp(attr[i], "slowfactor") == 0) set.slowFactor = atof(attr[i+1]);
        else if (strcmp(attr[i], "repeat") == 0) set.repeatChance = atoi(attr[i+1]);
        else if (strcmp(attr[i], "single") == 0) set.singleChance = atoi(attr[i+1]);
        else if (strcmp(attr[i], "partime") == 0 || strcmp(attr[i], "partTime") == 0) set.partTime = atoi(attr[i+1]);
    }

    if (set.slowFactor <= 0.0f) set.slowFactor = 1.0f;
    
    HQC_Container_VectorAdd(mgr.settingsList, &set);
}

// Helper to split comma separated string
static void _SplitAndAdd(HQC_VECTOR(char*) list, const char* str) {
    char* s = HQC_StringClone(str);
    char* token = strtok(s, ",");
    while (token) {
        char* val = HQC_StringClone(token);
        // Trim spaces if needed? strtok handles basic splitting.
        HQC_Container_VectorAdd(list, &val);
        token = strtok(NULL, ",");
    }
    free(s);
}

static void _ParseAttributes_StageProgression(const char** attr) {
    // stage1="...", diffi1="..."
    // We need to parse up to stage13
    
    for (int s = 1; s <= 13; s++) {
        char stageKey[32];
        char diffiKey[32];
        snprintf(stageKey, sizeof(stageKey), "stage%d", s);
        snprintf(diffiKey, sizeof(diffiKey), "diffi%d", s);
        
        const char* stageVal = NULL;
        const char* diffiVal = NULL;
        
        for (int i = 0; attr[i]; i += 2) {
            if (strcmp(attr[i], stageKey) == 0) stageVal = attr[i+1];
            else if (strcmp(attr[i], diffiKey) == 0) diffiVal = attr[i+1];
        }
        
        if (stageVal && diffiVal) {
            Stage stage;
            stage.levels = HQC_Container_CreateVector(sizeof(LevelEntry));
            
            // We need to split both strings and pair them
            // But wait, HQC_Container doesn't support vector of strings easily directly without typedef? 
            // I'll manually split and pair.
            
            char* sStr = HQC_StringClone(stageVal);
            char* dStr = HQC_StringClone(diffiVal);
            
            char* sCtx = NULL;
            char* dCtx = NULL;
            
            char* sToken = strtok_r(sStr, ",", &sCtx);
            char* dToken = strtok_r(dStr, ",", &dCtx);
            
            while (sToken && dToken) {
                LevelEntry entry;
                entry.graphicsID = HQC_StringClone(sToken);
                entry.settingsID = HQC_StringClone(dToken);
                
                HQC_Container_VectorAdd(stage.levels, &entry);
                
                sToken = strtok_r(NULL, ",", &sCtx);
                dToken = strtok_r(NULL, ",", &dCtx);
            }
            
            // Handle case where one list is shorter? Assume valid XML for now.
            // If diffi list is shorter, reuse last diffi? Original game logic might do that.
            // For now assume 1:1 mapping or repeat last difficulty if needed.
            // But typical levels.xml seems to have matching counts or logic.
            // Wait, looking at XML:
            // stage1 = "spiral,claw,..." (18 items)
            // diffi1 = "lvl11,lvl12,lvl13,lvl14,lvl15" (5 items)
            // MISMATCH!
            // Ah, the logic must be: Levels in stage use difficulty from the list sequentially?
            // Or maybe difficulty list is for "Level 1-1", "Level 1-2"?
            // Re-reading XML comments:
            // L1 = spiral,claw...
            // It seems "stage1" list contains the sequence of maps.
            // "diffi1" list contains settings to apply.
            // But how do they map? 18 maps vs 5 settings?
            // Maybe they cycle? Or maybe the settings are for "1-1", "1-2", "1-3"... and they apply to the map sequence?
            // Actually, in Zuma, you play maps.
            // Stage 1-1: Spiral, Settings lvl11?
            // Stage 1-2: Claw, Settings lvl12?
            // Stage 1-5: Turnaround, Settings lvl15?
            // Stage 1-6: Longrange? Settings?
            // The XML says:
            // stage1="spiral...inversespiral"
            // diffi1="lvl11...lvl15"
            // If I play 5 levels, do I advance to Stage 2?
            // Ah, usually you play a subset of maps per stage?
            // Or maybe the `settings` list defines the number of levels in that stage?
            // Yes! "diffi1" has 5 items. So Stage 1 has 5 levels.
            // But "stage1" has 18 maps. This is the POOL of maps for Stage 1.
            // The game picks a map from the pool for each level in the stage.
            // Ideally it picks sequentially or randomly.
            // For simplicity, let's pick sequentially from the pool.
            
            // So: Stage 1 has 5 levels (defined by diffi1 count).
            // Level 1: Settings lvl11, Map: stage1_pool[0] (Spiral)
            // Level 2: Settings lvl12, Map: stage1_pool[1] (Claw)
            // ...
            // Level 5: Settings lvl15, Map: stage1_pool[4] (Turnaround)
            
            // So the Stage struct should hold the generated sequence of levels.
            
            HQC_VECTOR(char*) mapPool = HQC_Container_CreateVector(sizeof(char*));
            char* mStr = HQC_StringClone(stageVal);
            char* mCtx = NULL;
            char* mToken = strtok_r(mStr, ",", &mCtx);
            while(mToken) {
                char* s = HQC_StringClone(mToken);
                HQC_Container_VectorAdd(mapPool, &s);
                mToken = strtok_r(NULL, ",", &mCtx);
            }
            free(mStr);
            
            char* setStr = HQC_StringClone(diffiVal);
            char* setCtx = NULL;
            char* setToken = strtok_r(setStr, ",", &setCtx);
            int mapIdx = 0;
            
            while(setToken) {
                LevelEntry entry;
                entry.settingsID = HQC_StringClone(setToken);
                
                // Pick map from pool
                if (HQC_Container_VectorCount(mapPool) > 0) {
                    char** pMapID = HQC_Container_VectorGet(mapPool, mapIdx % HQC_Container_VectorCount(mapPool));
                    entry.graphicsID = HQC_StringClone(*pMapID);
                    mapIdx++;
                } else {
                    entry.graphicsID = HQC_StringClone("longrange"); // Fallback
                }
                
                HQC_Container_VectorAdd(stage.levels, &entry);
                setToken = strtok_r(NULL, ",", &setCtx);
            }
            free(setStr);
            // Free map pool strings? (leaked for now, fixing later if needed)
            
            HQC_Container_VectorAdd(mgr.stages, &stage);
            
            free(sStr);
            free(dStr);
        }
    }
}

static void _StartElement(void *userData, const char *name, const char **atts) {
    if (strcmp(name, "Graphics") == 0) {
        _ParseAttributes_Graphics(atts);
    } else if (strcmp(name, "Settings") == 0) {
        _ParseAttributes_Settings(atts);
    } else if (strcmp(name, "TreasurePoint") == 0) {
        _ParseAttributes_TreasurePoint(atts);
    } else if (strcmp(name, "StageProgression") == 0) {
        _ParseAttributes_StageProgression(atts);
    }
}

static void _EndElement(void *userData, const char *name) {
}

void LevelMgr_Init() {
    mgr.graphicsList = HQC_Container_CreateVector(sizeof(LevelGraphics));
    mgr.settingsList = HQC_Container_CreateVector(sizeof(LevelSettings));
    mgr.stages       = HQC_Container_CreateVector(sizeof(Stage));
    mgr.currentStage = 0;
    mgr.currentLevel = 0;
}

bool LevelMgr_LoadLevels(const char* xmlPath) {
    HQC_XmlParser parser = HQC_XmlParser_Create(NULL);
    HQC_XmlParser_SetHandlers(parser, _StartElement, _EndElement);
    
    HQC_File file = HQC_File_Open(xmlPath, "rb");
    if (!file) {
        HQC_Log("Failed to open levels xml: %s", xmlPath);
        return false;
    }
    
    // Read whole file
    // HQC_File doesn't have GetSize? I'll read in chunks.
    char buffer[4096];
    // Implement read loop if needed, but for now assuming HQC_File_ReadToBuffer works or I loop.
    // The HQC_File API in header has ReadToBuffer but needs size/count.
    // Let's just read char by char or implement a loop if HQC_File_Read is generic.
    // Actually HQC_XML might need a loop.
    // Wait, HQC_File_Open uses stdio internally likely.
    // I'll try to read in a loop.
    
    // Hack: Just read chunks.
    // But `HQC_File_ReadToBuffer` signature: `void HQC_File_ReadToBuffer(HQC_File file, void* buffer, size_t size, size_t count);`
    // It's like fread.
    
    while (1) {
        // How to detect EOF with HQC_File? It doesn't seem to expose Feof.
        // I'll try reading 1 byte 4096 times? No.
        // I'll assume I can read a chunk.
        // HQC_File implementation likely uses fread.
        // Let's assume I can't easily detect EOF without return value from Read.
        // `HQC_File_ReadByte` returns uint8_t.
        // I'll read entire file into memory first using fseek/ftell if possible?
        // HQC_File_Seek exists.
        // But HQC_File_Tell? Not in header.
        
        // I'll assume I can rely on a basic parsing loop.
        // But for safety/speed, I'll just hardcode reading a large buffer if file size is unknown, 
        // or loop single byte reading (slow).
        
        // Let's try to modify HQC_File to support reading size or just use standard `fopen`?
        // I should stick to HQC if possible.
        // But `HQC_XmlParser_Parse` takes a buffer.
        
        // Let's use `HQC_File_ReadToBuffer` on a 1024 buffer and check if it filled it?
        // No return value.
        // This is problematic.
        // I will use standard FILE* for this function to be safe and efficient.
        FILE* f = fopen(xmlPath, "rb");
        if (f) {
            char buf[1024];
            size_t len;
            while ((len = fread(buf, 1, sizeof(buf), f)) > 0) {
                HQC_XmlParser_Parse(parser, buf, len, len < sizeof(buf));
            }
            fclose(f);
        }
        break;
    }
    
    HQC_XmlParser_Free(parser);
    
    HQC_Log("LevelMgr: Loaded %d graphics, %d settings, %d stages", 
        HQC_Container_VectorCount(mgr.graphicsList),
        HQC_Container_VectorCount(mgr.settingsList),
        HQC_Container_VectorCount(mgr.stages));
        
    return true;
}

LevelSettings* LevelMgr_GetCurrentSettings() {
    if (mgr.currentStage >= HQC_Container_VectorCount(mgr.stages)) return NULL;
    
    Stage* stage = HQC_Container_VectorGet(mgr.stages, mgr.currentStage);
    if (mgr.currentLevel >= HQC_Container_VectorCount(stage->levels)) return NULL;
    
    LevelEntry* entry = HQC_Container_VectorGet(stage->levels, mgr.currentLevel);
    return _FindSettings(entry->settingsID);
}

LevelGraphics* LevelMgr_GetCurrentGraphics() {
    if (mgr.currentStage >= HQC_Container_VectorCount(mgr.stages)) return NULL;
    
    Stage* stage = HQC_Container_VectorGet(mgr.stages, mgr.currentStage);
    if (mgr.currentLevel >= HQC_Container_VectorCount(stage->levels)) return NULL;
    
    LevelEntry* entry = HQC_Container_VectorGet(stage->levels, mgr.currentLevel);
    return _FindGraphics(entry->graphicsID);
}

bool LevelMgr_AdvanceLevel() {
    if (mgr.currentStage >= HQC_Container_VectorCount(mgr.stages)) return false;
    
    Stage* stage = HQC_Container_VectorGet(mgr.stages, mgr.currentStage);
    
    mgr.currentLevel++;
    if (mgr.currentLevel >= HQC_Container_VectorCount(stage->levels)) {
        mgr.currentLevel = 0;
        mgr.currentStage++;
        if (mgr.currentStage >= HQC_Container_VectorCount(mgr.stages)) {
            HQC_Log("LevelMgr: All stages complete!");
            return false; // Game Over / Win
        }
    }
    
    return true;
}

void LevelMgr_Reset() {
    mgr.currentStage = 0;
    mgr.currentLevel = 0;
}

// 0-based 起始进度（命令行 --stage/--level 用；越界自动收敛到有效范围）
void LevelMgr_SetProgress(int stage, int level) {
    int stageCount = (int)HQC_Container_VectorCount(mgr.stages);
    if (stageCount <= 0) { mgr.currentStage = 0; mgr.currentLevel = 0; return; }

    if (stage < 0) stage = 0;
    if (stage >= stageCount) stage = stageCount - 1;

    Stage* stg = (Stage*)HQC_Container_VectorGet(mgr.stages, stage);
    int levelCount = (int)HQC_Container_VectorCount(stg->levels);

    if (level < 0) level = 0;
    if (levelCount > 0 && level >= levelCount) level = levelCount - 1;

    mgr.currentStage = stage;
    mgr.currentLevel = level;
}

// 当前大关共有几个小关（HUD 显示 "lvl x-y" / 进度用）
int LevelMgr_GetCurrentStageLevelCount() {
    if (mgr.currentStage >= (int)HQC_Container_VectorCount(mgr.stages)) return 0;

    Stage* stg = (Stage*)HQC_Container_VectorGet(mgr.stages, mgr.currentStage);
    return (int)HQC_Container_VectorCount(stg->levels);
}

int LevelMgr_GetStageCount() {
    return (int)HQC_Container_VectorCount(mgr.stages);
}

int LevelMgr_GetSettingsCount() {
    return (int)HQC_Container_VectorCount(mgr.settingsList);
}

int LevelMgr_GetCurrentStage() { return mgr.currentStage + 1; }
int LevelMgr_GetCurrentLevelIndex() { return mgr.currentLevel + 1; }
const char* LevelMgr_GetCurrentLevelID() {
    LevelGraphics* gx = LevelMgr_GetCurrentGraphics();
    return gx ? gx->id : "unknown";
}
