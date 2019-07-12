#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#include <sokol/sokol_app.h>
#include <sokol/sokol_gfx.h>
#include <sokol/sokol_time.h>
#include <sokol/util/sokol_imgui.h>

#include <imgui/imgui.h>

#include "version.h"
#include "settings.h"
#include "font_source_code_pro_medium.h"
#include "font_source_code_pro_bold.h"

extern void imgui_set_style_photoshop();
extern void imgui_set_style_lightgreen();

struct FileContext {
    RevisionCtx* ctx;
    File* file;
    int current_revision;
    int prev_revision;
    const char* path;
} g_FileContext;

static const int COMMAND_NOP = 0;
static const int COMMAND_NEXT_CHUNK = 1;
static const int COMMAND_PREV_CHUNK = 2;
static const int COMMAND_NEXT_REVISION = 3;
static const int COMMAND_PREV_REVISION = 4;

struct Context {
    Settings settings;

    ImFont* font_regular;
    ImFont* font_bold;
    sg_image font_atlas;
    int input_command;
    int font_changed;
} g_Context;



static void update_font_texture() {
    ImGuiIO& io = ImGui::GetIO();

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
    sg_destroy_image(g_Context.font_atlas);
    g_Context.font_atlas = sg_make_image(&img_desc);
    io.Fonts->SetTexID((ImTextureID)(uintptr_t) g_Context.font_atlas.id);
}

static void load_fonts() {
    g_Context.font_changed = 0;

    ImGuiIO& io = ImGui::GetIO();
    ImFontConfig font_cfg;
    font_cfg.RasterizerMultiply = g_Context.settings.fontweight;

    if (g_Context.settings.fontpath_regular != 0) {
        font_cfg.FontDataOwnedByAtlas = true;
        g_Context.font_regular = io.Fonts->AddFontFromFileTTF(g_Context.settings.fontpath_regular, g_Context.settings.fontsize, &font_cfg);
    }
    if (!g_Context.font_regular) {
        font_cfg.FontDataOwnedByAtlas = false;
        g_Context.font_regular = io.Fonts->AddFontFromMemoryTTF((void*)SOURCECODEPRO_MEDIUM_TTF_DATA, SOURCECODEPRO_MEDIUM_TTF_LEN, g_Context.settings.fontsize, &font_cfg);
    }

    if (g_Context.settings.fontpath_bold != 0) {
        font_cfg.FontDataOwnedByAtlas = true;
        g_Context.font_bold = io.Fonts->AddFontFromFileTTF(g_Context.settings.fontpath_bold, g_Context.settings.fontsize, &font_cfg);
    }

    if (!g_Context.font_bold) {
        font_cfg.FontDataOwnedByAtlas = false;
        g_Context.font_bold = io.Fonts->AddFontFromMemoryTTF((void*)SOURCECODEPRO_BOLD_TTF_DATA, SOURCECODEPRO_BOLD_TTF_LEN, g_Context.settings.fontsize, &font_cfg);
    }

    update_font_texture();
}

static void reload_fonts() {
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->Clear();
    g_Context.font_regular = 0;
    g_Context.font_bold = 0;
    load_fonts();
}

