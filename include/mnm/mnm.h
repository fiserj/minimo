#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
// MAIN ENTRY
// -----------------------------------------------------------------------------

/// Entry point when running as a library. The 
///
/// @param[in] setup Function that runs once after the window was created.
/// @param[in] draw Function that runs on every frame.
/// @param[in] cleanup Function that runs once just before the window is destroyed.
///
/// @returns Zero if no error occurred.
///
/// @warning This function must be called from the main thread only (the thread
///   that calls `main`).
///
int mnm_run(void (* setup)(void), void (* draw)(void), void (* cleanup)(void));

/// Helper macro to instantiate basic main function that just runs `mnm_run`.
///
#define MNM_MAIN(setup, draw, cleanup) \
    int main(int argc, char** argv) \
    { \
        (void)argc; \
        (void)argv; \
        return mnm_run(setup, draw, cleanup); \
    }


// -----------------------------------------------------------------------------
// WINDOW
// -----------------------------------------------------------------------------

/// Window creation flags.
///
enum
{
    WINDOW_DEFAULT      = 0,
    WINDOW_FIXED_SIZE   = 1,
    WINDOW_FIXED_ASPECT = 2,
    WINDOW_FULL_SCREEN  = 4,
};

/// Changes window's size and attributes. The size is specified in "screen
/// coordinates", which may not correspond directly to pixels (typically on
/// high-DPI displays).
///
/// @param[in] width Width in screen coordinates.
/// @param[in] height Height in screen coordinates.
/// @param[in] flags Window attributes.
///
/// @warning This function must be called from the main thread only.
///
void size(int width, int height, int flags);

/// Sets window title.
///
/// @param[in] title Window title string.
///
/// @warning This function must be called from the main thread only.
///
void title(const char* title);

/// Sets VSync on or off. Starts off by default.
///
/// @param[in] vsync If non-zero, VSync is turned on.
///
/// @warning This function must be called from the main thread only.
///
void vsync(int vsync);

/// Signals that the window should be closed after the current frame.
///
/// @warning This function must be called from the main thread only.
///
void quit(void);

/// Returns window width in screen coordinates.
///
/// @returns Window width in screen coordinates.
///
float width(void);

/// Returns window height in screen coordinates.
///
/// @returns Window height in screen coordinates.
///
float height(void);

/// Returns window aspect ratio, i.e, its width divided by its height.
///
/// @returns Window aspect ratio.
///
float aspect(void);

/// Returns window DPI, or, more precisely, the ratio between screen and
/// framebuffer coordinates.
///
/// @returns Window DPI.
///
float dpi(void);


// -----------------------------------------------------------------------------
// INPUT
// -----------------------------------------------------------------------------

/// Special keys enum (alphabetical keys can be passed as characters).
///
enum
{
    KEY_ANY,

    KEY_BACKSPACE,
    KEY_DELETE,
    KEY_DOWN,
    KEY_ENTER,
    KEY_ESCAPE,
    KEY_LEFT,
    KEY_RIGHT,
    KEY_SPACE,
    KEY_TAB,
    KEY_UP,
};

/// Mouse button enum.
///
enum
{
    MOUSE_LEFT,
    MOUSE_MIDDLE,
    MOUSE_RIGHT,
};

/// Returns mouse X position in screen coordinates.
///
/// @returns Mouse X position in screen coordinates.
///
float mouse_x(void);

/// Returns mouse Y position in screen coordinates.
///
/// @returns Mouse Y position in screen coordinates.
///
float mouse_y(void);

/// Returns mouse delta in X position between current and previous frames in
/// screen coordinates.
///
/// @returns Mouse delta in X position in screen coordinates.
///
float mouse_dx(void);

/// Returns mouse delta in Y position between current and previous frames in
/// screen coordinates.
///
/// @returns Mouse delta in Y position in screen coordinates.
///
float mouse_dy(void);

/// Checks whether a particular mouse button went down in the current frame.
///
/// @returns Non-zero if button went down.
///
int mouse_down(int button);

/// Checks whether a particular mouse button was held down at least in current
/// and previous frame.
///
/// @returns Non-zero if button was held down.
///
int mouse_held(int button);

/// Checks whether a particular mouse button went up in the current frame.
///
/// @returns Non-zero if button went up.
///
int mouse_up(int button);

/// Checks whether a particular key went down in the current frame.
///
/// @returns Non-zero if key went down.
///
int key_down(int key);

/// Checks whether a particular key was held down at least in current
/// and previous frame.
///
/// @returns Non-zero if key was held down.
///
int key_held(int key);

