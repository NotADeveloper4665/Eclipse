// Offscreen Vulkan regression: real FSR shader compilation and resize/downscale transitions.
#include <libplacebo/vulkan.h>
#include <libplacebo/renderer.h>
#include <fstream>
#include <iostream>
#include <iterator>
#include <vector>

int main(int argc, char** argv)
{
    if (argc != 2) return 2;
    std::ifstream file(argv[1]);
    std::string shader((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    auto logParams = pl_log_default_params;
    logParams.log_cb = [](void*, enum pl_log_level level, const char* msg) { if (level <= PL_LOG_WARN) std::cerr << msg << '\n'; };
    logParams.log_level = PL_LOG_WARN;
    auto log = pl_log_create(PL_API_VER, &logParams);
    auto vkParams = pl_vulkan_default_params;
    vkParams.allow_software = true;
    auto vk = pl_vulkan_create(log, &vkParams);
    if (!vk) { std::cerr << "Vulkan unavailable\n"; return 77; }
    auto hook = pl_mpv_user_shader_parse(vk->gpu, shader.data(), shader.size());
    if (!hook) return 1;
    auto renderer = pl_renderer_create(log, vk->gpu);
    pl_frame input = {};
    input.num_planes = 3;
    input.color = pl_color_space_bt709;
    input.repr.sys = PL_COLOR_SYSTEM_BT_709;
    input.repr.levels = PL_COLOR_LEVELS_LIMITED;
    input.repr.bits.sample_depth = 8;
    input.repr.bits.color_depth = 8;
    input.crop = {0, 0, 64, 48};
    pl_tex planes[3] = {};
    for (int i = 0; i < 3; ++i) {
        const int w = i ? 32 : 64, h = i ? 24 : 48;
        std::vector<unsigned char> samples(w * h, i ? 128 : 90);
        if (i == 0) for (int y = 0; y < h; ++y) for (int x = 0; x < w; ++x) samples[y*w+x] = 16 + (x * 7 + y * 11) % 220;
        pl_tex_params texParams = {};
        texParams.w = w; texParams.h = h;
        texParams.format = pl_find_fmt(vk->gpu, PL_FMT_UNORM, 1, 8, 0, PL_FMT_CAP_SAMPLEABLE);
        texParams.sampleable = true; texParams.initial_data = samples.data();
        planes[i] = pl_tex_create(vk->gpu, &texParams);
        if (!planes[i]) return 1;
        input.planes[i].texture = planes[i];
        input.planes[i].components = 1;
        input.planes[i].component_mapping[0] = i;
    }
    const int widths[] = {96, 108, 128, 32, 128};
    for (int w : widths) {
        const int h = w * 3 / 4;
        pl_tex_params texParams = {};
        texParams.w = w; texParams.h = h;
        texParams.format = pl_find_fmt(vk->gpu, PL_FMT_UNORM, 4, 8, 0, PL_FMT_CAP_RENDERABLE);
        texParams.renderable = true; texParams.host_readable = true;
        auto outputTex = pl_tex_create(vk->gpu, &texParams);
        if (!outputTex) return 1;
        pl_frame output = {};
        output.num_planes = 1;
        output.planes[0].texture = outputTex;
        output.planes[0].components = 4;
        for (int i = 0; i < 4; ++i) output.planes[0].component_mapping[i] = i;
        output.crop = {0, 0, float(w), float(h)};
        output.color = pl_color_space_bt709;
        output.repr.sys = PL_COLOR_SYSTEM_RGB;
        output.repr.levels = PL_COLOR_LEVELS_FULL;
        auto params = pl_render_fast_params;
        params.hooks = &hook; params.num_hooks = 1;
        if (!pl_render_image(renderer, &input, &output, &params)) return 1;
        const auto errors = pl_renderer_get_errors(renderer);
        if (errors.errors || errors.num_disabled_hooks) { std::cerr << "Renderer disabled a shader: " << errors.errors << '\n'; return 1; }
        std::vector<unsigned char> pixels(w * h * 4);
        pl_tex_transfer_params transfer = {};
        transfer.tex = outputTex; transfer.ptr = pixels.data();
        if (!pl_tex_download(vk->gpu, &transfer)) return 1;
        unsigned sum = 0;
        for (auto value : pixels) sum += value;
        if (!sum) return 1;
        // A successfully parsed but never executed hook must not pass as FSR.
        params.hooks = nullptr; params.num_hooks = 0;
        if (!pl_render_image(renderer, &input, &output, &params)) return 1;
        std::vector<unsigned char> baseline(pixels.size());
        transfer.ptr = baseline.data();
        if (!pl_tex_download(vk->gpu, &transfer)) return 1;
        if (w > 64 && pixels == baseline) { std::cerr << "FSR produced ordinary scaler output\n"; return 1; }
        std::cout << "FSR render passed: " << w << 'x' << h << '\n';
        pl_tex_destroy(vk->gpu, &outputTex);
    }
    for (auto& tex : planes) pl_tex_destroy(vk->gpu, &tex);
    pl_mpv_user_shader_destroy(&hook);
    pl_renderer_destroy(&renderer);
    pl_vulkan_destroy(&vk);
    pl_log_destroy(&log);
}
