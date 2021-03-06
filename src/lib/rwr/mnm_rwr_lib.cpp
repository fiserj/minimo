#include "mnm_rwr.cpp"

using namespace mnm::rwr;

// -----------------------------------------------------------------------------
// PUBLIC API IMPLEMENTATION - WINDOW
// -----------------------------------------------------------------------------

void size(int width, int height, int flags)
{
    ASSERT(
        t_ctx->is_main_thread,
        "`size` must be called from main thread only."
    );

    ASSERT(
        g_ctx->window_info.display_scale.X > 0.0f,
        "Invalid horizontal display scale (%.1f).",
        g_ctx->window_info.display_scale.X
    );

    ASSERT(
        g_ctx->window_info.display_scale.Y > 0.0f,
        "Invalid vertical display scale (%.1f).",
        g_ctx->window_info.display_scale.Y
    );

    // TODO : Round instead?
    if (g_ctx->window_info.position_scale.X != 1.0f)
    {
        width = int(width * g_ctx->window_info.display_scale.X);
    }

    // TODO : Round instead?
    if (g_ctx->window_info.position_scale.Y != 1.0f)
    {
        height = int(height * g_ctx->window_info.display_scale.Y);
    }

    resize_window(g_ctx->window_handle, width, height, flags);
}

void title(const char* title)
{
    ASSERT(
        t_ctx->is_main_thread,
        "`title` must be called from main thread only."
    );

    glfwSetWindowTitle(g_ctx->window_handle, title);
}

void vsync(int vsync)
{
    ASSERT(
        t_ctx->is_main_thread,
        "`vsync` must be called from main thread only."
    );

    g_ctx->vsync_on          = bool(vsync);
    g_ctx->reset_back_buffer = true;
}

void quit(void)
{
    ASSERT(
        t_ctx->is_main_thread,
        "`quit` must be called from main thread only."
    );

    glfwSetWindowShouldClose(g_ctx->window_handle, GLFW_TRUE);
}

float width(void)
{
    return g_ctx->window_info.invariant_size.X;
}

float height(void)
{
    return g_ctx->window_info.invariant_size.Y;
}

float aspect(void)
{
    return
        f32(g_ctx->window_info.framebuffer_size.X) /
        f32(g_ctx->window_info.framebuffer_size.Y);
}

float dpi(void)
{
    return g_ctx->window_info.display_scale.X;
}

int dpi_changed(void)
{
    return g_ctx->window_info.display_scale_changed || !g_ctx->frame_number;
}

int pixel_width(void)
{
    return g_ctx->window_info.framebuffer_size.X;
}

int pixel_height(void)
{
    return g_ctx->window_info.framebuffer_size.Y;
}


// -----------------------------------------------------------------------------
/// PUBLIC API IMPLEMENTATION - CURSOR
// -----------------------------------------------------------------------------

