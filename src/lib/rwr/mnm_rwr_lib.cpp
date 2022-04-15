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
            t_ctx->mesh_recorder.attrib_state.size / sizeof(PackedNormal) == 0,
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
            t_ctx->mesh_recorder.position_buffer.data + offset
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
        t_ctx->record_info.type == RecordType::MESH,
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
            // texture(mesh.extra_data);
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

    ASSERT(stride > 0, "Negative texture stride (%i).", stride);

    add_texture(
        g_ctx->texture_cache,
        u16(id),
        u16(flags),
        u16(width),
        u16(height),
        u16(stride),
        data
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

int readable(int id)
{
    ASSERT(
        id > 0 && id < int(MAX_TEXTURES),
        "Texture ID %i out of available range 1 ... %i.",
        id, int(MAX_TEXTURES - 1)
    );

    const u32 read_frame = g_ctx->texture_cache.textures[u16(id)].read_frame;

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

    ASSERT(data, "Invalid data pointer.");

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
// PUBLIC API IMPLEMENTATION - PASSES
// -----------------------------------------------------------------------------

void pass(int id)
{
    ASSERT(
        id > 0 && id < int(MAX_PASSES),
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
