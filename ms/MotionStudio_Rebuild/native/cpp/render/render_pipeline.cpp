#include "../include/ms_gpu.hpp"
#include "../include/ms_image.hpp"
#include "../include/ms_error.hpp"
#include "../include/ms_types.hpp"
#include <iostream>
#include <vector>

namespace ms {

// ============================================================
// Full Phase 2 Render Pipeline
// Ties ImageBuffer + GPU + Cache + EffectEvaluator together
// ============================================================

class RenderPipeline {
public:
    RenderPipeline() = default;

    Error init(int width, int height, const std::string& backend = "mock") {
        if (initialized_) {
            return Error::make(ErrorCode::AlreadyInitialized, "RenderPipeline already initialized");
        }
        if (width <= 0 || height <= 0) {
            return Error::make(ErrorCode::InvalidArgument, "Invalid size", 
                               "width=" + std::to_string(width) + " height=" + std::to_string(height));
        }

        gpu_ = create_gpu_context(backend);
        if (!gpu_) {
            return Error::make(ErrorCode::GpuInitFailed, "Failed to create GPU context", backend);
        }
        if (!gpu_->init(width, height)) {
            return Error::make(ErrorCode::GpuInitFailed, "GPU context init failed", gpu_->backend_name());
        }

        evaluator_ = std::make_unique<EffectEvaluator>();
        if (!evaluator_->init(gpu_.get())) {
            return Error::make(ErrorCode::GpuInitFailed, "EffectEvaluator init failed");
        }

        cache_ = std::make_unique<FrameCache>(64);
        width_ = width;
        height_ = height;
        initialized_ = true;

        std::cout << "[RenderPipeline] Ready — " << gpu_->backend_name()
                  << " " << width_ << "x" << height_ << std::endl;
        return Error::success();
    }

    void shutdown() {
        if (evaluator_) evaluator_->shutdown();
        if (gpu_) gpu_->shutdown();
        evaluator_.reset();
        gpu_.reset();
        cache_.reset();
        initialized_ = false;
    }

    Error resize(int width, int height) {
        if (!initialized_) return Error::make(ErrorCode::NotInitialized, "Pipeline not initialized");
        if (width <= 0 || height <= 0) {
            return Error::make(ErrorCode::InvalidArgument, "Invalid resize");
        }
        gpu_->resize(width, height);
        width_ = width;
        height_ = height;
        cache_->clear(); // resolution changed
        return Error::success();
    }

    // Main entry: render one frame with optional effects
    Error render_frame(const GpuTexture& input,
                       const std::vector<EffectInstance>& effects,
                       TimeSec time,
                       int64_t frame_index,
                       GpuTexture& output) {
        if (!initialized_) {
            return Error::make(ErrorCode::NotInitialized, "RenderPipeline not initialized");
        }
        if (!gpu_->is_valid()) {
            return Error::make(ErrorCode::GpuContextLost, "GPU context lost");
        }

        // Build cache key
        CacheKey key;
        key.frame_index = frame_index;
        key.graph_hash = hash_effects(effects);

        // Try cache first
        if (cache_->get_gpu(key, output)) {
            return Error::success();
        }

        // Evaluate effects
        if (!evaluator_->evaluate(input, effects, time, output)) {
            return Error::make(ErrorCode::EffectLoadFailed, "Effect evaluation failed");
        }

        // Store in cache
        cache_->put_gpu(key, output);
        return Error::success();
    }

    // CPU path (fallback / export)
    Error render_frame_cpu(const ImageBuffer& input,
                           const std::vector<EffectInstance>& effects,
                           TimeSec time,
                           ImageBuffer& output) {
        if (!initialized_) {
            return Error::make(ErrorCode::NotInitialized, "RenderPipeline not initialized");
        }
        if (!input.valid()) {
            return Error::make(ErrorCode::InvalidArgument, "Invalid input buffer");
        }

        if (!evaluator_->evaluate_cpu(input, effects, time, output)) {
            return Error::make(ErrorCode::EffectLoadFailed, "CPU effect evaluation failed");
        }
        return Error::success();
    }

    Error create_preview_texture(int w, int h, GpuTexture& out) {
        if (!initialized_) return Error::make(ErrorCode::NotInitialized, "Not initialized");
        out = gpu_->create_texture(w, h);
        if (out.id == 0) {
            return Error::make(ErrorCode::TextureCreateFailed, "Failed to create preview texture");
        }
        return Error::success();
    }

    IGpuContext* gpu() { return gpu_.get(); }
    FrameCache* cache() { return cache_.get(); }
    bool is_initialized() const { return initialized_; }

private:
    uint64_t hash_effects(const std::vector<EffectInstance>& effects) const {
        uint64_t h = 14695981039346656037ull; // FNV offset
        for (const auto& e : effects) {
            for (char c : e.base_id) {
                h ^= static_cast<uint64_t>(c);
                h *= 1099511628211ull;
            }
            h ^= e.enabled ? 1 : 0;
            for (const auto& p : e.params) {
                h ^= static_cast<uint64_t>(p.value * 1000.f);
                h *= 1099511628211ull;
            }
        }
        return h;
    }

    bool initialized_ = false;
    int width_ = 0, height_ = 0;
    std::unique_ptr<IGpuContext> gpu_;
    std::unique_ptr<EffectEvaluator> evaluator_;
    std::unique_ptr<FrameCache> cache_;
};

// Global access for engine
static std::unique_ptr<RenderPipeline> g_pipeline;

Error init_render_pipeline(int w, int h) {
    g_pipeline = std::make_unique<RenderPipeline>();
    return g_pipeline->init(w, h);
}

void shutdown_render_pipeline() {
    if (g_pipeline) {
        g_pipeline->shutdown();
        g_pipeline.reset();
    }
}

RenderPipeline* get_render_pipeline() {
    return g_pipeline.get();
}

} // namespace ms
