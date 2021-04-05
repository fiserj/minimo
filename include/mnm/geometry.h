#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void begin(void);

void vertex(float x, float y, float z);

void color(unsigned int rgba);

void uv(float u, float v);

void end(void);

#ifdef __cplusplus
}
#endif
