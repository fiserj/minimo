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
/// framebuffer horizontal coordinates.
///
/// @returns Window DPI.
///
float dpi(void);

/// Checks whether the window DPI has changed in the current frame. Also true
/// for the very first frame.
///
/// @returns Non-zero if DPI has changed.
///
int dpi_changed(void);

/// Returns the backbuffer width in pixels.
///
/// @returns Backbuffer width in pixels.
///
int pixel_width(void);

/// Returns the backbuffer height in pixels.
///
/// @returns Backbuffer height in pixels.
///
int pixel_height(void);


// -----------------------------------------------------------------------------
/// @section CURSOR

/// Cursor type enum.
///
enum
{
    // Standard cursor shapes.
    CURSOR_ARROW,
    CURSOR_CROSSHAIR,
    CURSOR_H_RESIZE,
    CURSOR_HAND,
    CURSOR_I_BEAM,
    CURSOR_V_RESIZE,

    // Cursor not visible, but otherwise behaving normally.
    CURSOR_HIDDEN,

    // Cursor locked to the current window, providing unlimited movement.
    CURSOR_LOCKED,
};

/// Changes the active cursor.
///
/// @param[in] type Cursor type.
///
/// @warning This function must be called from the main thread only.
///
void cursor(int type);


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

    KEY_F1,
    KEY_F2,
    KEY_F3,
    KEY_F4,
    KEY_F5,
    KEY_F6,
    KEY_F7,
    KEY_F8,
    KEY_F9,
    KEY_F10,
    KEY_F11,
    KEY_F12,
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

/// Returns length of a sequence of successive clicks, in which each pair of
/// following clicks was separated by no more than 500 ms.
///
/// @returns Number of successive button clicks.
///
int mouse_clicked(int button);

/// Returns time duration for which the mouse button has been held down. Zero,
/// if it just went down, and negative if not currently held.
///
/// @returns Time in seconds.
///
float mouse_held_time(int button);

/// Returns horizontal scroll offset. Typical mouse wheel does not provide any.
///
/// @returns Scroll offset in arbitrary units.
///
float scroll_x(void);

/// Returns vertical scroll offset.
///
/// @returns Scroll offset in arbitrary units.
///
float scroll_y(void);

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

/// Returns time duration for which the key has been held down. Zero, if it just
/// went down, and negative if not currently held.
///
/// @returns Time in seconds.
///
float key_held_time(int key);

/// Returns queued character inputs.
///
/// @returns Next queued UTF-8 character input or zero when the queue is empty.
///
unsigned int codepoint(void);


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
/// dynamic meshes is automatically created from the list of submitted vertices.

/// Mesh flags.
///
/// @todo Add option to disable vertex multiplication by the current model
///   matrix. This would be useful when the data is already transformed and only
//    an identity matrix would be used.
///
enum
{
    // Static, triangle-based, position-only (so not all that useful).
    MESH_DEFAULT             = 0x0000,

    // Mesh type. Static by default.
    MESH_STATIC              = 0x0001,
    MESH_TRANSIENT           = 0x0002,
    MESH_DYNAMIC             = 0x0004,

    // Primitive type. Triangles by default.
    PRIMITIVE_TRIANGLES      = 0x0008,
    PRIMITIVE_QUADS          = 0x0010,
    PRIMITIVE_TRIANGLE_STRIP = 0x0020,
    PRIMITIVE_LINES          = 0x0030,
    PRIMITIVE_LINE_STRIP     = 0x0040,
    PRIMITIVE_POINTS         = 0x0050,

    // Vertex attribute flags. 3D position always on.
    VERTEX_COLOR             = 0x0080,
    VERTEX_NORMAL            = 0x0100,
    VERTEX_TEXCOORD          = 0x0200,

    // Texcoord uses full float range.
    TEXCOORD_F32             = 0x1000,

    // Optimizes the mesh data for beter rendering performance, potentially
    // changing the primitive ordering - don't use if you plan to use `range`.
    // Only useful for static or dynamic meshes, and for triangles or quads.
    OPTIMIZE_GEOMETRY        = 0x2000,

    // Disables transformation of submitted vertices by the current matrix.
    NO_VERTEX_TRANSFORM      = 0x4000,