/// Checks whether a particular key went up in the current frame.
///
/// @returns Non-zero if key went up.
///
int key_up(int key);


// -----------------------------------------------------------------------------
// TIME
// -----------------------------------------------------------------------------

/// Returns time elapsed since the start of the application, in seconds. The
/// timer is reset before the first `draw` call.
///
/// @returns Time in seconds.
///
double elapsed(void);

/// Returns time elapsed since last frame, in seconds.
///
/// @returns Time in seconds.
///
double dt(void);

/// Sleeps for at least the given amount of seconds. It should never be called
/// from the main thread, as it will stall the rendering.
///
/// @param[in] seconds Time in seconds.
///
void sleep_for(double seconds);

/// Starts a thread-local stopwatch.
///
void tic(void);

/// Returns time elapsed sine the last thread-local call to `tic`, in seconds.
///
/// @returns Time in seconds.
///
double toc(void);


// -----------------------------------------------------------------------------
// GEOMETRY
// -----------------------------------------------------------------------------

/// Vertex attribute flags.
///
enum
{
    VERTEX_COLOR    = 1,
    VERTEX_NORMAL   = 2,
    VERTEX_TEXCOORD = 4,
};

void begin_transient(int id, int attribs);

void begin_static(int id, int attribs);

/// Starts immediate geometry building mode. Only supported primitive is
/// triangles. Only vertex attributes specified in the `attribs` parameter are
/// recorded. Vertex position is always recorded.
///
/// @param[in] attribs Vertex attribute flags.
///
//void begin(int attribs);

/// Starts cached geometry / mesh building mode. Only supported primitive is
/// triangles. Only vertex attributes specified in the `attribs` parameter are
/// recorded. Vertex position is always recorded. The user-defined identifier
/// can be used repeatedly to overwrite existing content, but only the last
/// content is retained. Use the `mesh` function to submit cached geometry using
/// the current model matrix.
///
/// @param[in] attribs Vertex attribute flags.
/// @param[in] id Cached geometry identifier. Must be non-zero.
///
//void begin_cached(int attribs, int id);

/// Emits a vertex with given coordinates and current state (color, etc.). The
/// vertex position is multiplied by the current model matrix.
///
/// @param[in] x X coordinate of the vertex.
/// @param[in] y Y coordinate of the vertex.
/// @param[in] z Z coordinate of the vertex.
///
void vertex(float x, float y, float z);

/// Sets current color.
///
/// @param[in] rgba Color value in hexadecimal format (e.g., `0x00ff00ff` for
///   opaque green).
///
void color(unsigned int rgba);

/// Sets current normal vector.
///
/// @param[in] nx X component of the normal vector.
/// @param[in] ny Y component of the normal vector.
/// @param[in] nz Z component of the normal vector.
///
void normal(float nx, float ny, float nz);

/// Sets current texture coordinate.
///
/// @param[in] u U texture coordinate.
/// @param[in] v V texture coordinate.
///
void texcoord(float u, float v);

/// Ends the current geometry. Note that the data isn't immediately copied over
/// to the GPU, but it's retained on the CPU and submitted together after the
/// `draw` function returns.
///
void end(void);

/// Submits a cached geometry / mesh created previously with `begin_cached` with
/// the same identifier.
///
/// @param[in] id Cached geometry identifier. Must be non-zero.
///
void mesh(int id);


// -----------------------------------------------------------------------------
// TEXTURING
// -----------------------------------------------------------------------------

/// Texture mode flags.
///
enum
{
    TEXTURE_LINEAR  =  1,
    TEXTURE_NEAREST =  2,
    TEXTURE_REPEAT  =  4,
    TEXTURE_MIRROR  =  8,
    TEXTURE_CLAMP   = 12,
};

/// Loads an RGBA texture from raw pixel data. The user-defined identifier can
/// be used repeatedly to overwrite existing content, but only the last content
/// is retained. Use the `texture` function to submit loaded texture.
///
/// @param[in] id Texture identifier. Must be non-zero.
/// @param[in] width Image width in pixels.
/// @param[in] width Image height in pixels.
/// @param[in] stride Image stride in bytes. If zero, `width * 4` is assumed.
/// @param[in] rgba RGBA pixels.
///
void load_texture(int id, int width, int height, int stride, const void* rgba);

/// Sets the current texture mode properties. Default properties are
/// `TEXTURE_LINEAR | TEXTURE_REPEAT`.
///
/// @param[in] flags Texture mode properties.
///
void texture_mode(int flags);

