
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

#include <imgui/imgui.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#define HANDMADE_MATH_IMPLEMENTATION
#include <handmade/HandmadeMath.h>

#include <magu/ini.h>

#include "misc.h"

extern void imgui_sokol_event(const sapp_event* event);
extern void imgui_setup();
extern void imgui_teardown();


uint64_t last_time = 0;

typedef struct {
    hmm_mat4 mvp;
} vs_params_t;

hmm_mat4 view_proj;
sg_pass_action pass_action;
sg_draw_state draw_state = {0};

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


static void on_app_init(void) {
    sg_desc desc = (sg_desc){
        .mtl_device = sapp_metal_get_device(),
        .mtl_renderpass_descriptor_cb = sapp_metal_get_renderpass_descriptor,
        .mtl_drawable_cb = sapp_metal_get_drawable,
        .d3d11_device = sapp_d3d11_get_device(),
        .d3d11_device_context = sapp_d3d11_get_device_context(),
        .d3d11_render_target_view_cb = sapp_d3d11_get_render_target_view,
        .d3d11_depth_stencil_view_cb = sapp_d3d11_get_depth_stencil_view
    };
    sg_setup(&desc);

    pass_action = (sg_pass_action) {
        .colors[0] = { .action=SG_ACTION_CLEAR, .val={0.34f, 0.34f, 0.34f, 1.0f} }
    };

    float vertices[] = {
         1.0f,  1.0f,  1.0f,   1.0f, 1.0f, 1.0f, 1.0f,     1.0f, 1.0f,
         1.0f, -1.0f,  1.0f,   1.0f, 1.0f, 1.0f, 1.0f,     1.0f, 0.0f,
        -1.0f, -1.0f,  1.0f,   1.0f, 1.0f, 1.0f, 1.0f,     0.0f, 0.0f,
        -1.0f,  1.0f,  1.0f,   1.0f, 1.0f, 1.0f, 1.0f,     0.0f, 1.0f,
    };
    uint16_t indices[] = { 0, 1, 2,  0, 2, 3 };

    sg_buffer_desc vertexbuf_desc = (sg_buffer_desc){
        .size = sizeof(vertices),
        .content = vertices,
    };
    sg_buffer vbuf = sg_make_buffer(&vertexbuf_desc);

    sg_buffer_desc indexbuf_desc = (sg_buffer_desc){
        .type = SG_BUFFERTYPE_INDEXBUFFER,
        .size = sizeof(indices),
        .content = indices,
    };
    sg_buffer ibuf = sg_make_buffer(&indexbuf_desc);

    sg_shader_desc shader_desc = (sg_shader_desc){
        .vs.uniform_blocks[0] = {
            .size = sizeof(vs_params_t),
            .uniforms = {
                [0] = { .name="mvp", .type=SG_UNIFORMTYPE_MAT4 }
            }
        },
        .fs.images[0] = { .name="tex", .type=SG_IMAGETYPE_2D },
        .vs.source = vs_src,
        .fs.source = fs_src
    };
    sg_shader shd = sg_make_shader(&shader_desc);

    sg_pipeline_desc pipeline_desc = (sg_pipeline_desc){
        .layout = {
            .attrs = {
                [0] = { .name="position",   .format=SG_VERTEXFORMAT_FLOAT3 },
                [1] = { .name="color0",     .format=SG_VERTEXFORMAT_FLOAT4 },
                [2] = { .name="texcoord0",  .format=SG_VERTEXFORMAT_FLOAT2 }
            },
        },
        .shader = shd,
        .index_type = SG_INDEXTYPE_UINT16,
        .depth_stencil = {
            .depth_compare_func = SG_COMPAREFUNC_LESS_EQUAL,
            .depth_write_enabled = true
        },
        .rasterizer.cull_mode = SG_CULLMODE_BACK,
        //.rasterizer.sample_count = 4
    };
    sg_pipeline pip = sg_make_pipeline(&pipeline_desc);

    sg_image_desc img_desc = (sg_image_desc){
        .width = g_Settings.width,
        .height = g_Settings.height,
        .pixel_format = SG_PIXELFORMAT_RGBA8,
        .usage = SG_USAGE_STREAM,
        .min_filter = SG_FILTER_LINEAR,
        .mag_filter = SG_FILTER_LINEAR,
        .wrap_u = SG_WRAP_CLAMP_TO_EDGE,
        .wrap_v = SG_WRAP_CLAMP_TO_EDGE
    };
    sg_image img = sg_make_image(&img_desc);

    /* setup the draw state */
    draw_state = (sg_draw_state) {
        .pipeline = pip,
        .vertex_buffers[0] = vbuf,
        .index_buffer = ibuf,
        .fs_images[0] = img
    };

    stm_setup();
    imgui_setup();
}


