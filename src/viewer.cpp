
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
#include "font_source_code_pro_medium.h"
#include "font_source_code_pro_bold.h"

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
    int fontsize;
};

struct FileContext {
    RevisionCtx* ctx;
    File* file;
    int current_revision;
    int prev_revision;
    const char* path;
} g_FileContext;


struct Context {
    Settings settings;

    ImFont* font_regular;
    ImFont* font_bold;
    sg_image font_atlas;
} g_Context;


static void settings_init(Settings* s) {
    s->width = 800;
    s->height = 600;
    s->fontsize = 16;
}

static void settings_read(const char* path, Settings* s) {
    int size;
    void* data = read_file(path, &size);
    if (!data)
        return;
}

static void load_fonts() {

    ImGuiIO& io = ImGui::GetIO();
    ImFontConfig font_cfg;
    font_cfg.FontDataOwnedByAtlas = false;

    //g_Context.font_regular = io.Fonts->AddFontFromMemoryTTF((void*)VICTORMONO_REGULAR_TTF_DATA, VICTORMONO_REGULAR_TTF_LEN, (float)g_Context.settings.fontsize, &font_cfg);
    //g_Context.font_regular = io.Fonts->AddFontFromMemoryTTF((void*)SOURCECODEPRO_REGULAR_TTF_DATA, SOURCECODEPRO_REGULAR_TTF_LEN, (float)g_Context.settings.fontsize, &font_cfg);
    g_Context.font_regular = io.Fonts->AddFontFromMemoryTTF((void*)SOURCECODEPRO_MEDIUM_TTF_DATA, SOURCECODEPRO_MEDIUM_TTF_LEN, (float)g_Context.settings.fontsize, &font_cfg);
    g_Context.font_bold = io.Fonts->AddFontFromMemoryTTF((void*)SOURCECODEPRO_BOLD_TTF_DATA, SOURCECODEPRO_BOLD_TTF_LEN, (float)g_Context.settings.fontsize, &font_cfg);

    io.Fonts->Build();

    {
        unsigned char* font_pixels;
        int font_width, font_height;
        io.Fonts->GetTexDataAsRGBA32(&font_pixels, &font_width, &font_height);
        sg_image_desc img_desc = { };
        img_desc.width = font_width;
        img_desc.height = font_height;
        img_desc.pixel_format = SG_PIXELFORMAT_RGBA8;
        img_desc.wrap_u = SG_WRAP_CLAMP_TO_EDGE;
        img_desc.wrap_v = SG_WRAP_CLAMP_TO_EDGE;
        img_desc.min_filter = SG_FILTER_LINEAR;
        img_desc.mag_filter = SG_FILTER_LINEAR;
        img_desc.content.subimage[0][0].ptr = font_pixels;
        img_desc.content.subimage[0][0].size = font_width * font_height * sizeof(uint32_t);
        img_desc.label = "sokol-imgui-font";
        g_Context.font_atlas = sg_make_image(&img_desc);
        io.Fonts->SetTexID((ImTextureID)(uintptr_t) g_Context.font_atlas.id);
    }
}

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
    ctx.no_default_font = true;
    simgui_setup(&ctx);

    load_fonts();

    //imgui_set_style_photoshop();
    //imgui_set_style_lightgreen();
    ImGui::StyleColorsDark(&ImGui::GetStyle());
    //ImGui::StyleColorsLight(&ImGui::GetStyle());
}


