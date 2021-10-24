#include <imgui_draw.cpp>

extern "C" ImFont* ImGui_Patch_ImFontAtlas_AddFontFromMemoryCompressedTTF
(
    ImFontAtlas* atlas,
    const void*  compressed_data,
    unsigned int compressed_size,
    float        cap_height
)
{
    const unsigned int decompressed_size = stb_decompress_length(reinterpret_cast<const unsigned char*>(compressed_data));
    unsigned char*     decompressed_data = static_cast<unsigned char*>(IM_ALLOC(decompressed_size));

    stb_decompress(
        decompressed_data,
        reinterpret_cast<const unsigned char*>(compressed_data),
        compressed_size
    );

    stbtt_fontinfo info = {};
    stbtt_InitFont(&info, decompressed_data, 0);

    float pixel_height_for_cap_height = cap_height;

    if (const int table = stbtt__find_table(info.data, info.fontstart, "OS/2"))
    {
        if (ttUSHORT(info.data + table) >= 2) // Version.
        {
            const float default_cap_height = ttSHORT(info.data + table + 88); // sCapHeight.

            int ascent, descent;
            stbtt_GetFontVMetrics(&info, &ascent, &descent, nullptr);

            pixel_height_for_cap_height = (ascent - descent) * cap_height / default_cap_height;
        }
    }

    return atlas->AddFontFromMemoryTTF(
        decompressed_data,
        decompressed_size,
        pixel_height_for_cap_height
    );
}