/// Sets the active texture which is used with following `begin` and `mesh`
/// calls, until a new identifier is provided.
///
/// @param[in] id Non-zero texture identifier, or zero to disable texturing.
///
void texture(int id);


// -----------------------------------------------------------------------------
// TRANSFORMATIONS
// -----------------------------------------------------------------------------

/// Sets model matrix stack as the active one.
///
void model(void);

/// Sets view matrix stack as the active one.
///
void view(void);

/// Sets projection matrix stack as the active one.
///
void projection(void);

/// Pushes the top of the active matrix stack.
///
void push(void);

/// Pops the top of the active matrix stack.
///
void pop(void);

/// Replaces the top of the active matrix stack with the identity matrix.
///
void identity(void);

/// Multiplies the top of the active matrix stack with an orthographic matrix.
///
/// @param[in] left Left vertical clipping plane position.
/// @param[in] right Right vertical clipping plane position.
/// @param[in] bottom Bottom horizontal clipping plane position.
/// @param[in] top Top horizontal clipping plane position.
/// @param[in] near Distance to the near depth clipping plane.
/// @param[in] far Distance to the far depth clipping plane.
///
void ortho(float left, float right, float bottom, float top, float near, float far);

/// Multiplies the top of the active matrix stack with a perspective projection
/// matrix.
///
/// @param[in] fovy Field of view angle in degrees in the Y direction.
/// @param[in] aspect Aspect ratio determining the field of view in the X
///   direction. Typically, just call the `aspect` function.
/// @param[in] near 
/// @param[in] near Distance to the near depth clipping plane.
/// @param[in] far Distance to the far depth clipping plane.
///
void perspective(float fovy, float aspect, float near, float far);

/// Multiplies the top of the active matrix stack with a "camera" matrix,
/// looking from an eye point at a center point, with a prescribed up vector.
///
/// @param[in] eye_x X coordinate of the eye point.
/// @param[in] eye_y Y coordinate of the eye point.
/// @param[in] eye_z Z coordinate of the eye point.
/// @param[in] at_x X coordinate of the center point.
/// @param[in] at_y Y coordinate of the center point.
/// @param[in] at_z Z coordinate of the center point.
/// @param[in] up_x X direction of the up vector.
/// @param[in] up_y Y direction of the up vector.
/// @param[in] up_z Z direction of the up vector.
///
void look_at(float eye_x, float eye_y, float eye_z, float at_x, float at_y, float at_z, float up_x, float up_y, float up_z);

/// Multiplies the top of the active matrix stack with a rotation matrix.
///
/// @param[in] angle Angle of rotation in degrees.
/// @param[in] x X direction of the rotation axis vector.
/// @param[in] y Y direction of the rotation axis vector.
/// @param[in] z Z direction of the rotation axis vector.
///
void rotate(float angle, float x, float y, float z);

/// Multiplies the top of the active matrix stack with a rotation-around-X-axis
/// matrix.
///
/// @param[in] angle Angle of rotation in degrees.
///
void rotate_x(float angle);

/// Multiplies the top of the active matrix stack with a rotation-around-Y-axis
/// matrix.
///
/// @param[in] angle Angle of rotation in degrees.
///
void rotate_y(float angle);

/// Multiplies the top of the active matrix stack with a rotation-around-Z-axis
/// matrix.
///
/// @param[in] angle Angle of rotation in degrees.
///
void rotate_z(float angle);

/// Multiplies the top of the active matrix stack with a scale matrix. Only
/// uniform scaling is supported.
///
/// @param[in] scale Scale factor.
///
void scale(float scale);

/// Multiplies the top of the active matrix stack with a translation matrix.
///
/// @param[in] x X coordinate of the translation vector.
/// @param[in] y Y coordinate of the translation vector.
/// @param[in] z Z coordinate of the translation vector.
///
void translate(float x, float y, float z);


// -----------------------------------------------------------------------------
// MULTITHREADING
// -----------------------------------------------------------------------------

/// Adds an asynchronous task to the queue. Tasks can be created also from
/// within other tasks.
///
/// @param[in] func Pointer to the function to be executed.
/// @param[in] data Payload for the function.
///
/// @returns Non-zero if task was added to the queue.
///
/// @attention Tasks may not be run immediately if all the queue threads are
///   currently loaded.
///
int task(void (* func)(void* data), void* data);


// -----------------------------------------------------------------------------

#ifdef __cplusplus
}
#endif
