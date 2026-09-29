#include "Level.h"

#include <math.h>

typedef struct GauntLevelProgression {
    const char*                 id;
    HQC_VECTOR(LevelSettings)   settingsList;
    HQC_VECTOR(int)             difficultyList;
} GauntLevelProgression;


typedef struct GauntLevel {
    LevelGraphics*           graphics;
    GauntLevelProgression*   progression;
} GauntLevel;


typedef struct StageLevel {
    LevelGraphics*      graphics;
    LevelSettings*      difficulty;
} StageLevel;


typedef struct StageProgression {
    HQC_VECTOR(StageLevel) stageList;
} StageProgression;

//////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////

typedef struct CurveDot {
    float       x,  y;
    bool        t1, t2;
} CurveDot;


typedef struct Curve {
    HQC_VECTOR(CurveDot) dotList;
    v2f_t                startPosition;
} Curve;


static Curve* _CurveLoadFromFile(const char* filepath) {
    HQC_File file = HQC_File_Open(filepath, "rb");
    
    if (!file) {
        HQC_Log("Failed to open curve file: %s", filepath);
        return NULL;
    }

    // Skip header section
    HQC_File_Seek(file, 0x10);
    
    // Skip first part of file
    uint32_t count = HQC_File_ReadInt32(file);
    HQC_File_Seek(file, 0x14 + count * 10);

    uint32_t curveLength = HQC_File_ReadInt32(file);

    Curve* curve = HQC_Memory_Allocate(sizeof(*curve));
    if (!curve) {
        HQC_Log("Failed to allocate memory for curve");
        HQC_File_Close(file);
        return NULL;
    }
    
    curve->startPosition.x = HQC_File_ReadFloat(file);
    curve->startPosition.y = HQC_File_ReadFloat(file);
    curve->dotList         = HQC_Container_CreateVector(sizeof(CurveDot));

    float xprev = curve->startPosition.x;
    float yprev = curve->startPosition.y;

    for (uint32_t i = 0; i < curveLength; i++) {
        CurveDot curveDot;

        curveDot.t1 = HQC_File_ReadByte(file);
        curveDot.t2 = HQC_File_ReadByte(file);

        char dx = (char)HQC_File_ReadByte(file);
        char dy = (char)HQC_File_ReadByte(file);

        xprev += dx / 100.0f;
        yprev += dy / 100.0f;

        curveDot.x = xprev;
        curveDot.y = yprev;

        HQC_Container_VectorAdd(curve->dotList, &curveDot);
    }

    HQC_File_Close(file);

    HQC_Log("Curve loaded: %s (%u dots)", filepath, curveLength);

    return curve;
}

static void _CurveFree(Curve* curve) {
    if (!curve) return;
    HQC_Container_FreeVector(curve->dotList);
    HQC_Memory_Free(curve);
}

//////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////

typedef struct Level {
    LevelSettings* settings;
    LevelGraphics* graphics;

    HQC_Texture texture;
    HQC_Texture textureTopLevel;

    Curve* curveA;
    Curve* curveB;
} Level;


HLevel Level_Load(LevelSettings* settings, LevelGraphics* graphics) {
    Level* level = HQC_Memory_Allocate(sizeof(*level));
    if (!level) {
        HQC_Log("Failed to allocate memory for level");
        return NULL;
    }

    level->settings = settings;
    level->graphics = graphics;

    HQC_Log("Loading level texture: %s", graphics->textureFile);
    level->texture = HQC_Artist_LoadTexture(graphics->textureFile);
    if (!level->texture) {
        HQC_Log("Failed to load level texture: %s", graphics->textureFile);
        HQC_Memory_Free(level);
        return NULL;
    }
    
    level->textureTopLevel = (graphics->textureTopLayerFile != NULL) ? 
                                HQC_Artist_LoadTexture(graphics->textureTopLayerFile) :
                                NULL;

    HQC_Log("Loading curve A: %s", graphics->curveAFile);
    level->curveA = _CurveLoadFromFile(graphics->curveAFile);
    if (!level->curveA) {
        HQC_Log("Failed to load curve A: %s", graphics->curveAFile);
        if (level->texture) HQC_Artist_FreeTexture(level->texture);
        if (level->textureTopLevel) HQC_Artist_FreeTexture(level->textureTopLevel);
        HQC_Memory_Free(level);
        return NULL;
    }
    
    level->curveB = (graphics->curveBFile != NULL) ? 
                        _CurveLoadFromFile(graphics->curveBFile) :
                        NULL;

    HQC_Log("Level loaded successfully");
    return level;
}

