#pragma once

#ifdef __cplusplus
extern "C" {
#endif


// -----------------------------------------------------------------------------
/// @section WINDOW

/// Window creation flags.
///
enum
{
    WINDOW_DEFAULT      = 0x0000,
    WINDOW_FIXED_SIZE   = 0x0001,
    WINDOW_FIXED_ASPECT = 0x0002,
    WINDOW_FULL_SCREEN  = 0x0004,
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

/// Checks whether the window was resized.
///
/// @returns Non-zero if the window was resized.
///
int resized(void);

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
/// @section INPUT

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
/// @section TIME

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
/// @section GEOMETRY
///
/// Meshes are made out of one or two vertex buffers and optionally also one
/// index buffer. The vertex data are split into position-only buffer / stream,
/// and attributes-only one. Mesh types (static / transient / dynamic)
/// correspond to BGFX notation.
///
/// Transient meshes do not have index buffers, while index buffer of static and
/// dynamic meshes is automatically created from the list of submitted vertices,
/// using the meshoptimizer library to optimize for vertex cache and overdraw.
///
/// Internally, \a MiNiMo uses

/// Mesh flags.
///
enum
{
    // Mesh type. Static by default.
    MESH_TRANSIENT           = 0x0001,
    MESH_DYNAMIC             = 0x0002,

    // Primitive type. Triangles by default.
    PRIMITIVE_QUADS          = 0x0004,
    PRIMITIVE_TRIANGLE_STRIP = 0x0005,
    PRIMITIVE_LINES          = 0x0006,
    PRIMITIVE_LINE_STRIP     = 0x0007,
    PRIMITIVE_POINTS         = 0x0008,

    // Vertex attribute flags. 3D position always on.
    VERTEX_COLOR             = 0x0010,
    VERTEX_NORMAL            = 0x0020,
    VERTEX_TEXCOORD          = 0x0040,
};

/// Starts mesh geometry recording. Mesh type, primitive type and attributes
/// recorded per-vertex are specified via flags. Once recorded, a mesh can be
/// submitted arbitrary number of times, but transient meshes (and their IDs)
/// are only valid in the current frame.
///
/// The amount of transient geometry in a single frame is limited and can be
/// specified in the init phase via `transient_memory` call.
///
/// Only the attributes specified via flags are stored, so that the same
/// code can possibly be used to generate meshes with different attributes (the
/// unlisted attributes's functions will be empty).
///
/// Using existing ID is legal, but results in destruction of the previously
/// submitted data.
///
void begin_mesh(int id, int flags);

/// Ends the current geometry recording.
///
void end_mesh(void);

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

/// Submits recorded mesh geometry.
///
/// @param[in] id Mesh identifier.
///
void mesh(int id);


// -----------------------------------------------------------------------------
/// @section TEXTURING
///
/// Textures serve to add detail and also as framebuffer attachments for
/// offscreen rendering.

/// Texture flags.
///
enum
{
    // Linear sampling, repeat border mode, RGBA8 format.
    TEXTURE_DEFAULT    = 0x0000,

    // Sampling. Linear if no flag provided.
    TEXTURE_NEAREST    = 0x0001,

    // Border mode. Repeat if no flag provided.
    TEXTURE_MIRROR     = 0x0002,
    TEXTURE_CLAMP      = 0x0004,

    // Format. RGBA8 if no flag provided.
    TEXTURE_R8         = 0x0008,
    TEXTURE_D24S8      = 0x0010,
    TEXTURE_D32F       = 0x0018,

    // Render target properties. Nothing if no flag provided.
    TEXTURE_TARGET     = 0x0040,
    TEXTURE_READ_BACK  = 0x0080,
    TEXTURE_WRITE_ONLY = 0x0100,
    TEXTURE_BLIT_DST   = 0x0200,
};

/// Automatic texture size related to backbuffer size. When a window is resized,
/// so is the texture.
///
enum
{
    SIZE_DOUBLE    = 0xffff,
    SIZE_EQUAL     = 0xfffa,
    SIZE_HALF      = 0xfffb,
    SIZE_QUARTER   = 0xfffc,
    SIZE_EIGHTH    = 0xfffd,
    SIZE_SIXTEENTH = 0xfffe,
};

/// Loads an RGBA texture from raw pixel data. The user-defined identifier can
/// be used repeatedly to overwrite existing content, but only the last content
/// is retained. Use the `texture` function to submit loaded texture.
///
/// Using existing ID will result in destruction of the previously created data.
///
/// @param[in] id Texture identifier.
/// @param[in] flags Texture properties' flags.
/// @param[in] width Image width in pixels.
/// @param[in] height Image height in pixels.
/// @param[in] stride Image stride in bytes. Pass zero to auto-compute.
/// @param[in] data Pixel data or `NULL`. If provided, the texture is immutable.
///
void load_texture(int id, int flags, int width, int height, int stride, const void* data);

/// Like `load_texture`, but no existing content is provided. Also supports
/// automatic backbuffer-size-related scaling.
///
void create_texture(int id, int flags, int width, int height);

/// Sets the active texture which is used with next `mesh` call, or, if called
/// between `begin_framebuffer` and `end_framebuffer` calls, it adds the texture
/// as the framebuffer's attachment.
///
/// @param[in] id Texture identifier.
///
void texture(int id);


// -----------------------------------------------------------------------------
/// @section PASSES
///
/// Passes correspond to BGFX's views concept and are primarily used for
/// sorting draw submissions. Each pass has several attached properties -
/// viewport, view and projection transformations and optional clear state and
/// framebuffer.
///
/// Active passes are thread-local and "sticky" - a pass will be active until
/// another call of `pass` function.
///
/// Submitting to a pass from multiple threads at once is safe, but setting its
/// properties is not implicitly synchronized.

/// Sets the active pass (thread-local). The pass stays active until next call
/// of the function, and is persistent across multiple frame. By default, pass
/// with ID `0` is active.
///
/// @param[in] id Pass identifier.
///
void pass(int id);

/// Resets clear flags for the active pass (so that no clearing is done). By
/// default nothing is cleared.
///
void no_clear(void);

/// Sets the depth buffer clear value for the active pass.
///
/// @param[in] depth Depth buffer clear value.
///
void clear_depth(float depth);

/// Sets the color buffer clear value for the active pass.
///
/// @param[in] rgba Color buffer clear value.
///
void clear_color(unsigned int rgba);

/// Resets current pass' framebuffer. By default, no framebuffer is set.
///
void no_framebuffer(void);

/// Associates a framebuffer with the current pass.
///
/// @param[in] id Framebuffer identifier.
///
void framebuffer(int id);

/// Sets the viewport value for the current pass. Primitives drawn outside will
/// be clipped.
///
/// Width and height are specified in actual pixels, not in screen coordinates.
/// Symbolic constant `SIZE_*` can be used, but must be the same for both width
/// and height.
///
/// Viewport origin is at the window's top-left corner.
///
/// TODO : Decide behavior on the high-DPI displays.
///
/// @param[in] x Horizontal offset from the window's top-left corner.
/// @param[in] y Vertical offset from the window's top-left corner.
/// @param[in] width Viewport width, in pixels.
/// @param[in] height Viewport height, in pixels.
///
void viewport(int x, int y, int width, int height);

/// Shortcut for `viewport(0, 0, SIZE_EQUAL, SIZE_EQUAL)`.
///
void full_viewport();


// -----------------------------------------------------------------------------
/// @section FRAMEBUFFERS
///
/// Framebuffers serve as render targets for offscreen rendering. They are
/// composed of individual texture attachments that must be created beforehand,
/// and associated with the framebuffer in an begin/end block.

/// Starts framebuffer building. Framebuffers are comprised of one or more
/// texture attachments, which can be associated with them by calling `texture`
/// inside the begin / end pair.
///
/// Any existing framebuffer associated with the used ID is destroyed, but not
/// its textures.
///
/// @param[in] id Framebuffer identifier.
///
void begin_framebuffer(int id);

/// Ends framebuffer building.
///
void end_framebuffer(void);


// -----------------------------------------------------------------------------
/// @section TRANSFORMATIONS
///
/// Each thread has an implicit matrix stack, that works similarly to how stack
/// used to work in the old OpenGL, but \a MiNiMo only has a single stack.
///
/// Each submitted mesh's model matrix is implicitly assigned by using the
/// stack's top matrix when `mesh` function is called.
///
/// View and projection matrices of each pass are explicitly copied from the top
/// matrix when `view` and `projection` functions are called. This explicitness
/// is needed since multiple threads can submit draws to the same pass at once.

/// Copies the curent matrix stack's top to the active pass' view matrix.
///
void view(void);

/// Copies the curent matrix stack's top to the active pass' projection matrix.
///
void projection(void);

/// Pushes the matrix stack down by one, duplicating the current matrix.
///
void push(void);

/// Pops the matrix stack, replacing the current matrix with the one below it.
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
/// @section MULTITHREADING
///
/// Multithreading in \a MiNiMo is very basic. At init time, a thread pool is
/// created with `std::thread::hardware_concurrency` minus two threads (main
/// one and BGFX rendering one), but always at least one. Individual task can
/// then be submitted and they are queued and executed on the
/// first-come-first-serve basis.
///
/// Resources can be created and draw primitives submitted from any thread, but
/// synchronization infrastructure is provided at the moment.

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
/// @section MISCELLANEOUS
///
/// Miscellaneous utilities that don't belong in other sections. 

/// Sets the transient geometry per-thread memory budget. 32 MB by default. Must
/// be called in the `init` callback (otherwise has no effect).
///
/// @param[in] megabytes Memory limit in MB.
///
void transient_memory(int megabytes);

/// Returns the current frame number, starting with zero-th frame.
///
/// @returns Frame number.
///
int frame(void);


// -----------------------------------------------------------------------------
/// @section MAIN ENTRY
///
/// \a MiNiMo has a single blocking entry point and optional macro implementing
/// C/C++ main entry is provided.

/// Entry point when running as a library. Uses the provided callbacks and
/// blocks until the app finishes.
///
/// @param[in] init Function that runs once *before* the window and context are
///   created.
/// @param[in] setup Function that runs once *after* the window and context are
///   created.
/// @param[in] draw Function that runs on every frame.
/// @param[in] cleanup Function that runs once just before the window is
///   destroyed.
///
/// @returns Zero if no error occurred.
///
/// @attention Use `mnm::run` (see the end of this file), if compiling from C++
///  to avoid possible warnings about exception safety (such as MSVC's C5039).
///
/// @warning This function must be called from the main thread only (the thread
///   that calls `main`).
///
int mnm_run(void (* init)(void), void (* setup)(void), void (* draw)(void), void (* cleanup)(void));

#ifdef __cplusplus
} // extern "C" {

#define MNM_MAIN_NAME mnm::run

namespace mnm
{

/// C++ variant of `mnm_run` function. Provided as a courtesy to avoid possible
/// warnings about exception safety.
///
int run(void (* init)(void), void (* setup)(void), void (* draw)(void), void (* cleanup)(void));

} // namespace mnm

#else
#   define MNM_MAIN_NAME mnm_run
#endif // __cplusplus

/// Helper macro to instantiate basic main function that just runs either
/// `mnm_run` (if compiled as C file), or `mnm::run` (if compiled as C++).
///
#define MNM_MAIN(init, setup, draw, cleanup) \
    int main(int argc, char** argv) \
    { \
        (void)argc; \
        (void)argv; \
        return MNM_MAIN_NAME(init, setup, draw, cleanup); \
    }
