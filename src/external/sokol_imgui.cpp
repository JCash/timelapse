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

#include <sokol/sokol_app.h>
#include <sokol/sokol_gfx.h>
#include <sokol/util/sokol_imgui.h>
#include <sokol/sokol_time.h>