static void imgui_show_settings() {
    ImGui::BeginGroup();
        g_Context.font_changed = 0;
        ImGui::Text("Fonts");
        g_Context.font_changed |= ImGui::SliderFloat("Size", &g_Context.settings.fontsize, 0.1f, 64.0f, "%.2f");
        g_Context.font_changed |= ImGui::SliderFloat("Weight", &g_Context.settings.fontweight, 0.1f, 3.0f, "%.2f");

        // char buffer[2048];
        // buffer[0] = 0;
        // if (g_Context.settings.fontpath_regular)
        //     snprintf(buffer, sizeof(buffer), "%s", g_Context.settings.fontpath_regular);
        // int font_regular_changed = ImGui::InputText("Font Regular", buffer, sizeof(buffer));
        // g_Context.font_changed |= font_regular_changed;
        // if (font_regular_changed) {
        //     if (g_Context.settings.fontpath_regular) {
        //         free((void*)g_Context.settings.fontpath_regular);
        //         g_Context.settings.fontpath_regular = 0;
        //     }
        //     if (buffer[0] != 0)
        //         g_Context.settings.fontpath_regular = strdup(buffer);
        // }

        // buffer[0] = 0;
        // if (g_Context.settings.fontpath_bold)
        //     snprintf(buffer, sizeof(buffer), "%s", g_Context.settings.fontpath_bold);
        // int font_bold_changed = ImGui::InputText("Font Bold", buffer, sizeof(buffer));
        // g_Context.font_changed |= font_bold_changed;
        // if (font_bold_changed) {
        //     if (g_Context.settings.fontpath_bold) {
        //         free((void*)g_Context.settings.fontpath_bold);
        //         g_Context.settings.fontpath_bold = 0;
        //     }
        //     if (buffer[0] != 0)
        //         g_Context.settings.fontpath_bold = strdup(buffer);
        // }
    ImGui::EndGroup();
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

static void update_revision() {
    if (g_FileContext.current_revision != g_FileContext.prev_revision) {
        free_file(g_FileContext.file);
        g_FileContext.file = get_file_from_revision(g_FileContext.ctx, g_FileContext.current_revision+1);
        g_FileContext.prev_revision = g_FileContext.current_revision;
    }
}

// Get the liune start of the previous chunk
static int get_prev_chunk_from_line(int line) {
    const Revision* revision = g_FileContext.ctx == 0 ? 0 : &g_FileContext.ctx->revisions[g_FileContext.current_revision];
    for (int i = revision->num_chunks-1; i >= 0; --i) {
        Chunk* chunk = &revision->chunks[i];
        if (chunk->after.start < line) {
            return chunk->after.start;
        }
    }
    return 0;
}

// Get the liune start of the next chunk
static int get_next_chunk_from_line(int line) {
    const Revision* revision = g_FileContext.ctx == 0 ? 0 : &g_FileContext.ctx->revisions[g_FileContext.current_revision];
    const File* file = g_FileContext.file;
    for (int i = 0; i < revision->num_chunks; ++i) {
        Chunk* chunk = &revision->chunks[i];
        if (chunk->after.start > line) {
            return chunk->after.start;
        }
    }

    return file->num_lines;
}

static void imgui_show_controls() {
    const Revision* revision = g_FileContext.ctx == 0 ? 0 : &g_FileContext.ctx->revisions[g_FileContext.current_revision];

    ImGui::Text("File: %s", g_FileContext.path);
    ImGui::Text("Revision: %d / %d : %s", g_FileContext.current_revision+1, g_FileContext.ctx->num_revisions, revision->commit);
    ImGui::SetNextItemWidth(-16);
    ImGui::SliderInt("", &g_FileContext.current_revision, 0, g_FileContext.ctx->num_revisions-1, "");
}

static void imgui_show_main_file() {
    ImGuiStyle* style = &ImGui::GetStyle();

    const Revision* revision = g_FileContext.ctx == 0 ? 0 : &g_FileContext.ctx->revisions[g_FileContext.current_revision];
    const File* file = g_FileContext.file;

    ImFont* font_regular = g_Context.font_regular;
    ImFont* font_bold = g_Context.font_bold;

    ImVec2 textsize;
    textsize = ImGui::CalcTextSize("0");

    int num_digits = 0;
    int num_lines = file->num_lines;
    while (num_lines > 0) {
        num_digits++;
        num_lines /= 10;
    }
    int number_column_width = textsize.x * num_digits + style->ItemSpacing.x * 2;

    ImGui::BeginGroup();

    ImGui::Columns(4);

    ImGui::SetColumnWidth(0, 32);
    ImGui::Text("%d", g_FileContext.current_revision);

    ImGui::NextColumn();
    ImGui::SetColumnWidth(1, 80);

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
            ImGui::PushFont(font_bold);
        }

        ImGui::TextColored(colors[highlight], "%7s", line_revision);

        if (highlight) {
            ImGui::PopFont();
        }
    }

    // Line numbers
    ImGui::NextColumn();
    ImGui::SetColumnWidth(2, number_column_width);

    for (int i = 0; i < file->num_lines; ++i) {
        const char* line_revision = file->line_revisions[i];

        int highlight = 0;
        if (strcmp(revision->commit, line_revision)==0) {
            highlight = 1;
            ImGui::PushFont(font_bold);
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
        if (strcmp(revision->commit, line_revision)==0) {
            highlight = 1;
            ImGui::PushFont(font_bold);
        }

        ImGui::TextColored(colors[highlight], "%s", buffer);

        if (highlight) {
            ImGui::PopFont();
        }
    }

    ImGui::EndGroup();

    if (g_Context.input_command) {
        int current_line = ImGui::GetScrollY() / ImGui::GetTextLineHeightWithSpacing();

        switch(g_Context.input_command) {
        case COMMAND_PREV_CHUNK: ImGui::SetScrollY(ImGui::GetTextLineHeightWithSpacing() * get_prev_chunk_from_line(current_line)); break;
        case COMMAND_NEXT_CHUNK: ImGui::SetScrollY(ImGui::GetTextLineHeightWithSpacing() * get_next_chunk_from_line(current_line)); break;
        case COMMAND_PREV_REVISION: g_FileContext.current_revision--; if (g_FileContext.current_revision < 0) g_FileContext.current_revision = 0; break;
        case COMMAND_NEXT_REVISION: g_FileContext.current_revision++; if (g_FileContext.current_revision >= g_FileContext.ctx->num_revisions) g_FileContext.current_revision = g_FileContext.ctx->num_revisions-1; break;
        }
    }
    g_Context.input_command = COMMAND_NOP;
}

