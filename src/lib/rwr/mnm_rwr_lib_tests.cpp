#include "mnm_rwr_lib.cpp"

#undef WARN

#include <catch2/catch_test_macros.hpp>
#include <catch2/benchmark/catch_benchmark.hpp>


// -----------------------------------------------------------------------------
// DEFERRED EXECUTION
// -----------------------------------------------------------------------------

TEST_CASE("Deferred Execution", "[basic]")
{
    int value = 1;

    {
        defer(value++);

        {
            defer(
                for (int i = 0; i < 3; i++)
                {
                    value++;
                }
            );

            REQUIRE(value == 1);
        }

        REQUIRE(value == 4);
    }

    REQUIRE(value == 5);
}


// -----------------------------------------------------------------------------
// ARENA ALLOCATOR
// -----------------------------------------------------------------------------

TEST_CASE("Arena Allocator", "[basic]")
{
    BX_ALIGN_DECL_16(struct) Buffer
    {
        u8 data[128] = {};
    };

    Buffer         buffer;
    ArenaAllocator allocator;

    // Initialization.
    init(allocator, buffer.data, sizeof(buffer.data));
    REQUIRE(allocator.size == sizeof(buffer.data));
    REQUIRE(allocator.top == 0);
    REQUIRE(allocator.last == 0);

    // Non-aligned allocation I.
    void* first = BX_ALLOC(&allocator, 13);
    REQUIRE(first != nullptr);
    REQUIRE(allocator.owns(first));
    REQUIRE(allocator.size == sizeof(buffer.data));
    REQUIRE(allocator.top == 13);
    REQUIRE(allocator.last == 0);

    // Aligned allocation.
    void* second = BX_ALIGNED_ALLOC(&allocator, 16, 16);
    REQUIRE(second != nullptr);
    REQUIRE(second > first);
    REQUIRE(allocator.owns(second));
    REQUIRE(allocator.top == 32);
    REQUIRE(allocator.last == 13);

    // Non-aligned allocation I.
    void* third = BX_ALLOC(&allocator, 5);
    REQUIRE(third != nullptr);
    REQUIRE(third > second);
    REQUIRE(allocator.owns(third));
    REQUIRE(allocator.top == 37);
    REQUIRE(allocator.last == 32);

    // Failed in-place reallocation (out of space).
    void* third_realloc_failed = BX_REALLOC(&allocator, third, 100);
    REQUIRE(third_realloc_failed == nullptr);
    REQUIRE(!allocator.owns(third_realloc_failed));
    REQUIRE(allocator.top == 37);
    REQUIRE(allocator.last == 32);

    // Successful in-place reallocation.
    void* third_realloc_succeeded = BX_REALLOC(&allocator, third, 32);
    REQUIRE(third_realloc_succeeded != nullptr);
    REQUIRE(third_realloc_succeeded == third);
    REQUIRE(allocator.owns(third_realloc_succeeded));
    REQUIRE(allocator.top == 64);
    REQUIRE(allocator.last == 32);

    // Freeing of last allocation.
    BX_FREE(&allocator, third);
    REQUIRE(allocator.top == 32);
    REQUIRE(allocator.last == 32);

    // No-op freeing of previous block.
    BX_FREE(&allocator, second);
    REQUIRE(allocator.top == 32);
    REQUIRE(allocator.last == 32);

    // No-op freeing of the first block.
    BX_FREE(&allocator, first);
    REQUIRE(allocator.top == 32);
    REQUIRE(allocator.last == 32);

    // Realloc of second block (can't be done in-place).
    void* second_realloc_succeeded = BX_ALIGNED_REALLOC(&allocator, second, 64, 16);
    REQUIRE(second_realloc_succeeded != nullptr);
    REQUIRE(allocator.owns(second_realloc_succeeded));
    REQUIRE(allocator.top == 96);
    REQUIRE(allocator.last == 32);
    REQUIRE(bx::memCmp(second, second_realloc_succeeded, 16) == 0);
};


// -----------------------------------------------------------------------------
// STACK ALLOCATOR
// -----------------------------------------------------------------------------

