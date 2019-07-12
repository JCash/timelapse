
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


#include <misc/freetype/imgui_freetype.h>
#include <misc/freetype/imgui_freetype.cpp>

const char* get_text();


struct FreeTypeTest
{
    enum FontBuildMode
    {
        FontBuildMode_FreeType,
        FontBuildMode_Stb
    };

    FontBuildMode BuildMode;
    bool          WantRebuild;
    float         FontsMultiply;
    int           FontsPadding;
    unsigned int  FontsFlags;

    FreeTypeTest()
    {
        BuildMode = FontBuildMode_FreeType;
        WantRebuild = true;
        FontsMultiply = 1.0f;
        FontsPadding = 1;
        FontsFlags = 0;
    }

    // Call _BEFORE_ NewFrame()
    bool UpdateRebuild()
    {
        if (!WantRebuild)
            return false;
        ImGuiIO& io = ImGui::GetIO();
        io.Fonts->TexGlyphPadding = FontsPadding;
        for (int n = 0; n < io.Fonts->ConfigData.Size; n++)
        {
            ImFontConfig* font_config = (ImFontConfig*)&io.Fonts->ConfigData[n];
            font_config->RasterizerMultiply = FontsMultiply;
            font_config->RasterizerFlags = (BuildMode == FontBuildMode_FreeType) ? FontsFlags : 0x00;
        }
        if (BuildMode == FontBuildMode_FreeType)
            ImGuiFreeType::BuildFontAtlas(io.Fonts, FontsFlags);
        else if (BuildMode == FontBuildMode_Stb)
            io.Fonts->Build();
        WantRebuild = false;
        return true;
    }

    // Call to draw interface
    void ShowFreetypeOptionsWindow()
    {
        ImGui::Begin("FreeType Options");
        ImGui::ShowFontSelector("Fonts");
        WantRebuild |= ImGui::RadioButton("FreeType", (int*)&BuildMode, FontBuildMode_FreeType);
        ImGui::SameLine();
        WantRebuild |= ImGui::RadioButton("Stb (Default)", (int*)&BuildMode, FontBuildMode_Stb);
        WantRebuild |= ImGui::DragFloat("Multiply", &FontsMultiply, 0.001f, 0.0f, 2.0f);
        WantRebuild |= ImGui::DragInt("Padding", &FontsPadding, 0.1f, 0, 16);
        if (BuildMode == FontBuildMode_FreeType)
        {
            WantRebuild |= ImGui::CheckboxFlags("NoHinting",     &FontsFlags, ImGuiFreeType::NoHinting);
            WantRebuild |= ImGui::CheckboxFlags("NoAutoHint",    &FontsFlags, ImGuiFreeType::NoAutoHint);
            WantRebuild |= ImGui::CheckboxFlags("ForceAutoHint", &FontsFlags, ImGuiFreeType::ForceAutoHint);
            WantRebuild |= ImGui::CheckboxFlags("LightHinting",  &FontsFlags, ImGuiFreeType::LightHinting);
            WantRebuild |= ImGui::CheckboxFlags("MonoHinting",   &FontsFlags, ImGuiFreeType::MonoHinting);
            WantRebuild |= ImGui::CheckboxFlags("Bold",          &FontsFlags, ImGuiFreeType::Bold);
            WantRebuild |= ImGui::CheckboxFlags("Oblique",       &FontsFlags, ImGuiFreeType::Oblique);
            WantRebuild |= ImGui::CheckboxFlags("Monochrome",    &FontsFlags, ImGuiFreeType::Monochrome);
        }
        ImGui::End();
    }
};

FreeTypeTest freetype_test;
sg_image font_atlas = {};
uint64_t last_time = 0;


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
    sg_destroy_image(font_atlas);
    font_atlas = sg_make_image(&img_desc);
    io.Fonts->SetTexID((ImTextureID)(uintptr_t) font_atlas.id);
}

static void load_fonts() {
    ImGuiIO& io = ImGui::GetIO();
    ImFontConfig font_cfg;

    io.Fonts->AddFontFromFileTTF("./Roboto-Medium.ttf", 13.0f);
    io.Fonts->AddFontFromFileTTF("./Cousine-Regular.ttf", 13.0f);

    io.Fonts->AddFontFromFileTTF("./SourceCodePro-ExtraLight.ttf", 13.0f);
    io.Fonts->AddFontFromFileTTF("./SourceCodePro-Light.ttf", 13.0f);
    io.Fonts->AddFontFromFileTTF("./SourceCodePro-Medium.ttf", 13.0f);
    io.Fonts->AddFontFromFileTTF("./SourceCodePro-Regular.ttf", 13.0f);
    io.Fonts->AddFontFromFileTTF("./SourceCodePro-SemiBold.ttf", 13.0f);
    io.Fonts->AddFontFromFileTTF("./SourceCodePro-Bold.ttf", 13.0f);
    io.Fonts->AddFontFromFileTTF("./SourceCodePro-Black.ttf", 13.0f);
    io.Fonts->AddFontDefault();
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

    ImGui::StyleColorsDark(&ImGui::GetStyle());
    //ImGui::StyleColorsLight(&ImGui::GetStyle());
}


