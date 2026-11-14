#include "../include/ms_render.hpp"
#include <iostream>
#include <cmath>

namespace ms {

// ============================================================
// Simple Brightness / Contrast / Saturation effect
// (GLSL would live in assets; here we show the C++ side)
// ============================================================

class BrightnessContrastEffect : public IEffect {
public:
    const char* base_id() const override { return "brightness_contrast"; }
    const char* display_name() const override { return "Brightness & Contrast"; }

    bool init() override {
        // In real code: compile shader, get uniform locations
        return true;
    }

    void shutdown() override {}

    void apply(uint32_t /*input_tex*/, uint32_t /*output_fbo*/,
               int /*width*/, int /*height*/,
               const std::vector<EffectParam>& params,
               TimeSec /*time*/) override {
        float brightness = 0.f;
        float contrast   = 1.f;

        for (const auto& p : params) {
            if (p.name == "brightness") brightness = p.value;
            if (p.name == "contrast")   contrast   = p.value;
        }

        // Real code would:
        // glUseProgram(prog);
        // glUniform1f(loc_brightness, brightness);
        // glUniform1f(loc_contrast, contrast);
        // draw_fullscreen_quad();

        (void)brightness;
        (void)contrast;
    }
};

// ============================================================
// Blur (placeholder)
// ============================================================

class BlurEffect : public IEffect {
public:
    const char* base_id() const override { return "blur"; }
    const char* display_name() const override { return "Gaussian Blur"; }

    bool init() override { return true; }
    void shutdown() override {}

    void apply(uint32_t, uint32_t, int, int,
               const std::vector<EffectParam>& params, TimeSec) override {
        float radius = 0.f;
        for (const auto& p : params) {
            if (p.name == "radius") radius = p.value;
        }
        (void)radius;
        // Real: multi-pass separable Gaussian
    }
};

// ============================================================
// Chromatic Aberration
// ============================================================

class ChromaticEffect : public IEffect {
public:
    const char* base_id() const override { return "chromatic"; }
    const char* display_name() const override { return "Chromatic Aberration"; }

    bool init() override { return true; }
    void shutdown() override {}

    void apply(uint32_t, uint32_t, int, int,
               const std::vector<EffectParam>& params, TimeSec) override {
        float amount = 0.f;
        for (const auto& p : params) {
            if (p.name == "amount") amount = p.value;
        }
        (void)amount;
    }
};

// ============================================================
// Factory helper
// ============================================================

std::unique_ptr<IEffect> create_builtin_effect(const std::string& base_id) {
    if (base_id == "brightness_contrast") return std::make_unique<BrightnessContrastEffect>();
    if (base_id == "blur")                return std::make_unique<BlurEffect>();
    if (base_id == "chromatic")           return std::make_unique<ChromaticEffect>();
    return nullptr;
}

void register_all_builtin_effects(EffectPipeline& pipe) {
    pipe.register_effect(std::make_unique<BrightnessContrastEffect>());
    pipe.register_effect(std::make_unique<BlurEffect>());
    pipe.register_effect(std::make_unique<ChromaticEffect>());
    // Later: load the 171 shaders from original MotionStudio
}

} // namespace ms
