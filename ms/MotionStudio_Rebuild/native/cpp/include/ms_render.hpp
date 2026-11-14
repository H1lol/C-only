#pragma once

#include "ms_types.hpp"
#include <string>
#include <vector>
#include <memory>

namespace ms {

// ============================================================
// Abstract render backend (GLES / Vulkan / Metal later)
// ============================================================

class IRenderBackend {
public:
    virtual ~IRenderBackend() = default;

    virtual bool init(int width, int height) = 0;
    virtual void shutdown() = 0;
    virtual void resize(int width, int height) = 0;

    // Begin / end frame
    virtual void begin_frame() = 0;
    virtual void end_frame() = 0;

    // Simple textured quad (most effects end with this)
    virtual void draw_fullscreen_quad() = 0;

    // Create / destroy texture
    virtual uint32_t create_texture(int w, int h, const void* data = nullptr) = 0;
    virtual void     destroy_texture(uint32_t id) = 0;
    virtual void     bind_texture(uint32_t id, int unit = 0) = 0;

    // FBO for ping-pong effects
    virtual uint32_t create_fbo(int w, int h) = 0;
    virtual void     destroy_fbo(uint32_t id) = 0;
    virtual void     bind_fbo(uint32_t id) = 0;
    virtual void     bind_default_fbo() = 0;
};

// ============================================================
// Effect interface
// ============================================================

class IEffect {
public:
    virtual ~IEffect() = default;

    virtual const char* base_id() const = 0;
    virtual const char* display_name() const = 0;

    virtual bool init() = 0;
    virtual void shutdown() = 0;

    // Apply effect: input texture → output FBO
    virtual void apply(uint32_t input_tex, uint32_t output_fbo,
                       int width, int height,
                       const std::vector<EffectParam>& params,
                       TimeSec time) = 0;
};

// ============================================================
// Effect pipeline (chain of effects)
// ============================================================

class EffectPipeline {
public:
    EffectPipeline();
    ~EffectPipeline();

    bool init(IRenderBackend* backend);
    void shutdown();

    // Register a built-in effect
    void register_effect(std::unique_ptr<IEffect> effect);

    // Run a list of effects
    bool process(uint32_t input_tex,
                 const std::vector<EffectInstance>& effects,
                 TimeSec time,
                 int width, int height,
                 uint32_t* out_tex);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace ms
