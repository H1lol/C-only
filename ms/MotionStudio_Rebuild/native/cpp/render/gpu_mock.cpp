#include "../include/ms_gpu.hpp"
#include <iostream>
#include <algorithm>

namespace ms {

// ============================================================
// Mock GPU Context (for testing without real GL)
// ============================================================

class MockGpuContext : public IGpuContext {
public:
    bool init(int width, int height) override {
        width_ = width;
        height_ = height;
        valid_ = true;
        next_tex_id_ = 1;
        next_fbo_id_ = 1;
        std::cout << "[GPU Mock] Init " << width << "x" << height << std::endl;
        return true;
    }

    void shutdown() override {
        valid_ = false;
        std::cout << "[GPU Mock] Shutdown" << std::endl;
    }

    void resize(int width, int height) override {
        width_ = width;
        height_ = height;
    }

    GpuTexture create_texture(int w, int h, PixelFormat fmt) override {
        GpuTexture t;
        t.id = next_tex_id_++;
        t.width = w;
        t.height = h;
        t.format = fmt;
        return t;
    }

    GpuTexture create_texture_from_buffer(const ImageBuffer& buf) override {
        return create_texture(buf.width(), buf.height(), buf.format());
    }

    void update_texture(GpuTexture&, const ImageBuffer&) override {}
    void destroy_texture(GpuTexture& tex) override { tex.id = 0; }

    uint32_t create_fbo(int, int) override { return next_fbo_id_++; }
    void destroy_fbo(uint32_t) override {}
    void bind_fbo(uint32_t) override {}
    void bind_default_framebuffer() override {}

    void clear(float, float, float, float) override {}
    void draw_fullscreen_quad(uint32_t) override {}

    bool read_pixels(int, int, int w, int h, ImageBuffer& out) override {
        return out.allocate(w, h);
    }

    const char* backend_name() const override { return "Mock"; }
    bool is_valid() const override { return valid_; }

private:
    int width_ = 0, height_ = 0;
    bool valid_ = false;
    uint32_t next_tex_id_ = 1;
    uint32_t next_fbo_id_ = 1;
};

std::unique_ptr<IGpuContext> create_gpu_context(const std::string& backend) {
    if (backend == "mock" || backend.empty()) {
        return std::make_unique<MockGpuContext>();
    }
    // Future: "gles", "vulkan"
    std::cout << "[GPU] Unknown backend '" << backend << "', falling back to mock" << std::endl;
    return std::make_unique<MockGpuContext>();
}

// ============================================================
// Frame Cache
// ============================================================

FrameCache::FrameCache(size_t max_entries) : max_entries_(max_entries) {}

void FrameCache::set_capacity(size_t max_entries) {
    max_entries_ = max_entries;
    evict_if_needed();
}

size_t FrameCache::size() const { return map_.size(); }

void FrameCache::clear() { map_.clear(); }

bool FrameCache::get(const CacheKey& key, ImageBuffer& out) {
    auto it = map_.find(key);
    if (it == map_.end()) return false;
    it->second.last_used = ++clock_;
    return out.copy_from(it->second.cpu);
}

void FrameCache::put(const CacheKey& key, const ImageBuffer& img) {
    Entry e;
    e.key = key;
    e.cpu.copy_from(img);
    e.last_used = ++clock_;
    map_[key] = std::move(e);
    evict_if_needed();
}

bool FrameCache::get_gpu(const CacheKey& key, GpuTexture& out) {
    auto it = map_.find(key);
    if (it == map_.end() || it->second.gpu.id == 0) return false;
    it->second.last_used = ++clock_;
    out = it->second.gpu;
    return true;
}

void FrameCache::put_gpu(const CacheKey& key, const GpuTexture& tex) {
    auto it = map_.find(key);
    if (it != map_.end()) {
        it->second.gpu = tex;
        it->second.last_used = ++clock_;
    } else {
        Entry e;
        e.key = key;
        e.gpu = tex;
        e.last_used = ++clock_;
        map_[key] = std::move(e);
        evict_if_needed();
    }
}

void FrameCache::evict_if_needed() {
    while (map_.size() > max_entries_) {
        auto oldest = map_.begin();
        for (auto it = map_.begin(); it != map_.end(); ++it) {
            if (it->second.last_used < oldest->second.last_used)
                oldest = it;
        }
        map_.erase(oldest);
    }
}

// ============================================================
// Effect Evaluator (stub)
// ============================================================

struct EffectEvaluator::Impl {
    IGpuContext* gpu = nullptr;
    FrameCache cache{32};
};

EffectEvaluator::EffectEvaluator() : impl_(std::make_unique<Impl>()) {}
EffectEvaluator::~EffectEvaluator() = default;

bool EffectEvaluator::init(IGpuContext* gpu) {
    impl_->gpu = gpu;
    return gpu != nullptr && gpu->is_valid();
}

void EffectEvaluator::shutdown() {
    impl_->gpu = nullptr;
    impl_->cache.clear();
}

bool EffectEvaluator::evaluate(const GpuTexture& input,
                               const std::vector<EffectInstance>& effects,
                               TimeSec /*time*/,
                               GpuTexture& output) {
    if (!impl_->gpu) return false;

    // For now: just pass through (real version runs shader chain)
    if (effects.empty()) {
        output = input;
        return true;
    }

    // Placeholder: create intermediate texture
    output = impl_->gpu->create_texture(input.width, input.height, input.format);
    // Real code would bind FBOs and run each effect shader in sequence
    return true;
}

bool EffectEvaluator::evaluate_cpu(const ImageBuffer& input,
                                   const std::vector<EffectInstance>& /*effects*/,
                                   TimeSec /*time*/,
                                   ImageBuffer& output) {
    // CPU fallback: just copy for now
    return output.copy_from(input);
}

} // namespace ms
