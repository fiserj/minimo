#pragma once

#ifdef __cplusplus
extern "C" {
#endif

enum
{
    WINDOW_DEFAULT      = 0,
    WINDOW_FIXED_SIZE   = 1,
    WINDOW_FIXED_ASPECT = 2,
    WINDOW_FULL_SCREEN  = 4,
};

void size(int width, int height, int flags);

void vsync(int vsync);

void title(const char* title);

int width(void);

int height(void);

float aspect(void);

float dpi(void);

void quit(void);

#ifdef __cplusplus
}
#endif
