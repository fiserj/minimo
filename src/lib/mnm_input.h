#pragma once

namespace mnm
{

enum struct InputState : u8
{
    DOWN     = 0x01,
    UP       = 0x02,
    HELD     = 0x04,
    REPEATED = 0x08,
};

template <typename T, u16 Size>
struct InputCache
{
    static constexpr u16 INPUT_SIZE    = Size + 1;
    static constexpr u16 INVALID_INPUT = Size;

    StaticArray<u8 , INPUT_SIZE> states;
    StaticArray<f32, INPUT_SIZE> timestamps;

    inline bool is(u16 input, InputState state) const
    {
        input = T::translate_input(input);

        return states[input] & u8(state);
    }

    inline f32 held_time(u16 input, f32 timestamp) const
    {
        input = T::translate_input(input);

        if (states[input] & (u8(InputState::DOWN) | u8(InputState::HELD)))
        {
            ASSERT(timestamp >= timestamps[input]);
            return timestamp -  timestamps[input];
        }

        return -1.0f;
    }

    void update_input(u16 input, InputState state, f32 timestamp = 0.0f)
    {
        states[input] |= u8(state);

        if (state == InputState::DOWN)
        {
            timestamps[input] = timestamp;
        }
    }

    void update_states()
    {
        for (u16 i = 0; i < Size; i++)
        {
            if (states[i] & u8(InputState::UP))
            {
                states[i] = 0;
            }
            else if (states[i] & u8(InputState::DOWN))
            {
                states[i] = InputState::HELD;
            }
            else
            {
                states[i] &= ~u8(InputState::REPEATED);
            }
        }
    }
};

struct KeyboardInput : InputCache<KeyboardInput, GLFW_KEY_LAST>
{
    static u16 translate_input(u16 input)
    {
        static const u16 s_keys[] =
        {
            0,                     // KEY_ANY

            GLFW_KEY_LEFT_ALT,     // KEY_ALT_LEFT
            GLFW_KEY_RIGHT_ALT,    // KEY_ALT_RIGHT
            GLFW_KEY_BACKSPACE,    // KEY_BACKSPACE
            GLFW_KEY_LEFT_CONTROL, // KEY_CONTROL_LEFT
            GLFW_KEY_RIGHT_CONTROL, // KEY_CONTROL_RIGHT
            GLFW_KEY_DELETE,        // KEY_DELETE
            GLFW_KEY_DOWN,          // KEY_DOWN
            GLFW_KEY_ENTER,         // KEY_ENTER
            GLFW_KEY_ESCAPE,        // KEY_ESCAPE
            GLFW_KEY_LEFT,          // KEY_LEFT
            GLFW_KEY_RIGHT,         // KEY_RIGHT
            GLFW_KEY_LEFT_SHIFT,    // KEY_SHIFT_LEFT
            GLFW_KEY_RIGHT_SHIFT,   // KEY_SHIFT_RIGHT
            GLFW_KEY_SPACE,         // KEY_SPACE
            GLFW_KEY_LEFT_SUPER,    // KEY_SUPER_LEFT
            GLFW_KEY_RIGHT_SUPER,   // KEY_SUPER_RIGHT
            GLFW_KEY_TAB,           // KEY_TAB
            GLFW_KEY_UP,            // KEY_UP

            GLFW_KEY_F1,            // KEY_F1
            GLFW_KEY_F2,            // KEY_F2
            GLFW_KEY_F3,            // KEY_F3
            GLFW_KEY_F4,            // KEY_F4
            GLFW_KEY_F5,            // KEY_F5
            GLFW_KEY_F6,            // KEY_F6
            GLFW_KEY_F7,            // KEY_F7
            GLFW_KEY_F8,            // KEY_F8
            GLFW_KEY_F9,            // KEY_F9
            GLFW_KEY_F10,           // KEY_F10
            GLFW_KEY_F11,           // KEY_F11
            GLFW_KEY_F12,           // KEY_F12
        };

        if (input < int(BX_COUNTOF(s_keys)))
        {
            input = s_keys[input];
        }
        else if ((input >= 'A' && input <= 'Z'))
        {
            input = input - 'A' + GLFW_KEY_A;
        }
        else if (input >= 'a' && input <= 'z')
        {
            input = input - 'a' + GLFW_KEY_A;
        }
        else
        {
            input = INVALID_INPUT;
        }

        return input;
    }
};

struct MouseInput : InputCache<MouseInput, GLFW_MOUSE_BUTTON_LAST>
{
    static constexpr f32 REPEATED_CLICK_DELAY = 0.5f; // NOTE : Could be configurable.

    Vec2                        current;
    Vec2                        previous;
    Vec2                        delta;
    Vec2                        scroll;
    StaticArray<u8, INPUT_SIZE> clicks;

    inline u8 repeated_click_count(u16 input) const
    {
        input = translate_input(input);

        return (states[input] & u8(InputState::DOWN)) ? clicks[input] : 0;
    }

    void update_input(u16 input, InputState state, f32 timestamp = 0.0f)
    {
        states[input] |= u8(state);

        if (state == InputState::DOWN)
        {
            if (timestamp - timestamps[input] <= REPEATED_CLICK_DELAY)
            {
                clicks[input]++;
            }
            else
            {
                clicks[input] = 1;
            }

            timestamps[input] = timestamp;
        }
    }

    inline void update_position(GLFWwindow* window, const Vec2& scale)
    {
        f64 x, y;
        glfwGetCursorPos(window, &x, &y);

        current.X = f32(scale.X * x);
        current.Y = f32(scale.Y * y);
    }

    inline void update_position_delta()
    {
        delta    = current - previous;
        previous = current;
    }

    static u16 translate_input(u16 input)
    {
        switch (input)
        {
        case MOUSE_LEFT:
            return GLFW_MOUSE_BUTTON_LEFT;
        case MOUSE_RIGHT:
            return GLFW_MOUSE_BUTTON_RIGHT;
        case MOUSE_MIDDLE:
            return GLFW_MOUSE_BUTTON_MIDDLE;
        default:
            return INVALID_INPUT;
        }
    }
};

} // namespace mnm
