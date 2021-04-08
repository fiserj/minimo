#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void begin(void);

void vertex(float x, float y, float z);

void color(unsigned int rgba);

void uv(float u, float v);

void end(void);

void model(void);

void view(void);

void projection(void);

void push(void);

void pop(void);

void identity(void);

void ortho(float left, float right, float bottom, float top, float near, float far);

void perspective(float fovy, float aspect, float near, float far);

void rotate(float angle, float x, float y, float z);

void scale(float x, float y, float z);

void translate(float x, float y, float z);

#ifdef __cplusplus
}
#endif
