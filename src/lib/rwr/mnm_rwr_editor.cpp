#include "mnm_rwr_lib.cpp"

namespace mnm
{

namespace rwr
{

namespace ed
{

namespace
{

// -----------------------------------------------------------------------------
// EDITOR CALLBACKS
// -----------------------------------------------------------------------------

void init(void)
{
    // ...
}

void setup(void)
{
    // ...
}

void draw(void)
{
    // ...
}

void cleanup(void)
{
    // ...
}

} // unnamed namespace

} // namespace ed

} // namespace rwr

} // namespace mnm


// -----------------------------------------------------------------------------
// MAIN EDITOR ENTRY
// -----------------------------------------------------------------------------

int main(int, char**)
{
    // TODO : Argument processing (file to open, etc.).

    return mnm_run(
        ed::init,
        ed::setup,
        ed::draw,
        ed::cleanup
    );
}