void cursor(int type)
{
    ASSERT(
        t_ctx->is_main_thread,
        "`cursor` must be called from main thread only."
    );

    ASSERT(
        type >= CURSOR_ARROW && type <= CURSOR_LOCKED,
        "Invalid cursor type %i.",
        type
    );

    if (g_ctx->active_cursor != u32(type))
    {
        g_ctx->active_cursor = u32(type);

        switch (type)
        {
        case CURSOR_HIDDEN:
            glfwSetInputMode(g_ctx->window_handle, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
            break;
        case CURSOR_LOCKED:
            glfwSetInputMode(g_ctx->window_handle, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            break;
        default:
            if (type >= CURSOR_ARROW && type <= CURSOR_LOCKED)
            {
                glfwSetInputMode(g_ctx->window_handle, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
                glfwSetCursor   (g_ctx->window_handle, g_ctx->window_cursors[u32(type)]);
            }
        }
    }
}


// -----------------------------------------------------------------------------
// PUBLIC API IMPLEMENTATION - INPUT
// -----------------------------------------------------------------------------

float mouse_x(void)
{
    return g_ctx->mouse.current.X;
}

float mouse_y(void)
{
    return g_ctx->mouse.current.Y;
}

float mouse_dx(void)
{
    return g_ctx->mouse.delta.X;
}

float mouse_dy(void)
{
    return g_ctx->mouse.delta.Y;
}

int mouse_down(int button)
{
    return g_ctx->mouse.is(button, InputState::DOWN);
}

int mouse_held(int button)
{
    return g_ctx->mouse.is(button, InputState::HELD);
}

int mouse_up(int button)
{
    return g_ctx->mouse.is(button, InputState::UP);
}

int mouse_clicked(int button)
{
    return g_ctx->mouse.repeated_click_count(button);
}

float mouse_held_time(int button)
{
    return g_ctx->mouse.held_time(button, f32(g_ctx->total_time.elapsed));
}

float scroll_x(void)
{
    return g_ctx->mouse.scroll.X;
}

float scroll_y(void)
{
    return g_ctx->mouse.scroll.Y;
}

int key_down(int key)
{
    return g_ctx->keyboard.is(key, InputState::DOWN);
}

int key_repeated(int key)
{
    return g_ctx->keyboard.is(key, InputState::REPEATED);
}

int key_held(int key)
{
    return g_ctx->keyboard.is(key, InputState::HELD);
}

int key_up(int key)
{
    return g_ctx->keyboard.is(key, InputState::UP);
}

float key_held_time(int key)
{
    return g_ctx->keyboard.held_time(key, f32(g_ctx->total_time.elapsed));
}

unsigned int codepoint(void)
{
    return int(next(g_ctx->codepoint_queue));
}


// -----------------------------------------------------------------------------
// PUBLIC API IMPLEMENTATION - TIME
// -----------------------------------------------------------------------------

double elapsed(void)
{
    return g_ctx->total_time.elapsed;
}

double dt(void)
{
    return g_ctx->frame_time.elapsed;
}

void sleep_for(double seconds)
{
    ASSERT(
        !t_ctx->is_main_thread,
        "`sleep_for` must not be called from the main thread."
    );

    std::this_thread::sleep_for(std::chrono::duration<double>(seconds));
}

void tic(void)
{
    tic(t_ctx->stop_watch);
}

double toc(void)
{
    return toc(t_ctx->stop_watch);
}


// -----------------------------------------------------------------------------
// PUBLIC API IMPLEMENTATION - MESH RECORDING
// -----------------------------------------------------------------------------

void begin_mesh(int id, int flags)
{
    ASSERT(
        t_ctx->record_info.type == RecordType::NONE,
        "Another recording in progress. Call respective `end_*` first."
    );

    ASSERT(
        id > 0 && id < int(MAX_MESHES),
        "Mesh ID %i out of available range 1 ... %i.",
        id, int(MAX_MESHES - 1)
    );

    t_ctx->record_info.flags      = u32(flags);
    t_ctx->record_info.extra_data = 0;
    t_ctx->record_info.id         = u16(id);
    t_ctx->record_info.type       = RecordType::MESH;

    start(t_ctx->mesh_recorder, t_ctx->record_info.flags);
}

void end_mesh(void)
{
    ASSERT(
        t_ctx->record_info.type == RecordType::MESH,
        "Mesh recording not started. Call `begin_mesh` first."
    );

    if (t_ctx->record_info.flags & (GENEREATE_FLAT_NORMALS | GENEREATE_SMOOTH_NORMALS))
    {
        ASSERT(
            t_ctx->mesh_recorder.attrib_state.size % sizeof(PackedNormal) == 0,
            "Vertex attribute state size (%" PRIu32 ") not divisible by the "
            "packed normal size (%zu).",
            t_ctx->mesh_recorder.attrib_state.size,
            sizeof(PackedNormal)
        );

        const u32 stride = t_ctx->mesh_recorder.attrib_state.size /
            sizeof(PackedNormal);
        const u32 offset = reinterpret_cast<u8*>(
            t_ctx->mesh_recorder.attrib_state.packed_normal
        ) - t_ctx->mesh_recorder.attrib_state.data;
        const Vec3* positions = reinterpret_cast<Vec3*>(
            t_ctx->mesh_recorder.position_buffer.data
        );
        PackedNormal* normals = reinterpret_cast<PackedNormal*>(
            t_ctx->mesh_recorder.attrib_buffer.data + offset
        );

        if (t_ctx->record_info.flags & GENEREATE_FLAT_NORMALS)
        {
            generate_flat_normals(
                t_ctx->mesh_recorder.vertex_count,
                stride,
                positions,
                normals
            );
        }
        else
        {
            generate_smooth_normals(
                t_ctx->mesh_recorder.vertex_count,
                stride,
                positions,
                &t_ctx->stack_allocator,
                normals
            );
        }
    }

    // TODO : Figure out error handling - crash or just ignore the submission?
    add_mesh(
        g_ctx->mesh_cache,
        t_ctx->record_info,
        t_ctx->mesh_recorder,
        g_ctx->vertex_layout_cache.layouts,
        &t_ctx->stack_allocator
    );

    end(t_ctx->mesh_recorder);

    t_ctx->record_info = {};
}

void vertex(float x, float y, float z)
{
    ASSERT(
        t_ctx->record_info.type == RecordType::MESH,
        "Mesh recording not started. Call `begin_mesh` first."
    );

    // TODO : We should measure whether branch prediction minimizes the cost of
    //        having a condition in here.
    if (!(t_ctx->record_info.flags & NO_VERTEX_TRANSFORM))
    {
        (*t_ctx->mesh_recorder.store_vertex)(
            (t_ctx->matrix_stack.top * HMM_Vec4(x, y, z, 1.0f)).XYZ,
            t_ctx->mesh_recorder.attrib_state,
            t_ctx->mesh_recorder
        );
    }
    else
    {
        (*t_ctx->mesh_recorder.store_vertex)(
            HMM_Vec3(x, y, z),
            t_ctx->mesh_recorder.attrib_state,
            t_ctx->mesh_recorder
        );
    }
}

void color(unsigned int rgba)
{
    ASSERT(
        t_ctx->record_info.type == RecordType::MESH ||
        t_ctx->record_info.type == RecordType::TEXT,
        "Mesh recording not started. Call `begin_mesh` first."
    );

    (*t_ctx->mesh_recorder.attrib_state.store_color)(
        t_ctx->mesh_recorder.attrib_state,
        rgba
    );
}

void normal(float nx, float ny, float nz)
{
    ASSERT(
        t_ctx->record_info.type == RecordType::MESH,
        "Mesh recording not started. Call `begin_mesh` first."
    );

    (*t_ctx->mesh_recorder.attrib_state.store_normal)(
        t_ctx->mesh_recorder.attrib_state,
        nx, ny, nz
    );
}

void texcoord(float u, float v)
{
    ASSERT(
        t_ctx->record_info.type == RecordType::MESH,
        "Mesh recording not started. Call `begin_mesh` first."
    );

    (*t_ctx->mesh_recorder.attrib_state.store_texcoord)(
        t_ctx->mesh_recorder.attrib_state,
        u, v
    );
}


// -----------------------------------------------------------------------------
// PUBLIC API IMPLEMENTATION - MESH SUBMISSION
// -----------------------------------------------------------------------------

void mesh(int id)
{
    ASSERT(
        id > 0 && id < int(MAX_MESHES),
        "Mesh ID %i out of available range 1 ... %i.",
        id, int(MAX_MESHES - 1)
    );

    DrawState& state = t_ctx->draw_state;
    state.pass = t_ctx->active_pass;
    state.framebuffer = g_ctx->pass_cache.passes[t_ctx->active_pass].framebuffer;

    const Mesh& mesh = g_ctx->mesh_cache.meshes[u16(id)];

    u32 mesh_flags = mesh.flags;

    if (bgfx::isValid(state.vertex_alias))
    {
        const u32 skips = vertex_layout_skips(mesh_flags, state.vertex_alias.idx);
        const u32 index = vertex_layout_index(mesh_flags, skips);

        mesh_flags &= ~skips;

        state.vertex_alias = g_ctx->vertex_layout_cache.handles[index];
    }

    if (!t_ctx->encoder)
    {
        t_ctx->encoder = bgfx::begin(!t_ctx->is_main_thread);
        ASSERT(t_ctx->encoder, "Failed to acquire BGFX encoder.");
    }

    // TODO : Check whether instancing works together with the aliasing.
    if (state.instances)
    {
        t_ctx->encoder->setInstanceDataBuffer(&state.instances->buffer);

        if (state.instances->is_transform)
        {
            mesh_flags |= INSTANCING_SUPPORTED;
        }
    }

    if (mesh_flags & TEXT_MESH)
    {
        if (!bgfx::isValid(state.texture))
        {
            texture(mesh.extra_data);
        }

        if (state.flags == STATE_DEFAULT)
        {
            // NOTE : Maybe we just want ensure that the blending is added?
            state.flags = STATE_BLEND_ALPHA | STATE_WRITE_RGB;
        }
    }

    if (!bgfx::isValid(state.program))
    {
        // TODO : Figure out how to do this without this ugliness.
        if (state.sampler.idx ==
            g_ctx->default_uniforms[u32(DefaultUniform::COLOR_TEXTURE_RED)].idx)
        {
            mesh_flags |= SAMPLER_COLOR_R;
        }

        const u32 index = default_program_index(mesh_flags);
        state.program = g_ctx->default_programs[index];

        ASSERT(bgfx::isValid(state.program), "Invalid state program.");
    }

    if (state.element_start != 0 || state.element_count != U32_MAX)
    {
        WARN(
            mesh_flags & OPTIMIZE_GEOMETRY,
            "Mesh %i has optimized geometry. Using sub-range might not work.",
            id
        );

        if (mesh_flags & PRIMITIVE_QUADS)
        {
            ASSERT(
                state.element_start % 4 == 0,
                "Sub-range start not divisble by 4."
            );

            ASSERT(
                state.element_count % 4 == 0,
                "Sub-range count not divisble by 4."
            );

            state.element_start = (state.element_start >> 1) * 3;
            state.element_count = (state.element_count >> 1) * 3;
        }
    }

    submit_mesh(
        mesh,
        t_ctx->matrix_stack.top,
        state,
        g_ctx->mesh_cache.transient_buffers,
        g_ctx->default_uniforms,
        *t_ctx->encoder
    );

    state = {};
}

void alias(int flags)
{
    t_ctx->draw_state.vertex_alias = { u16(flags) };
}

void range(int start, int count)
{
    ASSERT(start >= 0, "Negative start index.");

    t_ctx->draw_state.element_start = u32(start) ;
    t_ctx->draw_state.element_count = count >= 0 ? u32(count) : U32_MAX;
}

void state(int flags)
{
    t_ctx->draw_state.flags = u16(flags);
}

void scissor(int x, int y, int width, int height)
{
    ASSERT(x >= 0, "Negative scissor X (%i).", x);
    ASSERT(y >= 0, "Negative scissor Y (%i).", y);
    ASSERT(width >= 0, "Negative scissor width (%i).", width);
    ASSERT(height >= 0, "Negative scissor height (%i).", height);

    if (!t_ctx->encoder)
    {
        t_ctx->encoder = bgfx::begin(!t_ctx->is_main_thread);
        ASSERT(t_ctx->encoder, "Failed to acquire BGFX encoder.");
    }

    t_ctx->encoder->setScissor(u16(x), u16(y), u16(width), u16(height));
}


// -----------------------------------------------------------------------------
// PUBLIC API IMPLEMENTATION - TEXTURING
// -----------------------------------------------------------------------------

void load_texture(int id, int flags, int width, int height, int stride, const void* data)
{
    ASSERT(
        id > 0 && id < int(MAX_TEXTURES),
        "Texture ID %i out of available range 1 ... %i.",
        id, int(MAX_TEXTURES - 1)
    );

    ASSERT(width > 0, "Non-positive texture width (%i).", width);

    ASSERT(height > 0, "Non-positive texture height (%i).", height);

    ASSERT(
        (width < SIZE_EQUAL && height < SIZE_EQUAL) ||
        (width <= SIZE_DOUBLE && width == height), // TODO : Inspect necessity of this.
        "Non-conforming texture width (%i) or height (%i).",
        width, height
    );

    ASSERT(stride >= 0, "Negative texture stride (%i).", stride);

    add_texture(
        g_ctx->texture_cache,
        u16(id),
        u16(flags),
        u16(width),
        u16(height),
        u16(stride),
        data,
        &t_ctx->frame_allocator
    );
}

void create_texture(int id, int flags, int width, int height)
{
    load_texture(id, flags, width, height, 0, nullptr);
}

void texture(int id)
{
    ASSERT(
        id > 0 && id < int(MAX_TEXTURES),
        "Texture ID %i out of available range 1 ... %i.",
        id, int(MAX_TEXTURES - 1)
    );

    const Texture& texture = g_ctx->texture_cache.textures[u16(id)];

    if (t_ctx->record_info.type != RecordType::FRAMEBUFFER)
    {
        // TODO : Samplers should be set by default state and only overwritten when
        //        non-default shader is used.
        t_ctx->draw_state.texture = texture.handle;
        t_ctx->draw_state.sampler = default_sampler(g_ctx->default_uniforms, texture.format);
        t_ctx->draw_state.texture_size[0] = texture.width;
        t_ctx->draw_state.texture_size[1] = texture.height;
    }
    else
    {
        add_attachment(t_ctx->framebuffer_recorder, texture);
    }
}


// -----------------------------------------------------------------------------
// PUBLIC API IMPLEMENTATION - TEXTURE READBACK
// -----------------------------------------------------------------------------

void read_texture(int id, void* data)
{
    ASSERT(
        id > 0 && id < int(MAX_TEXTURES),
        "Texture ID %i out of available range 1 ... %i.",
        id, int(MAX_TEXTURES - 1)
    );

    ASSERT(data, "Invalid data pointer.");

    if (!t_ctx->encoder)
    {
        t_ctx->encoder = bgfx::begin(!t_ctx->is_main_thread);
        ASSERT(t_ctx->encoder, "Failed to acquire BGFX encoder.");
    }

    schedule_texture_read(
        g_ctx->texture_cache,
        u16(id),
        t_ctx->active_pass + MAX_PASSES, // TODO : It might be better to let the user specify the pass explicitly.
        t_ctx->encoder,
        data
    );
}

int read_screen(void* data)
{
    ASSERT(data, "Invalid data pointer.");

    const u32 id = MAX_TEXTURES + g_ctx->bgfx_frame_number;

    if (id <= g_ctx->last_screenshot)
    {
        return 0;
    }

    g_ctx->last_screenshot = id;

    char string[16];
    encode_pointer(data, string, sizeof(string));

    bgfx::requestScreenShot(BGFX_INVALID_HANDLE, string);

    return -int(id);
}

int readable(int id)
{
    u32 read_frame;

    if (id <= -int(MAX_TEXTURES))
    {
        read_frame = u32(-id) - MAX_TEXTURES + 2u;
    }
    else
    {
        ASSERT(
            id > 0 && id < int(MAX_TEXTURES),
            "Texture ID %i out of available range 1 ... %i.",
            id, int(MAX_TEXTURES - 1)
        );

        read_frame = g_ctx->texture_cache.textures[u16(id)].read_frame;
    }

    return g_ctx->bgfx_frame_number >= read_frame;
}


// -----------------------------------------------------------------------------
// PUBLIC API IMPLEMENTATION - INSTANCING
// -----------------------------------------------------------------------------

void begin_instancing(int id, int type)
{
    ASSERT(
        t_ctx->record_info.type == RecordType::NONE,
        "Another recording in progress. Call respective `end_*` first."
    );

    ASSERT(
        id > 0 && id < int(MAX_INSTANCE_BUFFERS),
        "Instance buffer ID %i out of available range 1 ... %i.",
        id, int(MAX_INSTANCE_BUFFERS - 1)
    );

    ASSERT(
        type >= INSTANCE_TRANSFORM && type <= INSTANCE_DATA_112,
        "Invalid instance buffer data type %i.",
        type
    );

    start(t_ctx->instance_recorder, u32(type));

    t_ctx->record_info.id           = u16(id);
    t_ctx->record_info.is_transform = type == INSTANCE_TRANSFORM;
    t_ctx->record_info.type         = RecordType::INSTANCES;
}

void end_instancing(void)
{
    ASSERT(
        t_ctx->record_info.type == RecordType::INSTANCES,
        "Instance buffer recording not started. Call `begin_instancing` first."
    );

    // TODO : Figure out error handling - crash or just ignore the submission?
    add_instances(
        g_ctx->instance_cache,
        t_ctx->instance_recorder,
        t_ctx->record_info.id,
        t_ctx->record_info.is_transform
    );

    end(t_ctx->instance_recorder);

    t_ctx->record_info = {};
}

void instance(const void* data)
{
    ASSERT(
        t_ctx->record_info.type == RecordType::INSTANCES,
        "Instance buffer recording not started. Call `begin_instancing` first."
    );

    ASSERT(data || t_ctx->record_info.is_transform, "Invalid data pointer.");

    append(
        t_ctx->instance_recorder,
        t_ctx->record_info.is_transform ? &t_ctx->matrix_stack.top : data
    );
}

void instances(int id)
{
    ASSERT(
        id > 0 && id < int(MAX_INSTANCE_BUFFERS),
        "Instance buffer ID %i out of available range 1 ... %i.",
        id, int(MAX_INSTANCE_BUFFERS - 1)
    );

    // TODO : Assert that instance ID is active in the cache in the current frame.
    t_ctx->draw_state.instances = &g_ctx->instance_cache.data[u16(id)];
}


// -----------------------------------------------------------------------------
// PUBLIC API IMPLEMENTATION - FONT ATLASING
// -----------------------------------------------------------------------------

void create_font(int id, const void* data)
{
    ASSERT(
        id > 0 && id < int(mnm::rwr::MAX_FONTS),
        "Font ID %i out of available range 1 ... %i.",
        id, int(mnm::rwr::MAX_FONTS - 1)
    );

    ASSERT(data, "Invalid data pointer.");

    // TODO : Should we invalidate all atlases using the font data previously
    //        associated with given `id`?
    bx::atomicExchangePtr(
        const_cast<void**>(&g_ctx->font_data_cache[u32(id)]),
        const_cast<void*>(data)
    );
}

void begin_atlas(int id, int flags, int font, float size)
{
    ASSERT(
        t_ctx->record_info.type == RecordType::NONE,
        "Another recording in progress. Call respective `end_*` first."
    );

    ASSERT(
        id > 0 && id < int(MAX_TEXTURES),
        "Atlas ID %i out of available range 1 ... %i.",
        id, int(MAX_TEXTURES - 1)
    );

    // TODO : Check `flags`.

    ASSERT(
        font > 0 && font < int(mnm::rwr::MAX_FONTS),
        "Font ID %i out of available range 1 ... %i.",
        font, int(mnm::rwr::MAX_FONTS - 1)
    );

    ASSERT(
        size >= 5.0f && size <= 4096.0f,
        "Invalid atlas font size. Must be between 5 and 4096."
    );

    FontAtlas* atlas = acquire_atlas(g_ctx->font_atlas_cache, u32(id));

    if (atlas)
    {
        t_ctx->record_info.id   = u16(id);
        t_ctx->record_info.type = RecordType::ATLAS;

        reset(
            *atlas,
            g_ctx->texture_cache,
            u16(id),
            u16(flags),
            g_ctx->font_data_cache[u32(font)],
            size
        );
    }
}

void end_atlas(void)
{
    ASSERT(
        t_ctx->record_info.type == RecordType::ATLAS,
        "Atlas recording not started. Call `begin_atlas` first."
    );

    FontAtlas* atlas = fetch_atlas(g_ctx->font_atlas_cache, t_ctx->record_info.id);
    ASSERT(atlas, "Invalid atlas ID %i.", t_ctx->record_info.id);

    update(*atlas, g_ctx->texture_cache, &t_ctx->frame_allocator);

    t_ctx->record_info = {};
}

void glyph_offset_hint(int offset)
{
    ASSERT(
        t_ctx->record_info.type == RecordType::ATLAS,
        "Atlas recording not started. Call `begin_atlas` first."
    );

    ASSERT(offset >= 0, "Negative offset hint X (%i).", offset);

    WARN(
        t_ctx->mesh_recorder.vertex_count,
        "`glyph_offset_hint` can only be called right after the atlas creation."
    );

    // TODO : Should be mutexed.
    if (!t_ctx->mesh_recorder.vertex_count)
    {
        FontAtlas* atlas = fetch_atlas(g_ctx->font_atlas_cache, t_ctx->record_info.id);
        ASSERT(atlas, "Invalid atlas ID %i.", t_ctx->record_info.id);

        atlas->codepoints.low_offset = u32(offset);
    }
}

void glyph_range(int first, int last)
{
    ASSERT(
        t_ctx->record_info.type == RecordType::ATLAS,
        "Atlas recording not started. Call `begin_atlas` first."
    );

    ASSERT(first >= 0, "Negative first codepoint (%i).", first);

    ASSERT(last >= 0, "Negative last codepoint (%i).", last);

    FontAtlas* atlas = fetch_atlas(g_ctx->font_atlas_cache, t_ctx->record_info.id);
    ASSERT(atlas, "Invalid atlas ID %i.", t_ctx->record_info.id);

    add_glyph_range(*atlas, u32(first), u32(last));
}

void glyphs_from_string(const char* string)
{
    ASSERT(
        t_ctx->record_info.type == RecordType::ATLAS,
        "Atlas recording not started. Call `begin_atlas` first."
    );

    ASSERT(string, "Invalid glyphs' string pointer.");

    FontAtlas* atlas = fetch_atlas(g_ctx->font_atlas_cache, t_ctx->record_info.id);
    ASSERT(atlas, "Invalid atlas ID %i.", t_ctx->record_info.id);

    add_glyphs_from_string(*atlas, string, nullptr);
}


// -----------------------------------------------------------------------------
// PUBLIC API IMPLEMENTATION - TEXT MESHES
// -----------------------------------------------------------------------------

void begin_text(int mesh_id, int atlas_id, int flags)
{
    ASSERT(
        t_ctx->record_info.type == RecordType::NONE,
        "Another recording in progress. Call respective `end_*` first."
    );

    ASSERT(
        atlas_id > 0 && atlas_id < int(MAX_TEXTURES),
        "Atlas ID %i out of available range 1 ... %i.",
        atlas_id, int(MAX_TEXTURES - 1)
    );

    // TODO : Check `flags`.

    FontAtlas* atlas = fetch_atlas(g_ctx->font_atlas_cache, u32(atlas_id));
    ASSERT(atlas, "Invalid atlas ID %i.", atlas_id);

    const int mesh_flags =
          PRIMITIVE_QUADS         |
          VERTEX_TEXCOORD         |
          VERTEX_COLOR            |
          TEXT_MESH               |
         (TEXT_TYPE_MASK & flags) |
        ((TEXCOORD_F32 | VERTEX_PIXCOORD) * is_updatable(*atlas));

    start(t_ctx->text_recorder, u32(flags), *atlas);

    begin_mesh(mesh_id, mesh_flags);

    t_ctx->record_info.extra_data = u32(atlas_id);
    t_ctx->record_info.type       = RecordType::TEXT;
}

void end_text(void)
{
    ASSERT(
        t_ctx->record_info.type == RecordType::TEXT,
        "Text recording not started. Call `begin_text` first."
    );

    t_ctx->record_info.type = RecordType::MESH;

    end_mesh();
}

void alignment(int flags)
{
    ASSERT(
        t_ctx->record_info.type == RecordType::TEXT,
        "Text recording not started. Call `begin_text` first."
    );

    if (flags & TEXT_H_ALIGN_MASK)
    {
        t_ctx->text_recorder.h_alignment = u16(flags & TEXT_H_ALIGN_MASK);
    }

    if (flags & TEXT_V_ALIGN_MASK)
    {
        t_ctx->text_recorder.v_alignment = u16(flags & TEXT_V_ALIGN_MASK);
    }
}

void line_height(float factor)
{
    ASSERT(
        t_ctx->record_info.type == RecordType::TEXT,
        "Text recording not started. Call `begin_text` first."
    );

    ASSERT(
        factor > 0.0f,
        "Non-positive line height factor count (%.1f).",
        factor
    );

    t_ctx->text_recorder.line_height = factor;
}

void text(const char* start, const char* end)
{
    ASSERT(
        t_ctx->record_info.type == RecordType::TEXT,
        "Text recording not started. Call `begin_text` first."
    );

    ASSERT(start, "Invalid text start pointer.");

    ASSERT(
        end > start || !end,
        "Invalid end pointer (address not bigger than the start one)."
    );

    const auto try_record_text = [&]()
    {
        return record_text(
            start,
            end,
            t_ctx->text_recorder,
            t_ctx->matrix_stack.top,
            &t_ctx->stack_allocator,
            t_ctx->mesh_recorder
        );
    };

    FontAtlas& atlas = *t_ctx->text_recorder.atlas;

    bool success = try_record_text();

    if (!success && is_updatable(atlas))
    {
        add_glyphs_from_string(atlas, start, end);
        update(atlas, g_ctx->texture_cache, &t_ctx->frame_allocator);

        success = try_record_text();
    }

    WARN(
        success,
        "Failed to lay text '...' due to missing glyphs." // TODO : Add text excerpt.
    );
}


// -----------------------------------------------------------------------------
// PUBLIC API IMPLEMENTATION - PASSES
// -----------------------------------------------------------------------------

void pass(int id)
{
    ASSERT(
        id >= 0 && id < int(MAX_PASSES),
        "Pass ID %i out of available range 1 ... %i.",
        id, int(MAX_PASSES - 1)
    );

    t_ctx->active_pass = u16(id);
    g_ctx->pass_cache.passes[t_ctx->active_pass].dirty_flags |= Pass::DIRTY_TOUCH;
}

void no_clear(void)
{
    Pass& pass = g_ctx->pass_cache.passes[t_ctx->active_pass];

    if (pass.clear_flags != BGFX_CLEAR_NONE)
    {
        pass.clear_flags  = BGFX_CLEAR_NONE;
        pass.dirty_flags |= Pass::DIRTY_CLEAR;
    }
}

void clear_depth(float depth)
{
    Pass& pass = g_ctx->pass_cache.passes[t_ctx->active_pass];

    if (  pass.clear_depth != depth            ||
        !(pass.dirty_flags & BGFX_CLEAR_DEPTH) ||
        !(pass.clear_flags & BGFX_CLEAR_DEPTH)
    )
    {
        pass.clear_flags |= BGFX_CLEAR_DEPTH;
        pass.clear_depth  = depth;
        pass.dirty_flags |= Pass::DIRTY_CLEAR;
    }
}

void clear_color(unsigned int rgba)
{
    Pass& pass = g_ctx->pass_cache.passes[t_ctx->active_pass];

    if (  pass.clear_rgba != rgba              ||
        !(pass.dirty_flags & BGFX_CLEAR_COLOR) ||
        !(pass.clear_flags & BGFX_CLEAR_COLOR)
    )
    {
        pass.clear_flags |= BGFX_CLEAR_COLOR;
        pass.clear_rgba   = rgba;
        pass.dirty_flags |= Pass::DIRTY_CLEAR;
    }
}

void no_framebuffer(void)
{
    Pass& pass = g_ctx->pass_cache.passes[t_ctx->active_pass];

    pass.framebuffer  = BGFX_INVALID_HANDLE;
    pass.dirty_flags |= Pass::DIRTY_FRAMEBUFFER;
}

void framebuffer(int id)
{
    ASSERT(
        id > 0 && id < int(MAX_FRAMEBUFFERS),
        "Framebuffer ID %i out of available range 1 ... %i.",
        id, int(MAX_FRAMEBUFFERS - 1)
    );

    Pass& pass = g_ctx->pass_cache.passes[t_ctx->active_pass];

    pass.framebuffer  = g_ctx->framebuffer_cache.framebuffers[u16(id)].handle;
    pass.dirty_flags |= Pass::DIRTY_FRAMEBUFFER;
}

void viewport(int x, int y, int width, int height)
{
    ASSERT(x >= 0, "Negative viewport X (%i).", x);

    ASSERT(y >= 0, "Negative viewport Y (%i).", y);

    ASSERT(width >= 0, "Negative viewport width (%i).", width);

    ASSERT(height >= 0, "Negative viewport height (%i).", height);

    ASSERT(
        (width < SIZE_EQUAL && height < SIZE_EQUAL) ||
        (width <= SIZE_DOUBLE && width == height), // TODO : Inspect necessity of this.
        "Non-conforming texture width (%i) or height (%i).",
        width, height
    );

    Pass& pass = g_ctx->pass_cache.passes[t_ctx->active_pass];

    if (pass.viewport_x      != x     ||
        pass.viewport_y      != y     ||
        pass.viewport_width  != width ||
        pass.viewport_height != height)
    {
        pass.viewport_x      = u16(x);
        pass.viewport_y      = u16(y);
        pass.viewport_width  = u16(width);
        pass.viewport_height = u16(height);
        pass.dirty_flags    |= Pass::DIRTY_RECT;
    }
}

void full_viewport(void)
{
    viewport(0, 0, SIZE_EQUAL, SIZE_EQUAL);
}


// -----------------------------------------------------------------------------
// PUBLIC API IMPLEMENTATION - FRAMEBUFFERS
// -----------------------------------------------------------------------------

void begin_framebuffer(int id)
{
    ASSERT(
        t_ctx->record_info.type == RecordType::NONE,
        "Another recording in progress. Call respective `end_*` first."
    );

    ASSERT(
        id > 0 && id < int(MAX_FRAMEBUFFERS),
        "Framebuffer ID %i out of available range 1 ... %i.",
        id, int(MAX_FRAMEBUFFERS - 1)
    );

    start(t_ctx->framebuffer_recorder);

    t_ctx->record_info.id   = u16(id);
    t_ctx->record_info.type = RecordType::FRAMEBUFFER;
}

void end_framebuffer(void)
{
    ASSERT(
        t_ctx->record_info.type == RecordType::FRAMEBUFFER,
        "Framebuffer recording not started. Call `begin_framebuffer` first."
    );

    add_framebuffer(
        g_ctx->framebuffer_cache,
        t_ctx->record_info.id,
        t_ctx->framebuffer_recorder.width,
        t_ctx->framebuffer_recorder.height,
        {
            t_ctx->framebuffer_recorder.attachments.data,
            t_ctx->framebuffer_recorder.count
        }
    );

    end(t_ctx->framebuffer_recorder);

    t_ctx->record_info = {};
}


// -----------------------------------------------------------------------------
// PUBLIC API IMPLEMENTATION - SHADERS
// -----------------------------------------------------------------------------

void create_uniform(int id, int type, int count, const char* name)
{
    ASSERT(
        id > 0 && id < int(MAX_UNIFORMS),
        "Uniform ID %i out of available range 1 ... %i.",
        id, int(MAX_UNIFORMS - 1)
    );

    ASSERT(
        type > 0 && type <= UNIFORM_SAMPLER,
        "Invalid uniform type %i.",
        type
    );

    ASSERT(count > 0, "Non-positive uniform count (%i).", count);

    ASSERT(name, "Invalid name string.");

    add_uniform(
        g_ctx->uniform_cache,
        u16(id),
        u16(type),
        u16(count),
        name
    );
}

void uniform(int id, const void* value)
{
    ASSERT(
        id > 0 && id < int(MAX_UNIFORMS),
        "Uniform ID %i out of available range 1 ... %i.",
        id, int(MAX_UNIFORMS - 1)
    );

    ASSERT(value, "Invalid uniform value pointer.");

    if (!t_ctx->encoder)
    {
        t_ctx->encoder = bgfx::begin(!t_ctx->is_main_thread);
        ASSERT(t_ctx->encoder, "Failed to acquire BGFX encoder.");
    }

    t_ctx->encoder->setUniform(
        g_ctx->uniform_cache.handles[u16(id)],
        value,
        U16_MAX
    );
}

void create_shader(int id, const void* vs_data, int vs_size, const void* fs_data, int fs_size)
{
    ASSERT(
        id > 0 && id < int(MAX_PROGRAMS),
        "Program ID %i out of available range 1 ... %i.",
        id, int(MAX_PROGRAMS - 1)
    );

    ASSERT(vs_data, "Invalid vertex shader data pointer.");

    ASSERT(vs_size > 0, "Non-positive vertex shader data size (%i).", vs_size);

    ASSERT(fs_data, "Invalid fragment shader data pointer.");

    ASSERT(fs_size > 0, "Non-positive fragment shader data size (%i).", fs_size);

    add_program(
        g_ctx->program_cache,
        u16(id),
        vs_data,
        u32(vs_size),
        fs_data,
        u32(fs_size)
    );
}

void shader(int id)
{
    ASSERT(
        id > 0 && id < int(MAX_PROGRAMS),
        "Program ID %i out of available range 1 ... %i.",
        id, int(MAX_PROGRAMS - 1)
    );

    t_ctx->draw_state.program = g_ctx->program_cache.handles[u16(id)];
}


// -----------------------------------------------------------------------------
// PUBLIC API IMPLEMENTATION - TRANSFORMATIONS
// -----------------------------------------------------------------------------

void view(void)
{
    Pass& pass = g_ctx->pass_cache.passes[t_ctx->active_pass];

    pass.view_matrix  = t_ctx->matrix_stack.top;
    pass.dirty_flags |= Pass::DIRTY_TRANSFORM;
}

void projection(void)
{
    Pass& pass = g_ctx->pass_cache.passes[t_ctx->active_pass];

    pass.proj_matrix  = t_ctx->matrix_stack.top;
    pass.dirty_flags |= Pass::DIRTY_TRANSFORM;
}

void push(void)
{
    push(t_ctx->matrix_stack);
}

void pop(void)
{
    pop(t_ctx->matrix_stack);
}

void identity(void)
{
    t_ctx->matrix_stack.top = HMM_Mat4d(1.0f);
}

void ortho(float left, float right, float bottom, float top, float near_, float far_)
{
    multiply_top(
        t_ctx->matrix_stack,
        HMM_Orthographic(left, right, bottom, top, near_, far_)
    );
}

void perspective(float fovy, float aspect, float near_, float far_)
{
    multiply_top(
        t_ctx->matrix_stack,
        HMM_Perspective(fovy, aspect, near_, far_)
    );
}

void look_at(float eye_x, float eye_y, float eye_z, float at_x, float at_y,
    float at_z, float up_x, float up_y, float up_z)
{
    multiply_top(
        t_ctx->matrix_stack,
        HMM_LookAt(
            HMM_Vec3(eye_x, eye_y, eye_z),
            HMM_Vec3( at_x,  at_y,  at_z),
            HMM_Vec3( up_x,  up_y,  up_z)
        )
    );
}

void rotate(float angle, float x, float y, float z)
{
    multiply_top(t_ctx->matrix_stack, HMM_Rotate(angle, HMM_Vec3(x, y, z)));
}

void rotate_x(float angle)
{
    // TODO : General rotation matrix is wasteful here.
    rotate(angle, 1.0f, 0.0f, 0.0f);
}

void rotate_y(float angle)
{
    // TODO : General rotation matrix is wasteful here.
    rotate(angle, 0.0f, 1.0f, 0.0f);
}

void rotate_z(float angle)
{
    // TODO : General rotation matrix is wasteful here.
    rotate(angle, 0.0f, 0.0f, 1.0f);
}

void scale(float scale)
{
    multiply_top(t_ctx->matrix_stack, HMM_Scale(HMM_Vec3(scale, scale, scale)));
}

void translate(float x, float y, float z)
{
    multiply_top(t_ctx->matrix_stack, HMM_Translate(HMM_Vec3(x, y, z)));
}


// -----------------------------------------------------------------------------
// PUBLIC API IMPLEMENTATION - MULTITHREADING
// -----------------------------------------------------------------------------

int task(void (* func)(void* data), void* data)
{
    ASSERT(func, "Invalid task function pointer.");

    Task* task = acquire_task(g_ctx->task_pool);

    if (task)
    {
        task->func = func;
        task->data = data;

        g_ctx->task_scheduler.AddTaskSetToPipe(task);
    }

    return task != nullptr;
}


// -----------------------------------------------------------------------------
// PUBLIC API IMPLEMENTATION - GENERAL MEMORY MANAGEMENT
// -----------------------------------------------------------------------------

void* alloc(int type, int size)
{
    ASSERT(type >= 0 && type <= 1, "Invalid requested memory type (%i).", type);
    ASSERT(size >= 0, "Negative requested memory size (%i).", size);

    if (type == MEMORY_TEMPORARY)
    {
        return BX_ALLOC(&t_ctx->frame_allocator, size);
    }

    return alloc(g_ctx->persistent_memory_cache, u32(size));
}

void dealloc(void* memory)
{
    if (memory)
    {
        dealloc(g_ctx->persistent_memory_cache, memory);
    }
}


// -----------------------------------------------------------------------------
// PUBLIC API IMPLEMENTATION - FILE IO
// -----------------------------------------------------------------------------




// -----------------------------------------------------------------------------
// PUBLIC API IMPLEMENTATION - PLATFORM INFO
// -----------------------------------------------------------------------------

int platform(void)
{
#if BX_PLATFORM_LINUX
    return PLATFORM_LINUX;
#elif BX_PLATFORM_OSX
    return PLATFORM_MACOS;
#elif BX_PLATFORM_WINDOWS
    return PLATFORM_WINDOWS;
#else
    return PLATFORM_UNKNOWN;
#endif
}

int renderer(void)
{
    switch (bgfx::getRendererType())
    {
    case bgfx::RendererType::Direct3D11:
        return RENDERER_DIRECT3D11;
    case bgfx::RendererType::Metal:
        return RENDERER_METAL;
    case bgfx::RendererType::OpenGL:
        return RENDERER_OPENGL;
    default:
        return RENDERER_UNKNOWN;
    }
}


// -----------------------------------------------------------------------------
// PUBLIC API IMPLEMENTATION - MISCELLANEOUS
// -----------------------------------------------------------------------------

void transient_memory(int megabytes)
{
    ASSERT(
        t_ctx->is_main_thread,
        "`transient_memory` must be called from main thread only."
    );

    ASSERT(
        megabytes > 0,
        "Non-positive amount of transient memory requested (%i).",
        megabytes
    );

    g_ctx->transient_memory = u32(megabytes << 20);
}

int frame(void)
{
    return int(g_ctx->frame_number);
}

int limit(int resource)
{
    switch (resource)
    {
    case ::MAX_FONTS:
        return int(mnm::rwr::MAX_FONTS);

    // TODO : Remaining limits.

    default:
        return 0;
    }
}


// -----------------------------------------------------------------------------