static void on_app_frame(void) {
    int appw = sapp_width();
    int apph = sapp_height();
    double dt = stm_sec(stm_laptime(&last_time));

    float dpi = sapp_dpi_scale();
    simgui_new_frame(appw, apph, dt);

    int w = appw / dpi;
    int h = apph / dpi;

    int win_flags = ImGuiWindowFlags_NoResize |
                    ImGuiWindowFlags_NoTitleBar |
                    ImGuiWindowFlags_NoMove |
                    ImGuiWindowFlags_NoCollapse;

    ImFont* font = g_Context.font_regular;

    int top_window_height = 80;
    int bottom_window_height = 80;
    int main_window_height = h - top_window_height - bottom_window_height;

    const Revision* revision = g_FileContext.ctx == 0 ? 0 : &g_FileContext.ctx->revisions[g_FileContext.current_revision];
    if (revision) {

        if (g_FileContext.current_revision != g_FileContext.prev_revision) {
            free_file(g_FileContext.file);
            g_FileContext.file = get_file_from_revision(g_FileContext.ctx, g_FileContext.current_revision+1);
            g_FileContext.prev_revision = g_FileContext.current_revision;
        }
        const File* file = g_FileContext.file;

        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImVec2(w, top_window_height));
        ImGui::Begin("Controls", 0, win_flags);

            ImGui::Text("File: %s", g_FileContext.path);
            ImGui::Text("Revision: %d / %d : %s", g_FileContext.current_revision+1, g_FileContext.ctx->num_revisions, revision->commit);
            ImGui::SetNextItemWidth(-16);
            ImGui::SliderInt("", &g_FileContext.current_revision, 0, g_FileContext.ctx->num_revisions-1, "");

        ImGui::End();

        ImGui::SetNextWindowPos(ImVec2(0, top_window_height));
        ImGui::SetNextWindowSize(ImVec2(w, main_window_height));
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
            if (strcmp(revision->commit, line_revision)==0) {
                highlight = 1;
                ImGui::PushFont(g_Context.font_bold);
            }

            ImGui::TextColored(colors[highlight], "%7s", line_revision);

            if (highlight) {
                ImGui::PopFont();
            }
        }

        // Line numbers
        ImGui::NextColumn();
        ImGui::SetColumnWidth(2, 32);

        for (int i = 0; i < file->num_lines; ++i) {
            const char* line_revision = file->line_revisions[i];

            int highlight = 0;
            if (strcmp(revision->commit, line_revision)==0) {
                highlight = 1;
                ImGui::PushFont(g_Context.font_bold);
            }

            ImGui::TextColored(colors[highlight], "%d", i);

            if (highlight) {
                ImGui::PopFont();
            }
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

        ImGui::EndGroup();

        ImGui::End();
    }

    ImGui::SetNextWindowPos(ImVec2(0, top_window_height + main_window_height));
    ImGui::SetNextWindowSize(ImVec2(w, bottom_window_height));
    ImGui::Begin("Details", 0, win_flags);

    //ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
    ImGui::Text("Commit: %s\tDate: %s", revision->commit, revision->date);
    ImGui::Text("Author: %s <%s>", revision->author, revision->email);
    ImGui::Text("Body: %s", revision->body);

    ImGui::End();

    // the sokol_gfx draw pass
    sg_pass_action pass_action = {};
    pass_action.colors[0].action = SG_ACTION_CLEAR;
    pass_action.colors[0].val[0] = 0.0f;
    pass_action.colors[0].val[1] = 0.0f;
    pass_action.colors[0].val[2] = 0.0f;
    pass_action.colors[0].val[3] = 1.0f;
    sg_begin_default_pass(&pass_action, appw, apph);
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

    settings_init(&g_Context.settings);

    sapp_desc desc = {};
    desc.init_cb = on_app_init;
    desc.frame_cb = on_app_frame;
    desc.cleanup_cb = on_app_cleanup;
    desc.event_cb = on_app_event;
    desc.width = g_Context.settings.width;
    desc.height = g_Context.settings.height;
    desc.window_title = "Timelapse";
#if defined(__APPLE__)
    desc.high_dpi = true;
#else
    desc.high_dpi = false;
#endif

    const char* path = 0;
    if (argc > 1) {
        path = argv[1];
    }

    memset(&g_FileContext, 0, sizeof(g_FileContext));
    if (path)
        load_file(path);

    return desc;
}