static const char* get_line(const char** cursor, const char* end) {
    const char* c = *cursor;
    const char* line = c;
    while (c < end && !(*c == 0 || *c == '\n')) {
        c++;
    }
    *cursor = ++c;
    return line[0] != 0 ? line : 0;
}

static void imgui_show_chunks() {
    ImGuiStyle* style = &ImGui::GetStyle();
    ImVec4 colors[3] = {style->Colors[ImGuiCol_Text], ImVec4(0.5f,1.0f,0.5f,1.0f), ImVec4(1.0f,0.5f,0.5f,1.0f)};

    const Revision* revision = g_FileContext.ctx == 0 ? 0 : &g_FileContext.ctx->revisions[g_FileContext.current_revision];

    const char* p = revision->chunk_begin;
    const char* pend = revision->chunk_end;
    while (p < pend) {
        const char* line = get_line(&p, pend);
        if (line) {
            int size = p - line;
            char* l = (char*)alloca(size + 1);
            strncpy(l, line, size);
            l[size] = 0;
            int color = l[0] == '-' ? 2 : (l[0] == '+' ? 1 : 0);
            ImGui::TextColored(colors[color], "%s", l);
        }
    }
}

static void imgui_show_details() {
    const Revision* revision = g_FileContext.ctx == 0 ? 0 : &g_FileContext.ctx->revisions[g_FileContext.current_revision];

    ImGui::Text("Commit: %s\tDate: %s", revision->commit, revision->date);
    ImGui::Text("Author: %s <%s>", revision->author, revision->email);
    ImGui::Text("Body: %s", revision->body);
}

static void imgui_show_revision(int w, int h) {
    int win_flags = ImGuiWindowFlags_NoResize |
                    ImGuiWindowFlags_NoTitleBar |
                    ImGuiWindowFlags_NoMove |
                    ImGuiWindowFlags_NoCollapse;

    int top_window_height = ImGui::GetTextLineHeightWithSpacing() * 4;

    int bottom_window_height = ImGui::GetTextLineHeightWithSpacing() * 6;

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(w, h - bottom_window_height));
    ImGui::Begin("Controls", 0, win_flags);

        imgui_show_controls();

        ImGuiTabBarFlags tab_bar_flags = ImGuiTabBarFlags_None;
        if (ImGui::BeginTabBar("MainTabs", tab_bar_flags))
        {
            if (ImGui::BeginTabItem("File"))
            {
                ImGui::BeginChild("FileView");
                    imgui_show_main_file();
                ImGui::EndChild();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Chunks"))
            {
                ImGui::BeginChild("ChunksView");
                    imgui_show_chunks();
                ImGui::EndChild();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Settings"))
            {
                ImGui::BeginChild("SettingsView");
                    imgui_show_settings();
                ImGui::EndChild();
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }

    ImGui::End();

    ImGui::SetNextWindowPos(ImVec2(0, h - bottom_window_height));
    ImGui::SetNextWindowSize(ImVec2(w, bottom_window_height));
    ImGui::Begin("Details", 0, win_flags);

        imgui_show_details();

    ImGui::End();
}

static void on_app_frame(void) {
    int appw = sapp_width();
    int apph = sapp_height();

    static uint64_t last_time = 0;
    double dt = stm_sec(stm_laptime(&last_time));

    if (g_Context.font_changed) {
        reload_fonts();
        g_Context.font_changed = 0;
    }

    float dpi = sapp_dpi_scale();
    simgui_new_frame(appw, apph, dt);

    int w = appw / dpi;
    int h = apph / dpi;

    update_revision();

    if (g_FileContext.ctx) {
        imgui_show_revision(w, h);
    }

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
    char buffer[4096];
    settings_write(get_settings_path(buffer, sizeof(buffer)), &g_Context.settings);
    settings_destroy(&g_Context.settings);
    simgui_shutdown();
    sg_shutdown();
}

static void on_app_event(const sapp_event* event) {
    if (simgui_handle_event(event)) {
        return;
    }

    if (event->type == SAPP_EVENTTYPE_KEY_DOWN) {
        if (event->key_code == SAPP_KEYCODE_ESCAPE) {
            sapp_quit();
        }

        switch (event->key_code) {
        case SAPP_KEYCODE_UP:   g_Context.input_command = COMMAND_PREV_CHUNK; break;
        case SAPP_KEYCODE_DOWN: g_Context.input_command = COMMAND_NEXT_CHUNK; break;
        case SAPP_KEYCODE_LEFT: g_Context.input_command = COMMAND_PREV_REVISION; break;
        case SAPP_KEYCODE_RIGHT:g_Context.input_command = COMMAND_NEXT_REVISION; break;
        default:                g_Context.input_command = COMMAND_NOP;
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
    char buffer[4096];
    settings_init(&g_Context.settings);
    settings_read(get_settings_path(buffer, sizeof(buffer)), &g_Context.settings);

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
