#include <astc-codec/astc-codec.h>

#include <assert.h>

namespace astc_codec
{

bool ASTCDecompressToRGBA(const uint8_t *, size_t, size_t, size_t, FootprintType, uint8_t *, size_t, size_t)
{
    assert(false && "This should never be called.");
    return false;
}

} // namespace astc_codec
