#include <imgui/imgui.h>

#if defined(__APPLE__)
    #define SOKOL_METAL
#elif defined(WIN32)
    #define SOKOL_D3D11
#elif defined(__EMSCRIPTEN__)
    #define SOKOL_GLES2
#else
    #error "No GFX Backend Specified"
#endif

#define SOKOL_IMPL
#define SOKOL_IMGUI_IMPL

#include "sokol_app.h"
#include "sokol_gfx.h"
#include "util/sokol_imgui.h"
#include "sokol_time.h"
