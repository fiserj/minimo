#include <imgui_draw.cpp>

extern "C" unsigned int stb_decompress_exported
(
    unsigned char*       output,
    const unsigned char* i,
    unsigned int         length
)
{
    return stb_decompress(output, i, length);
}