TEST_CASE("Stack Allocator", "[basic]")
{
    BX_ALIGN_DECL_16(struct) Buffer
    {
        u8 data[128] = {};
    };

    Buffer         buffer;
    StackAllocator allocator;

    // Initialization.
    init(allocator, buffer.data, sizeof(buffer.data));
    REQUIRE(allocator.size == sizeof(buffer.data));
    REQUIRE(allocator.top == 8);
    REQUIRE(allocator.last == 0);

    // Non-aligned allocation I.
    void* first = BX_ALLOC(&allocator, 13);
    REQUIRE(first != nullptr);
    REQUIRE(allocator.owns(first));
    REQUIRE(allocator.size == sizeof(buffer.data));
    REQUIRE(allocator.top == 29);
    REQUIRE(allocator.last == 8);

    // Aligned allocation.
    void* second = BX_ALIGNED_ALLOC(&allocator, 8, 16);
    REQUIRE(second != nullptr);
    REQUIRE(allocator.owns(second));
    REQUIRE(allocator.top == 56);
    REQUIRE(allocator.last == 40);

    // Failed allocation (out of space).
    void* third = BX_ALLOC(&allocator, 128);
    REQUIRE(third == nullptr);
    REQUIRE(!allocator.owns(third));
    REQUIRE(allocator.top == 56);
    REQUIRE(allocator.last == 40);

    // No-op freeing of a `NULL` pointer.
    BX_FREE(&allocator, nullptr);
    REQUIRE(allocator.top == 56);
    REQUIRE(allocator.last == 40);

    // In-place reallocation.
    void* second_realloced = BX_ALIGNED_REALLOC(&allocator, second, 16, 16);
    REQUIRE(second == second_realloced);
    REQUIRE(allocator.owns(second_realloced));
    REQUIRE(allocator.top == 64);
    REQUIRE(allocator.last == 40);

    // Reallocation "anew".
    void* first_realloced = BX_REALLOC(&allocator, first, 8);
    REQUIRE(first != first_realloced);
    REQUIRE(allocator.owns(first_realloced));
    REQUIRE(allocator.top == 80);
    REQUIRE(allocator.last == 64);

    // Freeing the middle block.
    BX_FREE(&allocator, second_realloced);
    REQUIRE(allocator.top == 80);
    REQUIRE(allocator.last == 64);

    // Freeing last block (and also the last valid one).
    BX_FREE(&allocator, first_realloced);
    REQUIRE(allocator.top == 8);
    REQUIRE(allocator.last == 0);
}


// -----------------------------------------------------------------------------
// DYNAMIC ARRAY
// -----------------------------------------------------------------------------

TEST_CASE("Dynamic Array", "[basic]")
{
    CrtAllocator allocator;

    DynamicArray<int> array = {};
    init(array, &allocator);
    REQUIRE(array.allocator == &allocator);

    reserve(array, 3);
    REQUIRE(array.size == 0);
    REQUIRE(array.capacity >= 3);

    int val = append(array, 10);
    REQUIRE(array.size == 1);
    REQUIRE(array[0] == 10);
    REQUIRE(val == 10);

    val = append(array, 20);
    REQUIRE(array.size == 2);
    REQUIRE(array[1] == 20);
    REQUIRE(val == 20);

    val = append(array, 30);
    REQUIRE(array.size == 3);
    REQUIRE(array[2] == 30);
    REQUIRE(val == 30);

    val = pop(array);
    REQUIRE(array.size == 2);
    REQUIRE(val == 30);

    resize(array, 10, 100);
    REQUIRE(array.size == 10);
    REQUIRE(array.capacity >= 10);

    for (u32 i = 2; i < array.size; i++)
    {
        REQUIRE(array[i] == 100);
    }

    deinit(array);
    REQUIRE(array.data == nullptr);
    REQUIRE(array.size == 0);
    REQUIRE(array.capacity == 0);
    REQUIRE(array.allocator == nullptr);
}


// -----------------------------------------------------------------------------
// MESH RECORDING
// -----------------------------------------------------------------------------

