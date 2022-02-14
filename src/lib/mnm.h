#pragma once

#include <mnm/mnm.h>

#include <assert.h>
#include <stdint.h>

#include <type_traits>

#include <bx/bx.h>
#include <bx/allocator.h>
#include <bx/endian.h>
#include <bx/mutex.h>
#include <bx/pixelformat.h>
#include <bx/platform.h>
#include <bx/timer.h>

BX_PRAGMA_DIAGNOSTIC_PUSH();
BX_PRAGMA_DIAGNOSTIC_IGNORED_MSVC(4127);
BX_PRAGMA_DIAGNOSTIC_IGNORED_MSVC(4514);
BX_PRAGMA_DIAGNOSTIC_IGNORED_MSVC(4820);
#include <bgfx/bgfx.h>
#include <bgfx/embedded_shader.h>
BX_PRAGMA_DIAGNOSTIC_POP();

BX_PRAGMA_DIAGNOSTIC_PUSH();
BX_PRAGMA_DIAGNOSTIC_IGNORED_MSVC(4820);
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
BX_PRAGMA_DIAGNOSTIC_POP();

BX_PRAGMA_DIAGNOSTIC_PUSH();
BX_PRAGMA_DIAGNOSTIC_IGNORED_CLANG_GCC("-Wmissing-field-initializers");
BX_PRAGMA_DIAGNOSTIC_IGNORED_CLANG_GCC("-Wnested-anon-types");
BX_PRAGMA_DIAGNOSTIC_IGNORED_CLANG_GCC("-Wunused-function");
BX_PRAGMA_DIAGNOSTIC_IGNORED_MSVC(4505);
BX_PRAGMA_DIAGNOSTIC_IGNORED_MSVC(4514);
#define HANDMADE_MATH_IMPLEMENTATION
#define HMM_STATIC
#include <HandmadeMath.h>
BX_PRAGMA_DIAGNOSTIC_POP();

#include "mnm_base.h"
#include "mnm_consts.h"
#include "mnm_array.h"
#include "mnm_stack.h"
#include "mnm_input.h"
#include "mnm_vertex_attribs.h"