static void on_app_frame(void) {
    int w = sapp_width();
    int h = sapp_height();

    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(w, h);
    io.DeltaTime = (float) stm_sec(stm_laptime(&last_time));
    ImGui::NewFrame();

    ImGui::SetNextWindowPos(ImVec2((w * 2) / 3, 0));
    ImGui::SetNextWindowSize(ImVec2((w * 1) / 3, h));
    ImGui::Begin("Config", 0);

    ImGui::SetWindowSize(ImVec2(g_Settings.width, g_Settings.height));

    // if (ImGui::CollapsingHeader("Noise"))
    // {
    //     ImGui::Checkbox("Show Noise", &show_noise);

    //     ImGui::InputInt("seed", &g_NoiseParams.seed, 1, 127);
    //     if (ImGui::SmallButton("Random")) {
    //         g_NoiseParams.seed = rand() & 0xFFFF;
    //     }

    //     ImGui::Combo("Noise Type", &noise_type, "Perlin\0Simplex (not implemented)\0");

    //     if (ImGui::CollapsingHeader("fBm")) {
    //         ImGui::Text("fBm");
    //         ImGui::InputInt("octaves", &g_NoiseParams.fbm_octaves);
    //         ImGui::SliderFloat("frequency", &g_NoiseParams.fbm_frequency, 0.01f, 1.5f);
    //         ImGui::SliderFloat("lacunarity", &g_NoiseParams.fbm_lacunarity, 1.0f, 3.0f);
    //         ImGui::SliderFloat("amplitude", &g_NoiseParams.fbm_amplitude, 0.01f, 10.0f);
    //         ImGui::SliderFloat("gain", &g_NoiseParams.fbm_gain, 0.01f, 2.0f);
    //     }

    //     ImGui::Separator();

    //     ImGui::Combo("Perturb Type", &g_NoiseParams.perturb_type, "None\0Perturb 1\0Perturb 2\0");
    //     if (g_NoiseParams.perturb_type == 1)
    //     {
    //         ImGui::SliderFloat("scale", &g_NoiseParams.perturb1_scale, 0.01f, 256.0f);
    //         ImGui::SliderFloat("a1", &g_NoiseParams.perturb1_a1, 0.01f, 10.0f);
    //         ImGui::SliderFloat("a2", &g_NoiseParams.perturb1_a2, 0.01f, 10.0f);
    //     }
    //     else if (g_NoiseParams.perturb_type == 2)
    //     {
    //         ImGui::SliderFloat("scale", &g_NoiseParams.perturb2_scale, 0.01f, 256.0f);
    //         ImGui::SliderFloat("qyx", &g_NoiseParams.perturb2_qyx, 0.01f, 10.0f);
    //         ImGui::SliderFloat("qyy", &g_NoiseParams.perturb2_qyy, 0.01f, 10.0f);
    //         ImGui::SliderFloat("rxx", &g_NoiseParams.perturb2_rxx, 0.01f, 10.0f);
    //         ImGui::SliderFloat("rxy", &g_NoiseParams.perturb2_rxy, 0.01f, 10.0f);
    //         ImGui::SliderFloat("ryx", &g_NoiseParams.perturb2_ryx, 0.01f, 10.0f);
    //         ImGui::SliderFloat("ryy", &g_NoiseParams.perturb2_ryy, 0.01f, 10.0f);
    //     }

    //     ImGui::Separator();
    //     ImGui::Text("Processing");

    //     ImGui::Checkbox("Use Radial Falloff", &g_NoiseParams.apply_radial);
    //     ImGui::SliderFloat("Radial Falloff", &g_NoiseParams.radial_falloff, 0.001f, 1.0f);

    //     ImGui::Combo("Modification Type", &g_NoiseParams.noise_modify_type, "None\0Billow\0Ridge\0");

    //     ImGui::SliderFloat("Power", &g_NoiseParams.contrast_exponent, 0.01f, 10.0f);
    // }

    // if (ImGui::CollapsingHeader("Erosion")) {
    //     ImGui::Checkbox("Use Erosion", &g_NoiseParams.use_erosion);

    //     ImGui::Combo("Erosion Type", &g_NoiseParams.erode_type, "Thermal\0Hydraulic\0");
    //     ImGui::InputInt("Iterations", &g_NoiseParams.erode_iterations);

    //     if (g_NoiseParams.erode_type == 0)
    //     {
    //         ImGui::SliderFloat("Talus", &g_NoiseParams.erode_thermal_talus, 0.0f, 16.0f);
    //     }
    //     else if (g_NoiseParams.erode_type == 1)
    //     {
    //         ImGui::SliderFloat("Rain Amount", &g_NoiseParams.erode_rain_amount, 0.0f, 5.0f);
    //         ImGui::SliderFloat("Solubility", &g_NoiseParams.erode_solubility, 0.0f, 1.0f);
    //         ImGui::SliderFloat("Evaporation", &g_NoiseParams.erode_evaporation, 0.0f, 1.0f);
    //         ImGui::SliderFloat("Capacity", &g_NoiseParams.erode_capacity, 0.0f, 5.0f);
    //         ImGui::Checkbox("Show Sediment", &show_sediment);
    //         ImGui::Checkbox("Show Water", &show_water);
    //     }
    // }

    // if (ImGui::CollapsingHeader("Voronoi"))
    // {
    //     ImGui::Checkbox("Show Cells", &show_voronoi);

    //     ImGui::InputInt("seed", &g_VoronoiParams.seed, 1, 127);
    //     if (ImGui::SmallButton("Random")) {
    //         g_VoronoiParams.seed = rand() & 0xFFFF;
    //     }

    //     ImGui::SliderInt("Border", &g_VoronoiParams.border, 0, g_MapParams.width/2);

    //     ImGui::Combo("Cell Type", &g_VoronoiParams.generation_type, "Random\0Hexagonal\0");

    //     if (g_VoronoiParams.generation_type == 0) // random
    //     {
    //         ImGui::SliderInt("Num Cells", &g_VoronoiParams.num_cells, 1, 16*1024);
    //         ImGui::SliderInt("Relax", &g_VoronoiParams.num_relaxations, 0, 100);
    //     }
    //     else if(g_VoronoiParams.generation_type == 1) // hexagonal
    //     {
    //         ImGui::SliderInt("Density", &g_VoronoiParams.hexagon_density, 1, 128);
    //     }
    // }

    // if (ImGui::CollapsingHeader("Map")) {

    //     ImGui::SliderInt("Sea Level", &g_MapParams.sea_level, 0, 255);

    //     if (ImGui::CollapsingHeader("Colors")) {
    //         ImGui::Text("Elevation limits and their colors");

    //         size_t num_limits = sizeof(height_limits)/sizeof(height_limits[0]);
    //         for( int i = 0; i < num_limits; ++i)
    //         {
    //             char name[64];

    //             snprintf(name, sizeof(name), "Color %d", i);
    //             float c[3] = { g_MapParams.colors[i*3+0]/255.0f, g_MapParams.colors[i*3+1]/255.0f, g_MapParams.colors[i*3+2]/255.0f};
    //             if (ImGui::ColorEdit3(name, c, ImGuiColorEditFlags_NoInputs|ImGuiColorEditFlags_NoLabel)) {
    //                 g_MapParams.colors[i*3+0] = (uint8_t)(c[0] * 255.0f);
    //                 g_MapParams.colors[i*3+1] = (uint8_t)(c[1] * 255.0f);
    //                 g_MapParams.colors[i*3+2] = (uint8_t)(c[2] * 255.0f);
    //             }

    //             ImGui::SameLine();

    //             snprintf(name, sizeof(name), "%d", i);

    //             int limit = g_MapParams.limits[i];
    //             int prev = i == 0 ? 0 : g_MapParams.limits[i-1];
    //             int next = i == num_limits-1 ? 255 : g_MapParams.limits[i+1];
    //             if (ImGui::SliderInt(name, &limit, prev, next)) {
    //                 g_MapParams.limits[i] = (uint8_t)limit;
    //             }
    //         }
    //     }
    //     ImGui::Checkbox("Use Shading", &g_MapParams.use_shading);
    //     ImGui::SliderInt("Shadow Step Length", &g_MapParams.shadow_step_length, 1, 16);
    //     ImGui::SliderFloat("Shadow Strength", &g_MapParams.shadow_strength, 0.0f, 1.0f);
    //     ImGui::SliderFloat("Shadow Strength Sea", &g_MapParams.shadow_strength_sea, 0.0f, 1.0f);
    // }

    ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);

    ImGui::End();

    /////////////////////////////////////////
    // Check for updates
    // uint32_t prev_voronoi_param_hash = g_VoronoiParamsHash;
    // uint32_t prev_noise_param_hash = g_NoiseParamsHash;
    // uint32_t prev_map_param_hash = g_MapParamsHash;

    // g_VoronoiParamsHash = Hash((void*)&g_VoronoiParams, sizeof(g_VoronoiParams));
    // g_NoiseParamsHash = Hash((void*)&g_NoiseParams, sizeof(g_NoiseParams));
    // g_MapParamsHash = Hash((void*)&g_MapParams, sizeof(g_MapParams));
    // bool update_voronoi = prev_voronoi_param_hash != g_VoronoiParamsHash;
    // bool update_noise = prev_noise_param_hash != g_NoiseParamsHash;
    // bool update_map = update_noise || update_voronoi || prev_map_param_hash != g_MapParamsHash;

    // if (update_map)
    // {
    //     UpdateParams(&g_VoronoiParams, &g_NoiseParams, &g_MapParams);
    // }

    // if (update_voronoi)
    // {
    //     GenerateVoronoi();
    // }

    // if (update_noise)
    // {
    //     int size = g_MapParams.width*g_MapParams.height;
    //     memset(sediment, 0, size*sizeof(float));
    //     memset(water, 0, size*sizeof(float));

    //     if (g_NoiseParams.perturb_type == 0)
    //         GenerateNoise(noisef, g_NoiseParams.noise_modify_type);
    //     else if(g_NoiseParams.perturb_type == 1)
    //         Perturb1(g_MapParams.width, g_MapParams.height, noisef);
    //     else if(g_NoiseParams.perturb_type == 2)
    //         Perturb2(g_MapParams.width, g_MapParams.height, noisef);

    //     if (g_NoiseParams.perturb_type != 0)
    //     {
    //         Blur(g_MapParams.width, g_MapParams.height, noisef, 255.0f);
    //         Blur(g_MapParams.width, g_MapParams.height, noisef, 255.0f);
    //         Blur(g_MapParams.width, g_MapParams.height, noisef, 255.0f);
    //         Blur(g_MapParams.width, g_MapParams.height, noisef, 255.0f);
    //     }

    //     ContrastNoise(noisef, g_NoiseParams.contrast_exponent);
    //     if (g_NoiseParams.use_erosion)
    //         Erode(g_MapParams.width, g_MapParams.height, noisef, sediment, water);

    //     Normalize(g_MapParams.width, g_MapParams.height, noisef);

    //     if (g_NoiseParams.apply_radial)
    //         NoiseRadial(g_MapParams.width, g_MapParams.height, g_NoiseParams.radial_falloff, noisef);

    //     Normalize(g_MapParams.width, g_MapParams.height, noisef);
    // }

    // if (update_map)
    // {
    //     for( int i = 0; i < g_MapParams.width * g_MapParams.height; ++i )
    //         heights[i] = (uint8_t)255.0f * noisef[i];

    //     GenerateMap(heights);
    //     ColorizeMap(heights, colors, sizeof(height_limits)/sizeof(height_limits[0]), g_MapParams.limits, g_MapParams.colors);
    // }

    // if (show_water) {
    //     for(int i = 0; i < g_MapParams.width * g_MapParams.height; ++i) {
    //         pixels[i*4 + 0] = (uint8_t)(255.0f * water[i]);
    //         pixels[i*4 + 1] = (uint8_t)(255.0f * water[i]);
    //         pixels[i*4 + 2] = (uint8_t)(255.0f * water[i]);
    //         pixels[i*4 + 3] = 0xFF;
    //     }
    // }
    // else if (show_sediment) {
    //     for(int i = 0; i < g_MapParams.width * g_MapParams.height; ++i) {
    //         pixels[i*4 + 0] = (uint8_t)(255.0f * sediment[i]);
    //         pixels[i*4 + 1] = (uint8_t)(255.0f * sediment[i]);
    //         pixels[i*4 + 2] = (uint8_t)(255.0f * sediment[i]);
    //         pixels[i*4 + 3] = 0xFF;
    //     }
    // }
    // else if (show_noise) {
    //     for(int i = 0; i < g_MapParams.width * g_MapParams.height; ++i) {
    //         pixels[i*4 + 0] = heights[i];
    //         pixels[i*4 + 1] = heights[i];
    //         pixels[i*4 + 2] = heights[i];
    //         pixels[i*4 + 3] = 0xFF;
    //     }
    // }
    // else if (show_voronoi) {
    //     SMap* map = GetMap();
    //     draw_voronoi(pixels, map);
    // }
    // else
    // {
    //     for(int i = 0; i < g_MapParams.width * g_MapParams.height; ++i) {
    //         pixels[i*4 + 0] = colors[i*3 + 0];
    //         pixels[i*4 + 1] = colors[i*3 + 1];
    //         pixels[i*4 + 2] = colors[i*3 + 2];
    //         pixels[i*4 + 3] = 0xFF;
    //     }
    // }

    // sg_image_content img_content = (sg_image_content){
    //     .subimage[0][0] = {
    //         .ptr = pixels,
    //         .size = g_MapParams.width*g_MapParams.height*4
    //     }
    // };
    // sg_update_image(draw_state.fs_images[0], &img_content);

    hmm_mat4 proj = HMM_Orthographic(-1, 2, -1, 1, 0.01f, 5.0f);
    hmm_mat4 view = HMM_LookAt(HMM_Vec3(0.0f, 0.0f, 4.0f), HMM_Vec3(0.0f, 0.0f, 0.0f), HMM_Vec3(0.0f, 1.0f, 0.0f));
    hmm_mat4 view_proj = HMM_MultiplyMat4(proj, view);

    hmm_mat4 identity = HMM_Mat4d(1);
    vs_params_t vs_params;
    vs_params.mvp = HMM_MultiplyMat4(view_proj, identity);

    sg_begin_default_pass(&pass_action, w, h);
    sg_apply_draw_state(&draw_state);
    sg_apply_uniform_block(SG_SHADERSTAGE_VS, 0, &vs_params, sizeof(vs_params));
    sg_draw(0, 6, 1);

    ImGui::Render();
    sg_end_pass();
    sg_commit();
}

