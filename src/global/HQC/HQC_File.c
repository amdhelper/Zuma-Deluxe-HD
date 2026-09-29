#include "../HQC.h"

#include <stdio.h>


HQC_File HQC_File_Open(const char* path, const char* mode) {
    FILE* file = fopen(path, mode);

    // ⚠️ 不再致命退出：缺文件是"可恢复"的（关卡曲线缺失 → 该关跳过/回菜单），
    //    旧行为会让任何一个素材缺失直接把整局游戏打死（Gauntlet 随机跳关时踩到过）。
    if (!file)
        HQC_Log("HQC_File_Open: Cannot open file [%s] with flags [%s]", path, mode);

    return file;
}


uint8_t HQC_File_ReadByte(HQC_File file) {
    uint8_t byte;
    HQC_File_ReadToBuffer(file, &byte, sizeof(byte), 1);
    return byte;
}


int16_t HQC_File_ReadInt16(HQC_File file) {
    int16_t buff;
    HQC_File_ReadToBuffer(file, &buff, sizeof(buff), 1);
    return buff;
}


int32_t HQC_File_ReadInt32(HQC_File file) {
    int32_t buff;
    HQC_File_ReadToBuffer(file, &buff, sizeof(buff), 1);
    return buff;
}


int64_t HQC_File_ReadInt64(HQC_File file) {
    int64_t buff;
    HQC_File_ReadToBuffer(file, &buff, sizeof(buff), 1);
    return buff;
}


float HQC_File_ReadFloat(HQC_File file) {
    float buff;
    HQC_File_ReadToBuffer(file, &buff, sizeof(buff), 1);
    return buff;
}


void HQC_File_ReadToBuffer(HQC_File file, void* buffer, size_t size, size_t count) {
    fread(buffer, size, count, (FILE*)file);
}


void HQC_File_Seek(HQC_File file, long offset) {
    fseek((FILE*)file, offset, SEEK_SET);
}


void HQC_File_Close(HQC_File file) {
    fclose((FILE*)file);
}