static void on_app_frame(void) {
    int appw = sapp_width();
    int apph = sapp_height();
    double dt = stm_sec(stm_laptime(&last_time));

    float dpi = sapp_dpi_scale();

    int w = appw / dpi;
    int h = apph / dpi;

    int win_flags = ImGuiWindowFlags_NoResize |
                    ImGuiWindowFlags_NoTitleBar |
                    ImGuiWindowFlags_NoMove |
                    ImGuiWindowFlags_NoCollapse;

    if (freetype_test.UpdateRebuild())
    {
        update_font_texture();
    }

    simgui_new_frame(appw, apph, dt);

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    freetype_test.ShowFreetypeOptionsWindow();

    //PushTextWrapPos
    const char* text = get_text();
    ImGui::TextWrapped("%s", text);


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

sapp_desc sokol_main(int argc, char* argv[]) {
    sapp_desc desc = {};
    desc.init_cb = on_app_init;
    desc.frame_cb = on_app_frame;
    desc.cleanup_cb = on_app_cleanup;
    desc.event_cb = on_app_event;
    desc.width = 800;
    desc.height = 600;
    desc.window_title = "Timelapse";
#if defined(__APPLE__)
    desc.high_dpi = true;
#else
    desc.high_dpi = false;
#endif

    return desc;
}


const char* get_text() {
    return "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Auctor neque vitae tempus quam pellentesque. Volutpat sed cras ornare arcu dui. Vel facilisis volutpat est velit egestas dui id ornare arcu. Fames ac turpis egestas sed. Blandit volutpat maecenas volutpat blandit aliquam etiam erat velit. Morbi tristique senectus et netus et malesuada fames. Nulla aliquet enim tortor at auctor. Cras adipiscing enim eu turpis. Aliquam etiam erat velit scelerisque in dictum non. Praesent elementum facilisis leo vel fringilla est ullamcorper eget. Nunc sed augue lacus viverra vitae congue eu consequat ac. Elit sed vulputate mi sit amet mauris commodo quis imperdiet. Purus in massa tempor nec feugiat nisl pretium fusce. Vitae nunc sed velit dignissim sodales. Netus et malesuada fames ac turpis egestas sed tempus urna. Sit amet justo donec enim diam vulputate ut.\
\n\n\
Massa sapien faucibus et molestie ac feugiat. Lacus laoreet non curabitur gravida. Imperdiet dui accumsan sit amet nulla. Nulla facilisi cras fermentum odio eu feugiat pretium nibh ipsum. Scelerisque felis imperdiet proin fermentum leo vel orci. Amet risus nullam eget felis eget nunc lobortis. Neque laoreet suspendisse interdum consectetur libero. Arcu dui vivamus arcu felis bibendum ut. Cursus risus at ultrices mi tempus. Semper auctor neque vitae tempus quam pellentesque nec nam. At elementum eu facilisis sed odio morbi quis commodo. Vehicula ipsum a arcu cursus vitae congue mauris rhoncus. Vitae nunc sed velit dignissim sodales ut eu sem. Aliquet enim tortor at auctor. Urna id volutpat lacus laoreet non. Cras sed felis eget velit aliquet sagittis id.\
\n\n\
Ut morbi tincidunt augue interdum velit. Maecenas pharetra convallis posuere morbi leo urna molestie. Sagittis aliquam malesuada bibendum arcu. Libero id faucibus nisl tincidunt eget nullam non. Eu turpis egestas pretium aenean pharetra magna ac placerat vestibulum. Euismod nisi porta lorem mollis aliquam. Suspendisse potenti nullam ac tortor vitae. Cum sociis natoque penatibus et magnis dis parturient. Quisque egestas diam in arcu. Egestas congue quisque egestas diam in. At urna condimentum mattis pellentesque id nibh tortor id. Vestibulum morbi blandit cursus risus at ultrices mi tempus. Lorem donec massa sapien faucibus. Volutpat blandit aliquam etiam erat velit. Egestas dui id ornare arcu. Mollis nunc sed id semper risus in. Enim neque volutpat ac tincidunt vitae semper quis lectus nulla. Amet consectetur adipiscing elit pellentesque habitant morbi tristique senectus et. Pulvinar mattis nunc sed blandit libero volutpat.\
\n\n\
Viverra justo nec ultrices dui. Et tortor at risus viverra adipiscing at in. Vitae nunc sed velit dignissim sodales ut eu sem integer. Arcu vitae elementum curabitur vitae. Proin sed libero enim sed faucibus. Mi proin sed libero enim sed faucibus. Risus nec feugiat in fermentum posuere urna nec tincidunt. Magna etiam tempor orci eu lobortis elementum nibh. Quis auctor elit sed vulputate mi sit amet. Cursus mattis molestie a iaculis at. Risus sed vulputate odio ut. Magna sit amet purus gravida quis blandit turpis cursus. Eu facilisis sed odio morbi. Nam at lectus urna duis convallis convallis tellus id. Mi tempus imperdiet nulla malesuada pellentesque elit eget. Congue mauris rhoncus aenean vel elit.\
\n\n\
In nisl nisi scelerisque eu ultrices vitae. Neque sodales ut etiam sit. Nec feugiat nisl pretium fusce id velit ut tortor. Luctus venenatis lectus magna fringilla. Pellentesque elit eget gravida cum sociis natoque penatibus et magnis. Elit duis tristique sollicitudin nibh sit amet commodo nulla facilisi. Turpis massa tincidunt dui ut ornare lectus sit amet est. Pellentesque sit amet porttitor eget dolor morbi non. Ultrices eros in cursus turpis. Eu feugiat pretium nibh ipsum. Cursus metus aliquam eleifend mi in nulla posuere sollicitudin. Egestas fringilla phasellus faucibus scelerisque eleifend. Turpis tincidunt id aliquet risus feugiat in ante.";
}
