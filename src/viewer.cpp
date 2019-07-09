
#if defined(__APPLE__)
    #define SOKOL_METAL
#elif defined(WIN32)
    #define SOKOL_D3D11
#elif defined(__EMSCRIPTEN__)
    #define SOKOL_GLES2
#else
    #error "No GFX Backend Specified"
#endif

#include <sokol/sokol_app.h>
#include <sokol/sokol_gfx.h>
#include <sokol/sokol_time.h>
#include <sokol/util/sokol_imgui.h>

#include <imgui/imgui.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#include <magu/ini.h>

#include "misc.h"
#include "version.h"

extern void imgui_sokol_event(const sapp_event* event);
extern void imgui_setup();
extern void imgui_teardown();

extern void imgui_set_style_photoshop();
extern void imgui_set_style_lightgreen();

uint64_t last_time = 0;

typedef struct {
    float mvp[16];
} vs_params_t;


float rx = 0.0f;
float ry = 0.0f;
int update_count = 0;

extern const char *vs_src, *fs_src;

struct Settings {
    int width;
    int height;
    //const char* fontpath;
    //int fontsize;
} g_Settings;

struct FileContext {
    RevisionCtx* ctx;
    File* file;
    int current_revision;
    int prev_revision;
    const char* path;
} g_FileContext;

static void settings_init(Settings* s) {
    s->width = 800;
    s->height = 600;
}

static void settings_read(const char* path, Settings* s) {
    int size;
    void* data = read_file(path, &size);
    if (!data)
        return;

    s->width = 800;
    s->height = 600;
}



static sg_pass_action pass_action;

static void on_app_init(void) {
    sg_desc desc = { };
    desc.mtl_device = sapp_metal_get_device();
    desc.mtl_renderpass_descriptor_cb = sapp_metal_get_renderpass_descriptor;
    desc.mtl_drawable_cb = sapp_metal_get_drawable;
    desc.d3d11_device = sapp_d3d11_get_device();
    desc.d3d11_device_context = sapp_d3d11_get_device_context();
    desc.d3d11_render_target_view_cb = sapp_d3d11_get_render_target_view;
    desc.d3d11_depth_stencil_view_cb = sapp_d3d11_get_depth_stencil_view;
    desc.gl_force_gles2 = sapp_gles2();
    sg_setup(&desc);
    stm_setup();

    simgui_desc_t ctx = {};
    ctx.dpi_scale = sapp_dpi_scale();
    simgui_setup(&ctx);

    // initial clear color
    pass_action.colors[0].action = SG_ACTION_CLEAR;
    pass_action.colors[0].val[0] = 0.0f;
    pass_action.colors[0].val[1] = 0.0f;
    pass_action.colors[0].val[2] = 0.0f;
    pass_action.colors[0].val[3] = 1.0f;

    //imgui_set_style_photoshop();
    //imgui_set_style_lightgreen();
    ImGui::StyleColorsDark(&ImGui::GetStyle());
    //ImGui::StyleColorsLight(&ImGui::GetStyle());
}