TEST_CASE("Mesh Recording", "[benchmark]")
{
    CrtAllocator allocator;

    SECTION("Torus")
    {
        DynamicArray<Vec3> vertices;
        init(vertices, &allocator);
        defer(deinit(vertices));

        constexpr int radial_resolution  = 100;
        constexpr int tubular_resolution = 250;

        const auto torus_vertex = [&](int index)
        {
            const float radius    = 0.50f;
            const float thickness = 0.15f;

            const int i = index / tubular_resolution;
            const int j = index % tubular_resolution;

            const float u =  6.28318530718f * j / tubular_resolution;
            const float v =  6.28318530718f * i / radial_resolution;

            const float x = (radius + thickness * cosf(v)) * cosf(u);
            const float y = (radius + thickness * cosf(v)) * sinf(u);
            const float z = thickness * sinf(v);

            append(vertices, HMM_Vec3(x, y, z));
        };

        for (int r0 = 0; r0 < radial_resolution; r0++)
        {
            const int r1 = (r0 + 1) % radial_resolution;

            for (int t0 = 0; t0 < tubular_resolution; t0++)
            {
                const int t1 = (t0 + 1) % tubular_resolution;

                const int i0 = r0 * tubular_resolution + t0;
                const int i1 = r0 * tubular_resolution + t1;
                const int i2 = r1 * tubular_resolution + t1;
                const int i3 = r1 * tubular_resolution + t0;

                torus_vertex(i0);
                torus_vertex(i1);
                torus_vertex(i2);
                torus_vertex(i3);
            }
        }

        constexpr u32 stack_size = 8_MB;

        void* stack_buffer = BX_ALIGNED_ALLOC(&allocator, stack_size, 16);
        defer(BX_ALIGNED_FREE(&allocator, stack_buffer, 16));

        StackAllocator stack_allocator;
        init(stack_allocator, stack_buffer, stack_size);

        MeshRecorder recorder;
        init(recorder, &stack_allocator);

        const auto submit = [&](int flags)
        {
            REQUIRE(recorder.vertex_count == 0);

            start(recorder, u32(flags));
            defer(end(recorder));

            const Mat4 transform = HMM_Mat4d(1.0f);

            for (u32 i = 0; i < vertices.size; i++)
            {
                const Vec3& v = vertices[i];

                (*recorder.store_vertex)(
                    (transform * HMM_Vec4(v.X, v.Y, v.Z, 1.0f)).XYZ,
                    recorder.attrib_state,
                    recorder
                );
            }

            REQUIRE(recorder.vertex_count == u32(radial_resolution * tubular_resolution * 6));

            return recorder.vertex_count;
        };

        BENCHMARK("Vertices Only")
        {
            return submit(PRIMITIVE_QUADS);
        };
    }
}


// -----------------------------------------------------------------------------
// EXAMPLES - COMMON SETUP
// -----------------------------------------------------------------------------
// We're relying on the preprocess to not include the `<mnm/mnm.h>` header again
// (that should work fine), and hijacking the main function for each example.
// Any additional header that the examples might include should also be included
// before (so far, there's only one inclusion of `<math.h>`).

#undef MNM_MAIN_NAME

