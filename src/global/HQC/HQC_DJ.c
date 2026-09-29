#include "../HQC.h"

#include <bass.h>

#ifdef ZUMA_NO_BASSFX
    #include <math.h>        // powf：无 BASS_FX 时用采样率变调
#else
    #include <bass_fx.h>
#endif

// ── 音频开关与音量（2026-09-29 重写）───────────────────────────────────────
// 旧实现：HQC_DJ_LoadMusic 是空函数体（缺 return），音乐永远加载不出来；
// PlayMusic/StopMusic 空实现 → 全程无声。BASS_Init 失败还会致命退出（无头无法跑）。
// 现在：可关闭（无头/CI）、有主音量、支持 .mo3 多曲（MOD order 切曲 = MUS_* 常量）。

static bool  audioEnabled  = true;
static float masterVolume  = 0.35f;
static float _soundPitch   = 0;

bool HQC_Audio_IsEnabled() {
    return audioEnabled;
}

void HQC_Audio_SetEnabled(bool enabled) {
    audioEnabled = enabled;
}

float HQC_DJ_GetMasterVolume() {
    return masterVolume;
}

void HQC_DJ_SetMasterVolume(float volume) {
    masterVolume = volume;
    if (masterVolume < 0) masterVolume = 0;
    if (masterVolume > 1) masterVolume = 1;
}


HQC_Sound HQC_DJ_LoadSound(const char* filepath) {
    if (!audioEnabled)
        return NULL;

#ifdef ZUMA_NO_BASSFX
    // 没有 BASS_FX 的平台（Windows 版只带 bass.dll）：直接建**可播放**流
    //（原路径是 DECODE 流 + BASS_FX_TempoCreate 转成 tempo 流，那套依赖 bass_fx.dll）
    HSTREAM sound = BASS_StreamCreateFile(FALSE, filepath, 0, 0, 0);
    if (!sound) {
        HQC_Log("Audio: cannot open sound %s [%d]", filepath, BASS_ErrorGetCode());
        return NULL;
    }
#else
    HSTREAM sound = BASS_StreamCreateFile(FALSE, filepath, 0, 0, BASS_STREAM_DECODE);
    if (!sound) {
        HQC_Log("Audio: cannot open sound %s [%d]", filepath, BASS_ErrorGetCode());
        return NULL;
    }

    sound = BASS_FX_TempoCreate(sound, BASS_SAMPLE_FX);
    if (!sound) {
        HQC_Log("Audio: tempo create failed for %s [%d]", filepath, BASS_ErrorGetCode());
        return NULL;
    }
#endif

    HSTREAM* out = HQC_Memory_Allocate(sizeof(*out));
    *out = sound;

    return out;
}


void HQC_DJ_SetSoundPith(float pitch) {
    _soundPitch = pitch;
}


void HQC_DJ_PlaySoundPitch(HQC_Sound sound, float semitones) {
    if (!audioEnabled || !sound)
        return;

    HSTREAM snd = *((HSTREAM*)sound);

    float total = _soundPitch + semitones;

#ifdef ZUMA_NO_BASSFX
    // 没有 BASS_FX：用采样率变调（BASS 核心自带）。副作用是同时改变时长，
    // 对短音效（爆炸/宝石）听感可接受；Linux 走 BASS_FX 的 TEMPO_PITCH 不受影响。
    BASS_CHANNELINFO info;

    if (BASS_ChannelGetInfo(snd, &info) && info.freq > 0) {
        float freq = (float)info.freq * powf(2.0f, total / 12.0f);
        BASS_ChannelSetAttribute(snd, BASS_ATTRIB_FREQ, freq);
    }
#else
    BASS_ChannelSetAttribute(snd, BASS_ATTRIB_TEMPO_PITCH, total);
#endif

    BASS_ChannelSetAttribute(snd, BASS_ATTRIB_VOL, masterVolume);
    BASS_ChannelPlay(snd, TRUE);
}


void HQC_DJ_PlaySound(HQC_Sound sound) {
    HQC_DJ_PlaySoundPitch(sound, 0.0f);
}


void HQC_DJ_StopSound(HQC_Sound sound) {
    if (!audioEnabled || !sound)
        return;

    HSTREAM snd = *((HSTREAM*)sound);
    BASS_ChannelStop(snd);
}


///////////////////////////////////////////////////////////////////////////////
// 音乐：content/music/zuma.mo3 是单个 MOD 音乐文件，内含多首曲子
// （原始游戏用 order 切曲；ResourceStore 的 MUS_* 枚举就是 order 号）。
///////////////////////////////////////////////////////////////////////////////

typedef struct Music_ {
    HMUSIC handle;
    int    currentOrder;
} Music_;

static Music_* currentMusic = NULL;


HQC_Music HQC_DJ_LoadMusic(const char* filepath) {
    if (!audioEnabled)
        return NULL;

    HMUSIC handle = BASS_MusicLoad(
        FALSE, filepath, 0, 0,
        BASS_MUSIC_LOOP | BASS_MUSIC_RAMP | BASS_MUSIC_PRESCAN,
        44100);

    if (!handle) {
        // 有些构建里 .mo3 走流式解码
        HSTREAM stream = BASS_StreamCreateFile(FALSE, filepath, 0, 0, BASS_SAMPLE_LOOP);
        if (!stream) {
            HQC_Log("Audio: cannot load music %s [%d]", filepath, BASS_ErrorGetCode());
            return NULL;
        }

        Music_* music = HQC_Memory_Allocate(sizeof(*music));
        music->handle = (HMUSIC)stream;   // 复用字段存 stream（用 BASS_Channel* 通用 API 操作）
        music->currentOrder = -1;
        HQC_Log("Audio: loaded music as stream %s", filepath);
        return music;
    }

    Music_* music = HQC_Memory_Allocate(sizeof(*music));
    music->handle = handle;
    music->currentOrder = -1;

    HQC_Log("Audio: loaded music %s", filepath);
    return music;
}


void HQC_DJ_PlayMusicOrder(HQC_Music hmusic, int order) {
    if (!audioEnabled || !hmusic)
        return;

    Music_* music = (Music_*)hmusic;

    if (music->currentOrder == order && BASS_ChannelIsActive(music->handle) == BASS_ACTIVE_PLAYING)
        return;

    // MOD order 定位（zuma.mo3 内每首曲子一个 order 区间）
    if (!BASS_ChannelSetPosition(music->handle, MAKELONG(order, 0), BASS_POS_MUSIC_ORDER)) {
        // 非 MOD 音乐：忽略 order，从头播放
        BASS_ChannelSetPosition(music->handle, 0, BASS_POS_BYTE);
    }

    BASS_ChannelSetAttribute(music->handle, BASS_ATTRIB_VOL, masterVolume);
    BASS_ChannelPlay(music->handle, FALSE);

    music->currentOrder = order;
    currentMusic = music;
}


void HQC_DJ_StopMusic(HQC_Music hmusic) {
    if (!audioEnabled || !hmusic)
        return;

    Music_* music = (Music_*)hmusic;
    BASS_ChannelStop(music->handle);
    music->currentOrder = -1;
}