    // Keeps the geometry on CPU (positions only).
    KEEP_CPU_GEOMETRY        = 0x8000, // TODO : Add support.
};

/// Mesh draw state flags. Subset of the most comonly used ones from BGFX.
///
enum
{
    STATE_BLEND_ADD          = 0x0001,
    STATE_BLEND_ALPHA        = 0x0002,
    STATE_BLEND_MAX          = 0x0003,
    STATE_BLEND_MIN          = 0x0004,

    STATE_CULL_CCW           = 0x0010,
    STATE_CULL_CW            = 0x0020,

    STATE_DEPTH_TEST_GEQUAL  = 0x0040,
    STATE_DEPTH_TEST_GREATER = 0x0080,
    STATE_DEPTH_TEST_LEQUAL  = 0x00c0,
    STATE_DEPTH_TEST_LESS    = 0x0100,

    STATE_MSAA               = 0x0200,

    STATE_WRITE_A            = 0x0400,
    STATE_WRITE_RGB          = 0x0800,
    STATE_WRITE_Z            = 0x1000,

    STATE_DEFAULT            = STATE_CULL_CW         |
                               STATE_DEPTH_TEST_LESS |
                               STATE_MSAA            |
                               STATE_WRITE_A         |
                               STATE_WRITE_RGB       |
                               STATE_WRITE_Z         ,
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
/// vertex position is multiplied by the current model matrix, unless the
/// `NO_VERTEX_TRANSFORM` flag was provided in the `begin_mesh` call.
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

/// Sets alias for next submited mesh's vertex buffer.
///
/// @param[in] flags Vertex attribute flags.
///
void alias(int flags);

/// Sets element range for the for next submitted mesh call.
///
/// @param[in] start First element.
/// @param[in] count Element count.
///
void range(int start, int count);

/// Sets state for next submitted mesh call, after which it gets reset.
/// `STATE_DEFAULT` by default.
///
/// @param[in] flags Draw state flags.
///
void state(int flags);

/// Sets scissor for next submitted mesh call, after which it gets cleared.
///
/// @param[in] x Horizontal offset from window's top-left corner.
/// @param[in] y Vertical offset from window's top-left corner.
/// @param[in] width Width of the scissor region.
/// @param[in] height Height of the scissor region.
///
/// @attention All values are in pixels, not screen coordinates.
///
void scissor(int x, int y, int width, int height);


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
/// @section TEXTURE READBACK
///
/// ...

/// Schedules a texture content to be read and copied to a given data buffer.
/// The texture must have been created with `TEXTURE_READ_BACK` flag.
///
/// Content is not available immediately - use `readable` to check whether
/// `data` has already been filled with valid content (typically in the next
/// frame).
///
/// @param[in] id Texture identifier.
/// @param[in] data Destination data buffer.
///
void read_texture(int id, void* data);

/// Checks whether a texture can be read back in current frame.
///
/// @param[in] id Texture identifier.
///
/// @returns Non-zero if the texture was copied to the destination data buffer.
///
int readable(int id);


// -----------------------------------------------------------------------------
/// @section INSTANCING
///
/// ...

/// Instance data type.
///
enum
{
    // Per-instance transform. If provided, it's always first in the layout.
    INSTANCE_TRANSFORM,

    // Per-instance custom data with fixed byte-size.
    INSTANCE_DATA_16,
    INSTANCE_DATA_32,
    INSTANCE_DATA_48,
    INSTANCE_DATA_64,
    INSTANCE_DATA_80,
    INSTANCE_DATA_96,
    INSTANCE_DATA_112,
};

/// Starts instance buffer recording. Mesh type, primitive type and attributes
/// recorded per-vertex are specified via flags. Once recorded, the buffer can
/// be associated with arbitrary number of mesh submissions, but it's lifetime
/// is limited to the current frame only.
///
/// @param[in] id Instance buffer identifier.
/// @param[in] type Instance buffer data type.
///
///
void begin_instancing(int id, int type);

/// Ends the current instance buffer recording.
///
void end_instancing(void);

/// Copies custom data into the instance buffer. Expected size corresponds to
/// the `INSTANCE_DATA_*` flag specified in the `begin_instancing` call.
///
/// If `INSTANCE_TRANSFORM` was part of the creation flags, it is added
/// automatically from the curent matrix stack's top. If instance data only
/// contain the transformation, pass `NULL` as the parameter.
///
/// @param[in] data Instance data or `NULL`.
///
void instance(const void* data);

/// Sets the active instance buffer which is used with next `mesh` call.
///
/// @param[in] id Instance buffer identifier.
///
void instances(int id);


// -----------------------------------------------------------------------------
/// @section FONT ATLASES
///
/// ...

/// Creates font face from the given data blob.
///
void create_font(int id, const void* data);

/// Text atlas flags.
///
enum
{
    // Immutable, no oversampling.
    ATLAS_DEFAULT         = 0x0000,

