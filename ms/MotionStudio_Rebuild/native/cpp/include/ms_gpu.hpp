#pragma once

#include "ms_image.hpp"
#include "ms_types.hpp"
#include <memory>
#include <string>
#include <unordered_map>

namespace ms {

// ============================================================
// Phase 2 — GPU Preview & Effect Evaluation
// ============================================================

class IGpuContext {
public:
    virtual ~IGpuContext() = default;

    virtual bool init(int width, int height) = 0;
    virtual void shutdown() = 0;
    virtual void resize(int width, int height) = 0;

    // Texture management
    virtual GpuTexture create_texture(int w, int h, PixelFormat fmt = PixelFormat::RGBA8) = 0;
    virtual GpuTexture create_texture_from_buffer(const ImageBuffer& buf) = 0;
    virtual void       update_texture(GpuTexture& tex, const ImageBuffer& buf) = 0;
    virtual void       destroy_texture(GpuTexture& tex) = 0;

    // Framebuffer / FBO for effect ping-pong
    virtual uint32_t create_fbo(int w, int h) = 0;
    virtual void     destroy_fbo(uint32_t fbo) = 0;
    virtual void     bind_fbo(uint32_t fbo) = 0;
    virtual void     bind_default_framebuffer() = 0;

    // Drawing
    virtual void clear(float r, float g, float b, float a = 1.f) = 0;
    virtual void draw_fullscreen_quad(uint32_t texture_id) = 0;

    // Readback (for export / debug)
    virtual bool read_pixels(int x, int y, int w, int h, ImageBuffer& out) = 0;

    // Info
    virtual const char* backend_name() const = 0;
    virtual bool is_valid() const = 0;
};

// Factory — real implementations later (GLES, Vulkan, mock)
std::unique_ptr<IGpuContext> create_gpu_context(const std::string& backend = "mock");

// ============================================================
// Frame Cache (for smooth scrubbing & effect caching)
// ============================================================

struct CacheKey {
    int64_t frame_index = 0;
    uint64_t graph_hash = 0;   // hash of active effects + params at this time

    bool operator==(const CacheKey& o) const {
        return frame_index == o.frame_index && graph_hash == o.graph_hash;
    }
};

struct CacheKeyHash {
    size_t operator()(const CacheKey& k) const {
        return std::hash<int64_t>()(k.frame_index) ^ (std::hash<uint64_t>()(k.graph_hash) << 1);
    }
};

class FrameCache {
public:
    explicit FrameCache(size_t max_entries = 64);

    void set_capacity(size_t max_entries);
    size_t size() const;
    void clear();

    // Returns true if hit
    bool get(const CacheKey& key, ImageBuffer& out);
    void put(const CacheKey& key, const ImageBuffer& img);

    // GPU side cache (texture ids)
    bool get_gpu(const CacheKey& key, GpuTexture& out);
    void put_gpu(const CacheKey& key, const GpuTexture& tex);

private:
    struct Entry {
        CacheKey key;
        ImageBuffer cpu;
        GpuTexture gpu;
        uint64_t last_used = 0;
    };

    size_t max_entries_;
    uint64_t clock_ = 0;
    std::unordered_map<CacheKey, Entry, CacheKeyHash> map_;

    void evict_if_needed();
};

// ============================================================
// Effect Evaluator (runs effect chain on GPU or CPU fallback)
// ============================================================

class EffectEvaluator {
public:
    EffectEvaluator();
    ~EffectEvaluator();

    bool init(IGpuContext* gpu);
    void shutdown();

    // Evaluate a list of effects on an input texture / buffer
    // Returns output texture (caller does not own it — owned by evaluator/cache)
    bool evaluate(const GpuTexture& input,
                  const std::vector<EffectInstance>& effects,
                  TimeSec time,
                  GpuTexture& output);

    // CPU fallback path
    bool evaluate_cpu(const ImageBuffer& input,
                      const std::vector<EffectInstance>& effects,
                      TimeSec time,
                      ImageBuffer& output);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace ms