static void on_app_cleanup(void) {
    imgui_teardown();
    sg_shutdown();
}

static void log_msg(const char* msg) {
    printf("%s\n", msg);
    fflush(stdout);
}


static void on_app_event(const sapp_event* event) {
    imgui_sokol_event(event);

    if (event->type == SAPP_EVENTTYPE_KEY_DOWN) {
        if (event->key_code == SAPP_KEYCODE_ESCAPE) {
            exit(0);
        }
    }
    else if(event->type == SAPP_EVENTTYPE_RESIZED) {
        //printf("RESIZED: %d, %d\n", event->framebuffer_width, event->framebuffer_height);
    }
}

sapp_desc sokol_main(int argc, char* argv[]) {

    settings_init(&g_Settings);

    sapp_desc desc;
    desc.init_cb = on_app_init;
    desc.frame_cb = on_app_frame;
    desc.cleanup_cb = on_app_cleanup;
    desc.event_cb = on_app_event;
    desc.width = g_Settings.width;
    desc.height = g_Settings.height;
    desc.window_title = "Timelapse";
    return desc;
}


#if defined(SOKOL_METAL)
const char* vs_src =
    "#include <metal_stdlib>\n"
    "using namespace metal;\n"
    "struct params_t {\n"
    "  float4x4 mvp;\n"
    "};\n"
    "struct vs_in {\n"
    "  float4 position [[attribute(0)]];\n"
    "  float4 color [[attribute(1)]];\n"
    "  float2 uv [[attribute(2)]];\n"
    "};\n"
    "struct vs_out {\n"
    "  float4 pos [[position]];\n"
    "  float4 color;\n"
    "  float2 uv;\n"
    "};\n"
    "vertex vs_out _main(vs_in in [[stage_in]], constant params_t& params [[buffer(0)]]) {\n"
    "  vs_out out;\n"
    "  out.pos = params.mvp * in.position;\n"
    "  out.color = float4(in.color.xyz, 1.0);\n"
    "  out.uv = in.uv;\n"
    "  return out;\n"
    "}\n";