namespace
{

struct ExampleTest
{
    int       (*run )(const ::mnm::rwr::Callbacks&) = nullptr;
    void      (*draw)(void)                         = nullptr;
    const char* name                                = nullptr;
    u8*         data                                = nullptr;
    int         width                               = 0;
    int         height                              = 0;
    int         screenshot                          = 0;
};

ExampleTest example_test = { ::mnm::rwr::run };

void example_draw(void)
{
    const f64 elapsed = g_ctx->total_time.elapsed;
    g_ctx->total_time.elapsed = 0.0;

    (*example_test.draw)();

    g_ctx->total_time.elapsed = elapsed;

    if (frame() == 0)
    {
        const int width  = pixel_width ();
        const int height = pixel_height();

        // NOTE : Memory is deallocated when MiNiMo terminates.
        example_test.data       = reinterpret_cast<u8*>(alloc(MEMORY_PERSISTENT, width * height * 4));
        example_test.width      = width;
        example_test.height     = height;
        example_test.screenshot = read_screen(example_test.data);
    }

    if (readable(example_test.screenshot))
    {
        if (bgfx::getRendererType() == bgfx::RendererType::Metal)
        {
            for (int i = 0, n = example_test.width * example_test.height * 4; i < n; i += 4)
            {
                bx::swap(example_test.data[i], example_test.data[i + 2]);
            }
        }

        char name[512];
        bx::snprintf(
            name,
            sizeof(name),
            "%s/%s_%s_%s_%.1f.png",
            MNM_ASSETS_TEST_OUTPUTS_DIR,
            example_test.name,
            BX_PLATFORM_NAME,
            bgfx::getRendererName(bgfx::getRendererType()),
            dpi()
        );

        int expected_width;
        int expected_height;
        u8* expected_data = stbi_load(name, &expected_width, &expected_height, nullptr, 4);

        if (expected_data)
        {
            defer(stbi_image_free(expected_data));

            REQUIRE(example_test.width  == expected_width );
            REQUIRE(example_test.height == expected_height);

            bool images_equal = true;

            for (int i = 0, n = expected_width * expected_height * 4; i < n; i++)
            {
                // TODO : Think whether pixel-perfect equality is necessary.
                if (images_equal && example_test.data[i] != expected_data[i])
                {
                    INFO(
                        "First failed pixel at ("  <<
                        ((i / 4) % expected_width) <<
                        ", "                       <<
                        ((i / 4) / expected_width) <<
                        ")"
                    );

                    images_equal = false;
                }

                const int diff = example_test.data[i] - expected_data[i];

                expected_data[i] = (i % 4) != 3 ? u8(diff < 0 ? -diff : diff) : 255;
            }

            if (!images_equal)
            {
                bx::snprintf(
                    name,
                    sizeof(name),
                    "%s_%s_%s_%.1f_diff.png",
                    example_test.name,
                    BX_PLATFORM_NAME,
                    bgfx::getRendererName(bgfx::getRendererType()),
                    dpi()
                );

                stbi_write_png(
                    name,
                    example_test.width,
                    example_test.height,
                    4,
                    expected_data,
                    example_test.width * 4
                );

                INFO("Mismatched diff saved to '" << name << "'.");
            }

            REQUIRE(images_equal);
        }
        else
        {
            bx::snprintf(
                name,
                sizeof(name),
                "%s_%s_%s_%.1f_result.png",
                example_test.name,
                BX_PLATFORM_NAME,
                bgfx::getRendererName(bgfx::getRendererType()),
                dpi()
            );

            stbi_write_png(
                name,
                example_test.width,
                example_test.height,
                4,
                example_test.data,
                example_test.width * 4
            );

            INFO("Result not found; saving appearance to '" << name << "'.");
            REQUIRE(false);
        }

        quit();
    }
}

int MNM_MAIN_NAME
(
    void (* init   )(void),
    void (* setup  )(void),
    void (* draw   )(void),
    void (* cleanup)(void)
)
{
    example_test.draw = draw;
    example_test.data = nullptr;

    return (*example_test.run)({ init, setup, example_draw, cleanup });
}

const char* prettify_example_name(const char* in, char* out, u32 size)
{
    // NOTE : Only for examples. Assumes `in` consists solely of lowercase
    //        letters and underscores.

    bool upper = true;
    u32  i     = 0;

    for (i = 0; i < size && in[i]; i++)
    {
        if (in[i] == '_')
        {
            out[i] = ' ';
            upper  = true;
        }
        else
        {
            ASSERT(
                *in >= 97 && *in <= 122,
                "Invalid example name character '%c'",
                in[i]
            );

            out[i] = in[i] - char(upper ? 32 : 0);
            upper  = false;
        }
    }

    out[i] = '\0';

    return out;
}

#define EXAMPLE_TEST_BEGIN(name) \
    namespace name \
    { \
        char        test_name[32]; \
        const char* image_name = #name; \

#define EXAMPLE_TEST_END \
        TEST_CASE( \
            prettify_example_name(image_name, test_name, sizeof(test_name)), \
            "[example][graphics]" \
        ) \
        { \
            example_test.name = image_name; \
            main(0, nullptr); \
        } \
    } // namespace name

} // unnamed namespace


// -----------------------------------------------------------------------------
// EXAMPLES
// -----------------------------------------------------------------------------
// Unfortunately, there's no nice solution to include a header based on some
// preprocessor variable, so it has to be split like this, but the example name
// only has to be repeated once.

EXAMPLE_TEST_BEGIN(hello_triangle)
#include "hello_triangle.c"
EXAMPLE_TEST_END

EXAMPLE_TEST_BEGIN(static_geometry)
#include "static_geometry.c"
EXAMPLE_TEST_END

EXAMPLE_TEST_BEGIN(transient_geometry)
#include "transient_geometry.c"
EXAMPLE_TEST_END

EXAMPLE_TEST_BEGIN(element_range)
#include "element_range.c"
EXAMPLE_TEST_END

EXAMPLE_TEST_BEGIN(vertex_alias)
#include "vertex_alias.c"
EXAMPLE_TEST_END

EXAMPLE_TEST_BEGIN(instancing)
#include "instancing.c"
EXAMPLE_TEST_END

EXAMPLE_TEST_BEGIN(normals)
#include "normals.c"
EXAMPLE_TEST_END

EXAMPLE_TEST_BEGIN(normals_autogen)
#include "normals_autogen.c"
EXAMPLE_TEST_END

EXAMPLE_TEST_BEGIN(custom_shader)
#include "custom_shader.c"
EXAMPLE_TEST_END

EXAMPLE_TEST_BEGIN(font_atlas)
#include "font_atlas.c"
EXAMPLE_TEST_END
