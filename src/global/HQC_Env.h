#pragma once

// 可移植的环境变量设置：POSIX 的 setenv / Windows 的 _putenv_s
//   （Windows 上 setenv 不存在，mingw 会报 implicit declaration of function 'setenv'）
#include <stdlib.h>

static inline int HQC_Env_Set(const char* name, const char* value, int overwrite) {
#ifdef _WIN32
    if (!overwrite && getenv(name))
        return 0;

    return _putenv_s(name, value);
#else
    return setenv(name, value, overwrite);
#endif
}