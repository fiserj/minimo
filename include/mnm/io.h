#pragma once

#ifdef __cplusplus
extern "C" {
#endif

enum
{
    MOUSE_LEFT,
    MOUSE_MIDDLE,
    MOUSE_RIGHT,
};

enum
{
    KEY_ANY,

    KEY_DOWN,
    KEY_ENTER,
    KEY_ESCAPE,
    KEY_LEFT,
    KEY_RIGHT,
    KEY_TAB,
    KEY_UP,
};

int mouse_x(void);

int mouse_y(void);

int mouse_dx(void);

int mouse_dy(void);

int mouse_down(int button);

int mouse_held(int button);

int mouse_up(int _button);

int key_down(int key);

int key_held(int key);

int key_up(int key);

#ifdef __cplusplus
}
#endif
