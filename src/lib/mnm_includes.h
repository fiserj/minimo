#pragma once

#include <assert.h>
#include <stdint.h>

#include <type_traits>

#include <bx/bx.h>
#include <bx/allocator.h>

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