    // Non-stored glyphs are attempted to be added when creating text mesh.
    ATLAS_ALLOW_UPDATE    = 0x0001, // TODO : Add support.

    // Stores distance to glyph outline rather than direct rasterization.
    ATLAS_SDF             = 0x0004, // TODO : Add support.

    // Oversampling for better quality.
    ATLAS_H_OVERSAMPLE_2X = 0x0008,
    ATLAS_H_OVERSAMPLE_3X = 0x0010,
    ATLAS_H_OVERSAMPLE_4X = 0x0018,

    ATLAS_V_OVERSAMPLE_2X = 0x0040,

    // Usage hint. Does not require an updatable atlas to be thread safe.
    ATLAS_NOT_THREAD_SAFE = 0x0080,
};

/// One font/size per atlas. Texture size is calculated automatically to fit.
///
void begin_atlas(int id, int flags, int font, float size);

/// ...
///
void end_atlas(void);

/// ...
///
void glyph_range(int first, int last);

/// ...
///
void glyphs_from_string(const char* string);


// -----------------------------------------------------------------------------
/// @section TEXT MESHES
///
/// ...

/// Text mesh flags.
///
enum
{
    // Static, left-aligned horizontally, baseline-aligned vertically, Y down.
    TEXT_DEFAULT            = 0x0000,

    // Text mesh type.
    TEXT_STATIC             = 0x0001,
    TEXT_TRANSIENT          = 0x0002,
    TEXT_DYNAMIC            = 0x0004,

    // Horizontal alignment.
    TEXT_H_ALIGN_LEFT       = 0x0008,
    TEXT_H_ALIGN_CENTER     = 0x0010,
    TEXT_H_ALIGN_RIGHT      = 0x0020,

    // Vertical alignment.
    TEXT_V_ALIGN_BASELINE   = 0x0040,
    TEXT_V_ALIGN_MIDDLE     = 0x0080,
    TEXT_V_ALIGN_CAP_HEIGHT = 0x0100,

    // Y axis direction.
    TEXT_Y_AXIS_DOWN        = 0x0200,
    TEXT_Y_AXIS_UP          = 0x0400,