const char* fs_src =
    "#include <metal_stdlib>\n"
    "using namespace metal;\n"
    "struct fs_in {\n"
    "  float4 color;\n"
    "  float2 uv;\n"
    "};\n"
    "fragment float4 _main(fs_in in [[stage_in]], texture2d<float> tex [[texture(0)]], sampler smp [[sampler(0)]]) {\n"
    "  return float4(tex.sample(smp, in.uv).xyz, 1.0) * in.color;\n"
    "};\n";

#elif defined(SOKOL_GLES2)
const char* vs_src =
    "uniform mat4 mvp;\n"
    "attribute vec4 position;\n"
    "attribute vec4 color0;\n"
    "attribute vec2 texcoord0;\n"
    "varying vec2 uv;"
    "varying vec4 color;"
    "void main() {\n"
    "  gl_Position = mvp * position;\n"
    "  uv = texcoord0;\n"
    "  color = color0;\n"
    "}\n";
const char* fs_src =
    "precision mediump float;\n"
    "uniform sampler2D tex;\n"
    "varying vec4 color;\n"
    "varying vec2 uv;\n"
    "void main() {\n"
    "  gl_FragColor = texture2D(tex, uv) * color;\n"
    "}\n";
#elif defined(SOKOL_D3D11)
const char* vs_src =
    "cbuffer params {\n"
    "  float2 disp_size;\n"
    "};\n"
    "struct vs_in {\n"
    "  float2 pos: POSITION;\n"
    "  float2 uv: TEXCOORD0;\n"
    "  float4 color: COLOR0;\n"
    "};\n"
    "struct vs_out {\n"
    "  float2 uv: TEXCOORD0;\n"
    "  float4 color: COLOR0;\n"
    "  float4 pos: SV_Position;\n"
    "};\n"
    "vs_out main(vs_in inp) {\n"
    "  vs_out outp;\n"
    "  outp.pos = float4(((inp.pos/disp_size)-0.5)*float2(2.0,-2.0), 0.5, 1.0);\n"
    "  outp.uv = inp.uv;\n"
    "  outp.color = inp.color;\n"
    "  return outp;\n"
    "}\n";
const char* fs_src =
    "Texture2D<float4> tex: register(t0);\n"
    "sampler smp: register(s0);\n"
    "float4 main(float2 uv: TEXCOORD0, float4 color: COLOR0): SV_Target0 {\n"
    "  return tex.Sample(smp, uv) * color;\n"
    "}\n";
#else
    #error "No shader implemented yet for this platform"
#endif