void Level_Free(HLevel hlevel) {
    Level* level = (Level*)hlevel;
    if (!level) return;

    if (level->texture) HQC_Artist_FreeTexture(level->texture);
    if (level->textureTopLevel) HQC_Artist_FreeTexture(level->textureTopLevel);
    
    _CurveFree(level->curveA);
    _CurveFree(level->curveB);
    
    HQC_Memory_Free(level);
}


void Level_Draw(HLevel hlevel, float x, float y) {
    Level* level = (Level*)hlevel;
    
    if (!level || !level->texture)
        return;

    HQC_Artist_DrawTexture(level->texture, x, y);
}


void Level_DrawTopLayer(HLevel hlevel, float x, float y) {
    Level* level = (Level*)hlevel;

    if (!level || !level->textureTopLevel)
        return;

    HQC_Artist_DrawTexture(level->textureTopLevel, x, y);
}


const char* Level_GetDisplayName(HLevel hlevel) {
    Level* level = (Level*)hlevel;
    if (!level || !level->graphics) return "unknown";

    return level->graphics->dispName;
}


LevelSettings* Level_GetSettings(HLevel hlevel) {
    Level* level = (Level*)hlevel;
    return level ? level->settings : NULL;
}


LevelGraphics* Level_GetGraphics(HLevel hlevel) {
    Level* level = (Level*)hlevel;
    return level ? level->graphics : NULL;
}


int Level_GetCurveLength(HLevel hlevel) {
    Level* level = (Level*)hlevel;
    if (!level || !level->curveA) return 0;

    return (int)HQC_Container_VectorCount(level->curveA->dotList);
}


v2f_t Level_GetCurveCoords(HLevel hlevel, float pos) {
    Level* level = (Level*)hlevel;

    if (!level || !level->curveA)
        return v2f_t_default;

    int len = Level_GetCurveLength(hlevel);
    if ((int)pos > len-1) pos = len-1;
    if (pos <= 0.0f) pos = 0.0f;

    CurveDot* cdot = (CurveDot*)HQC_Container_VectorGet(level->curveA->dotList, (int)pos);
    v2f_t coords = { (cdot->x+104) * 1.5, cdot->y * 1.5 };

    return coords;
}


v2f_t Level_GetCurveDirection(HLevel hlevel, float pos) {
    Level* level = (Level*)hlevel;

    v2f_t dir = { 1.0f, 0.0f };
    if (!level || !level->curveA) return dir;

    v2f_t a = Level_GetCurveCoords(hlevel, pos);
    v2f_t b = Level_GetCurveCoords(hlevel, pos + 1.0f);

    float dx = b.x - a.x;
    float dy = b.y - a.y;
    float len = sqrtf(dx*dx + dy*dy);

    if (len < 0.0001f) {
        dir.x = 1.0f; dir.y = 0.0f;
        return dir;
    }

    dir.x = dx / len;
    dir.y = dy / len;

    return dir;
}


void Level_GetCurveFlags(HLevel hlevel, float pos, int* isTunnel, int* isTopPriority) {
    Level* level = (Level*)hlevel;

    if (isTunnel)      *isTunnel = 0;
    if (isTopPriority) *isTopPriority = 0;

    if (!level || !level->curveA) return;

    int len = Level_GetCurveLength(hlevel);
    int idx = (int)pos;
    if (idx < 0) idx = 0;
    if (idx > len-1) idx = len-1;

    CurveDot* cdot = (CurveDot*)HQC_Container_VectorGet(level->curveA->dotList, idx);

    if (isTunnel)      *isTunnel = cdot->t1;
    if (isTopPriority) *isTopPriority = cdot->t2;
}


v2f_t Level_GetPitPos(HLevel hlevel) {
    int len = Level_GetCurveLength(hlevel);
    if (len <= 0) return v2f_t_default;

    return Level_GetCurveCoords(hlevel, (float)(len - 1));
}


int Level_GetTreasureCount(HLevel hlevel) {
    Level* level = (Level*)hlevel;
    if (!level || !level->graphics || !level->graphics->coinsPosList) return 0;

    return (int)HQC_Container_VectorCount(level->graphics->coinsPosList);
}


v2f_t Level_GetTreasurePos(HLevel hlevel, int index) {
    Level* level = (Level*)hlevel;

    if (!level || !level->graphics || !level->graphics->coinsPosList)
        return v2f_t_default;

    int count = Level_GetTreasureCount(hlevel);
    if (count <= 0) return v2f_t_default;
    if (index < 0) index = 0;
    if (index >= count) index = count - 1;

    v2f_t* p = (v2f_t*)HQC_Container_VectorGet(level->graphics->coinsPosList, index);

    // levels.xml 的坐标与曲线同一坐标系（原版 +104 / *1.5 的换算）
    v2f_t pos = { (p->x + 104) * 1.5f, p->y * 1.5f };
    return pos;
}