    // Aligns glyph quads to integer coordinates.
    TEXT_ALIGN_TO_INTEGER   = 0x0800,
};

/// Starts text mesh recording. Internally, the regular mesh recorder is used,
/// so that all the drawing-related commands can be used as with any other mesh.
///
/// Each glyph is represented as a textured quad with per-vertex color.
///
void begin_text(int id, int atlas, int flags);

/// Ends the current text mesh recording.
///
void end_text(void);

/// Sets text alignment flags state inside an active text mesh creation. By
/// default, text is left-aligned horizontally and baseline-aligned vertically.
///
/// @param[in] flags Text mesh alignment flags.
///
void alignment(int flags);

/// Sets text line height factor inside an active text mesh creation. The final
/// line height is then calculated as `cap_height * factor`. Set to `2` by
/// default.
///
/// @param[in] factor Text mesh line height factor.
///
void line_height(float factor);

/// Adds glyph quads into current text mesh. Strings are assumed to use UTF-8.
/// Currently set alignment and line height are used, and the glyph vertex
/// positions are multiplied by the current model matrix.
///
/// @param[in] start First byte of the string.
/// @param[in] end One byte past the end of the string, or `NULL`. If not
///   provided, processing stops when the null-terminator is found.
///
void text(const char* start, const char* end);

/// Calculates bounding box of a given text. Can be called outside of
/// `begin_text` / `end_text` block.
///
/// @param[in] atlas Font atlas ID.
/// @param[in] start First byte of the string.
/// @param[in] end One byte past the end of the string, or `NULL`.
/// @param[in] line_height Text mesh line height factor.
/// @param[out] width Text bounding box width, in pixels. Can be `NULL`.
/// @param[out] height Text bounding box height, in pixels. Can be `NULL`.
///
/// @attention The returned size has no transformation applied on it.
///
void text_size(int atlas, const char* start, const char* end, float line_height, float* width, float* height);


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
/// @todo Decide behavior on the high-DPI displays.
///
/// @param[in] x Horizontal offset from the window's top-left corner.
/// @param[in] y Vertical offset from the window's top-left corner.
/// @param[in] width Viewport width, in pixels.
/// @param[in] height Viewport height, in pixels.
///
void viewport(int x, int y, int width, int height);

/// Shortcut for `viewport(0, 0, SIZE_EQUAL, SIZE_EQUAL)`.
///
void full_viewport(void);


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
/// @section SHADERS
///
/// By default, \a MiNiMo provides basic shaders for common vertex attribute
/// combinations, but custom shaders can be created.

/// Uniform type.
///
enum
{
    UNIFORM_VEC4    = 0x0001,
    UNIFORM_MAT4    = 0x0002,
    UNIFORM_MAT3    = 0x0003,
    UNIFORM_SAMPLER = 0x0004,
};

/// Creates a uniform.
///
/// Using existing ID will result in destruction of the previously created data.
///
/// @param[in] id Uniform identifier.
/// @param[in] type Uniform type.
/// @param[in] count Number of elements in an array.
///
void create_uniform(int id, int type, int count, const char* name);

/// Sets the uniform value which is used with next `mesh` call.
///
/// @param[in] id Uniform identifier.
/// @param[in] value Uniform data.
///
void uniform(int id, const void* value);

/// Creates a shader program. The shader data must be in specific format
///
/// Using existing ID will result in destruction of the previously created data.
///
/// @param[in] id Shader program identifier.
/// @param[in] vs_data Vertex shader blob data.
/// @param[in] vs_size Vertex shader blob size in bytes.
/// @param[in] fs_data Fragment shader blob data.
/// @param[in] fs_size Fragment shader blob size in bytes.
///
void create_shader(int id, const void* vs_data, int vs_size, const void* fs_data, int fs_size);

/// Sets the shader program used with next `mesh` call. If no shader is
/// provided, the default one for mesh's vertex attributes (or an alias) is
/// used.
///
/// @param[in] id Shader program identifier.
///
void shader(int id);


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
/// @section IMAGE IO
///
/// ...

unsigned char* load_image(const char* file_name, int channels, int* width, int* height);

unsigned char* decode_image(const void* data, int bytes, int channels, int* width, int* height);

void save_image(const char* file_name, const void* data, int width, int height, int channels);


// -----------------------------------------------------------------------------
/// @section GENERAL FILE IO
///
/// ...

/// Reads the raw contents of a file and places it in an array.
///
unsigned char* load_bytes(const char* file_name, int* bytes_read);

void save_bytes(const char* file_name, const void* data, int bytes);

/// Reads the contents of a file as a single null-terminated string.
///
char* load_string(const char* file_name);

void save_string(const char* file_name, const char* string);

/// Releases memory allocated by one of the `load_*` or `decode_*` functions.
///
void unload(void* file_content);


// -----------------------------------------------------------------------------
/// @section PLATFORM INFO
///
/// Functionality to retrieve basic information about the current environment.

/// Supported platforms.
///
enum
{
    PLATFORM_LINUX,
    PLATFORM_MACOS,
    PLATFORM_WINDOWS,
    PLATFORM_UNKNOWN,
};

/// Supported renderers.
///
enum
{
    RENDERER_DIRECT3D11,
    RENDERER_METAL,
    RENDERER_OPENGL,
    RENDERER_UNKNOWN,
};

/// Returns current platform identifier.
///
/// @returns Current platform identifier.
///
int platform(void);

/// Returns current renderer identifier.
///
/// @returns Current renderer identifier.
///
int renderer(void);


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