static void on_app_frame(void) {
    int w = sapp_width();
    int h = sapp_height();
    double dt = stm_sec(stm_laptime(&last_time));

    simgui_new_frame(w, h, dt);

    int win_flags = ImGuiWindowFlags_NoResize |
                    ImGuiWindowFlags_NoTitleBar |
                    ImGuiWindowFlags_NoMove |
                    ImGuiWindowFlags_NoCollapse;

    const Revision* revision = g_FileContext.ctx == 0 ? 0 : &g_FileContext.ctx->revisions[g_FileContext.current_revision];
    if (revision) {

        if (g_FileContext.current_revision != g_FileContext.prev_revision) {
            free_file(g_FileContext.file);
            g_FileContext.file = get_file_from_revision(g_FileContext.ctx, g_FileContext.current_revision+1);
            g_FileContext.prev_revision = g_FileContext.current_revision;
        }
        const File* file = g_FileContext.file;

        int top_window_height = 80;

        ImGui::SetNextWindowPos(ImVec2(0, 0));
        //ImGui::SetNextWindowSize(ImVec2(w, (h*1)/7));
        ImGui::SetNextWindowSize(ImVec2(w, top_window_height));
        //ImGui::SetNextWindowContentSize(ImVec2(w, (h*1)/7));
        ImGui::Begin("Controls", 0, win_flags);

            ImGui::Text("File: %s", g_FileContext.path);
            ImGui::Text("Revision: %d / %d : %s", g_FileContext.current_revision+1, g_FileContext.ctx->num_revisions, revision->commit);
            ImGui::SliderInt("", &g_FileContext.current_revision, 0, g_FileContext.ctx->num_revisions-1, "");

        ImGui::End();

        //ImGui::SetNextWindowPos(ImVec2(0, (h*1)/7));
        ImGui::SetNextWindowPos(ImVec2(0, top_window_height));
        ImGui::SetNextWindowSize(ImVec2(w, (h*4)/7));
        ImGui::Begin("Main", 0, win_flags);

        ImGui::BeginGroup();
        ImGui::Columns(4);

        ImGui::SetColumnWidth(0, 32);
        ImGui::Text("%d", g_FileContext.current_revision);

        ImGui::NextColumn();
        ImGui::SetColumnWidth(1, 80);

        ImGuiStyle* style = &ImGui::GetStyle();
        ImVec4 colors[2] = {style->Colors[ImGuiCol_Text], ImVec4(0.5f,1.0f,0.5f,1.0f)};

        const char* prev_revision = "";
        for (int i = 0; i < file->num_lines; ++i) {
            const char* line_revision = file->line_revisions[i];
            if (prev_revision == line_revision)
                line_revision = "";
            prev_revision = file->line_revisions[i];

            int highlight = 0;
            if (strcmp(revision->commit, line_revision)==0)
                highlight = 1;

            ImGui::TextColored(colors[highlight], "%7s", line_revision);
        }

        ImGui::NextColumn();
        // Line numbers
        ImGui::SetColumnWidth(2, 32);

        for (int i = 0; i < file->num_lines; ++i) {
            const char* line_revision = file->line_revisions[i];

            int highlight = 0;
            if (strcmp(revision->commit, line_revision)==0)
                highlight = 1;

            ImGui::TextColored(colors[highlight], "%d", i);
        }

        // The contents of the file
        ImGui::NextColumn();

        for (int i = 0; i < file->num_lines; ++i) {
            const char* line_revision = file->line_revisions[i];
            const char* line = file->lines[i];
            const char* lineend = strchr(line, '\n');

            char* buffer;
            if (lineend) {
                int size = lineend-line;
                buffer = (char*)alloca(size+1);
                memcpy(buffer, line, size);
                buffer[size] = 0;
            } else {
                buffer = (char*)line;
            }

            int highlight = 0;
            if (strcmp(revision->commit, line_revision)==0)
                highlight = 1;

            ImGui::TextColored(colors[highlight], "%s", buffer);
        }

        //ImGui::Columns(1);
        ImGui::EndGroup();

        ImGui::End();
    }

    ImGui::SetNextWindowPos(ImVec2(0, (h*5)/7));
    ImGui::SetNextWindowSize(ImVec2(w, (h*2)/7));
    ImGui::Begin("Details", 0, win_flags);

    //ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
    ImGui::Text("Commit: %s\tDate: %s", revision->commit, revision->date);
    ImGui::Text("Author: %s <%s>", revision->author, revision->email);
    ImGui::Text("Body: %s", revision->body);

    ImGui::End();

    // the sokol_gfx draw pass
    sg_begin_default_pass(&pass_action, w, h);
    simgui_render();
    sg_end_pass();
    sg_commit();
}

static void on_app_cleanup(void) {
    simgui_shutdown();
    sg_shutdown();
}

static void log_msg(const char* msg) {
    printf("%s\n", msg);
    fflush(stdout);
}


static void on_app_event(const sapp_event* event) {
    if (simgui_handle_event(event)) {
        return;
    }

    if (event->type == SAPP_EVENTTYPE_KEY_DOWN) {
        if (event->key_code == SAPP_KEYCODE_ESCAPE) {
            sapp_quit();
        }
    }
    else if(event->type == SAPP_EVENTTYPE_RESIZED) {
        //printf("RESIZED: %d, %d\n", event->framebuffer_width, event->framebuffer_height);
    }
}

static void unload_file() {
    free_file(g_FileContext.file);
    free_revisions(g_FileContext.ctx);
}

static void load_file(const char* path) {
    unload_file();

    g_FileContext.ctx = get_revisions(path);
    if (!g_FileContext.ctx)
        return;
    g_FileContext.current_revision = g_FileContext.ctx->num_revisions - 1;
    g_FileContext.prev_revision = -1;
    g_FileContext.path = path;
}

sapp_desc sokol_main(int argc, char* argv[]) {

    settings_init(&g_Settings);

    sapp_desc desc = {};
    desc.init_cb = on_app_init;
    desc.frame_cb = on_app_frame;
    desc.cleanup_cb = on_app_cleanup;
    desc.event_cb = on_app_event;
    desc.width = g_Settings.width;
    desc.height = g_Settings.height;
    desc.window_title = "Timelapse";
// #if defined(__APPLE__)
//     desc.high_dpi = true;
// #else
//     desc.high_dpi = false;
// #endif

    const char* path = 0;
    if (argc > 1) {
        path = argv[1];
    }

    memset(&g_FileContext, 0, sizeof(g_FileContext));
    if (path)
        load_file(path);

    return desc;
}
