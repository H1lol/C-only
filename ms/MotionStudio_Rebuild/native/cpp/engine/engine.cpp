#include "../include/ms_engine.hpp"
#include "../include/ms_edit_ops.hpp"
#include "../include/ms_finishing.hpp"
#include "../include/ms_scopes.hpp"
#include "../include/ms_types.hpp"
#include "../include/ms_gpu.hpp"
#include "../include/ms_image.hpp"
#include <atomic>
#include <mutex>
#include <chrono>
#include <iostream>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <algorithm>

namespace ms {

struct Engine::Impl {
    std::mutex mutex;
    bool initialized = false;
    int width = 1080;
    int height = 1920;
    float fps = 30.f;
    std::atomic<TimeSec> current_time{0.0};
    std::atomic<bool> playing{false};
    std::atomic<int64_t> frame_index{0};
    // Bumped by every operation that can change rendered pixels. The frame cache
    // key includes it, so no edit can be served a stale frame.
    std::atomic<uint64_t> revision{0};
    std::chrono::steady_clock::time_point last_tick;
    FrameCallback frame_cb;
};

Engine& Engine::instance() { static Engine e; return e; }
Engine::Engine() : impl_(std::make_unique<Impl>()), project_(std::make_unique<ProjectSession>()), last_frame_(std::make_unique<ImageBuffer>()), frame_cache_(std::make_unique<FrameCache>(48)), autosave_(project_.get()) {
    player_ = std::make_unique<TimelinePlayer>(project_.get());
    audio_ = std::make_unique<TimelineAudioPlayback>(project_.get());
}
Engine::~Engine() = default;

bool Engine::init(int width, int height) {
    return new_project("Untitled", width, height, 30.f);
}

bool Engine::new_project(const std::string& name, int width, int height, float fps) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->width = width > 0 ? width : 1080;
    impl_->height = height > 0 ? height : 1920;
    impl_->fps = fps > 1.f ? fps : 30.f;
    project_->new_project(name, impl_->width, impl_->height, impl_->fps);
    player_->set_session(project_.get());
    player_->prepare();
    audio_->set_session(project_.get());
    audio_->stop();
    audio_->prepare();
    impl_->current_time = 0.0;
    impl_->frame_index = 0;
    impl_->playing = false;
    impl_->last_tick = std::chrono::steady_clock::now();
    impl_->initialized = true;
    frame_cache_->clear();
    perf_.set_backend("native-cpu");
    return true;
}

void Engine::shutdown() {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->playing = false;
    impl_->initialized = false;
    audio_->stop();
    last_frame_->clear(0,0,0,0);
}

bool Engine::is_initialized() const { return impl_->initialized; }

bool Engine::load_project_json(const std::string& json) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    auto e = project_->load_from_json(json);
    if (!e.ok()) return false;
    impl_->width = project_->meta().width;
    impl_->height = project_->meta().height;
    impl_->fps = project_->meta().fps;
    impl_->current_time = 0.0;
    impl_->frame_index = 0;
    impl_->playing = false;
    player_->set_session(project_.get());
    audio_->set_session(project_.get());
    audio_->stop();
    audio_->prepare();
    auto prep = player_->prepare();
    // Opening a saved project never goes through new_project(), so this has to
    // bring the engine up itself: render_frame() bails immediately while
    // `initialized` is false, which left the preview black after a load.
    impl_->last_tick = std::chrono::steady_clock::now();
    impl_->initialized = true;
    frame_cache_->clear();
    perf_.set_backend("native-cpu");
    return prep.ok();
}

std::string Engine::project_to_json() const { return project_->to_json(); }

void Engine::seek(TimeSec t) {
    t = std::max<TimeSec>(0.0, t);
    impl_->current_time = t;
    impl_->frame_index = static_cast<int64_t>(t * project_->meta().fps);
    project_->seek(t);
    // Audio follows the video playhead exactly; the platform output stage pulls
    // PCM for the same instant via read_audio().
    audio_->seek(t);
}

void Engine::play() {
    impl_->last_tick = std::chrono::steady_clock::now();
    impl_->playing = true;
    project_->play();
    audio_->seek(impl_->current_time.load());
    audio_->play();
}

void Engine::pause() {
    impl_->playing = false;
    project_->pause();
    audio_->pause();
}
void Engine::toggle_play() { if (impl_->playing.load()) pause(); else play(); }

PreviewState Engine::state() const {
    PreviewState s;
    s.time = impl_->current_time.load();
    s.width = project_->meta().width;
    s.height = project_->meta().height;
    s.playing = impl_->playing.load();
    s.fps = project_->meta().fps;
    s.frame_index = impl_->frame_index.load();
    return s;
}

bool Engine::render_frame() {
    if (!impl_->initialized) return false;
    perf_.begin_frame();
    if (impl_->playing.load()) {
        auto now = std::chrono::steady_clock::now();
        double dt = std::chrono::duration<double>(now - impl_->last_tick).count();
        impl_->last_tick = now;
        double t = impl_->current_time.load() + dt;
        const double end = project_->sequence_end();
        if (end > 0.0 && t >= end) t = 0.0;
        seek(t);
    }
    const int64_t fi = impl_->frame_index.load();
    // The document fingerprint is the edit revision, not a clip count: two edits
    // can leave the clip count unchanged (a trim, a move, an undo of either) and
    // would otherwise reuse a stale frame.
    const uint64_t revision = impl_->revision.load();
    CacheKey key{fi, revision ^ (static_cast<uint64_t>(project_->clip_count()) << 48) ^
                         (static_cast<uint64_t>(project_->tracks().size()) << 56)};
    bool hit = frame_cache_->get(key, *last_frame_);
    if (!hit) {
        Error e = player_->render_frame(impl_->current_time.load(), *last_frame_);
        if (!e.ok()) return false;
        frame_cache_->put(key, *last_frame_);
    }
    // The cache holds ungraded frames; grade and look are applied on top so
    // changing either never needs a cache flush.
    if (grade_active_) {
        GradeSettings g;
        g.exposure = grade_[0];
        g.contrast = grade_[1];
        g.saturation = grade_[2];
        g.temperature = grade_[3];
        g.lift = grade_[4];
        g.gamma = grade_[5];
        g.gain = grade_[6];
        apply_finishing_grade(*last_frame_, g);
    }
    if (look_ && look_strength_ > 0.f) {
        apply_lut(*last_frame_, *look_, look_strength_);
    }
    perf_.end_frame(hit); perf_.set_cache_bytes(0);
    (void)autosave_.tick();
    if (impl_->frame_cb) impl_->frame_cb(state());
    return true;
}

bool Engine::copy_last_frame(void* rgba, size_t bytes) const {
    if (!rgba || !last_frame_->valid()) return false;
    const size_t need = static_cast<size_t>(last_frame_->stride()) * static_cast<size_t>(last_frame_->height());
    if (bytes < need) return false;
    std::memcpy(rgba, last_frame_->data(), need);
    return true;
}

size_t Engine::last_frame_bytes() const {
    if (!last_frame_->valid()) return 0;
    return static_cast<size_t>(last_frame_->stride()) * static_cast<size_t>(last_frame_->height());
}

bool Engine::load_look(const std::string& path) {
    auto lut = std::make_unique<Lut3D>();
    if (!load_cube_lut(path, *lut)) return false;
    look_ = std::move(lut);
    if (look_strength_ <= 0.f) look_strength_ = 1.f;
    return true;
}

void Engine::clear_look() { look_.reset(); }

void Engine::set_look_strength(float strength) {
    look_strength_ = std::clamp(strength, 0.f, 1.f);
}

Error Engine::save_project(const std::string& path) { return project_->save_to_file(path); }
Error Engine::autosave_project(const std::string& path) { autosave_.set_path(path); return autosave_.force(); }
PerformanceStats Engine::performance() const { return perf_.stats(); }

void Engine::resize(int width, int height) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (width > 0) impl_->width = width;
    if (height > 0) impl_->height = height;
    project_->meta().width = impl_->width;
    project_->meta().height = impl_->height;
}

bool Engine::add_effect_to_clip(ClipId clip, const std::string& base_id) {
    // Locate the clip and attach a CPU effect to its motion stack.
    for (auto& track : const_cast<std::vector<TrackInfo>&>(project_->tracks())) {
        for (auto& c : track.clips) if (c.id == clip) {
            c.motion.ensure_defaults();
            c.motion.effects.push_back({base_id, {}, true});
            project_->mark_dirty();
            return true;
        }
    }
    return false;
}

Error Engine::read_audio(float* interleaved, int frames, int channels) {
    if (!audio_) return Error::make(ErrorCode::NotInitialized, "audio not available");
    return audio_->read_pcm(interleaved, frames, channels);
}

AudioPlaybackInfo Engine::audio_info() const {
    return audio_ ? audio_->info() : AudioPlaybackInfo{};
}

AudioEffectChain& Engine::audio_effects() { return audio_->effects(); }

bool Engine::set_effect_param(EffectId effect, const std::string& name, float value) {
    (void)effect; (void)name; (void)value;
    // Effect IDs are intentionally lightweight in the first native API. Parameters are stored by name in EffectOp.
    return true;
}

void Engine::set_grade(float exposure, float contrast, float saturation, float temperature,
                       float lift, float gamma, float gain) {
    grade_[0] = exposure;
    grade_[1] = contrast > 0.f ? contrast : 1.f;
    grade_[2] = saturation < 0.f ? 0.f : saturation;
    grade_[3] = temperature;
    grade_[4] = lift;
    grade_[5] = gamma > 0.01f ? gamma : 1.f;
    grade_[6] = gain < 0.f ? 0.f : gain;
    grade_active_ = true;
}

void Engine::clear_grade() { grade_active_ = false; }

bool Engine::scopes_histogram(int* out, int bins) const {
    if (!out || bins < 2 || bins > 256 || !last_frame_ || !last_frame_->valid()) return false;
    ScopeHistogram h;
    if (!Scopes::histogram(*last_frame_, h).ok()) return false;
    for (int i = 0; i < bins; ++i) {
        const int src = i * 256 / bins;
        out[0 * bins + i] = h.bins_r[src];
        out[1 * bins + i] = h.bins_g[src];
        out[2 * bins + i] = h.bins_b[src];
        out[3 * bins + i] = h.bins_y[src];
    }
    return true;
}

uint64_t Engine::add_transition(TrackId track, ClipId outgoing, ClipId incoming, int type,
                                TimeSec duration) {
    if (!project_ || !outgoing || !incoming || outgoing == incoming) return 0;
    TrackInfo* t = project_->find_track(track);
    if (!t) return 0;
    project_->push_undo();
    // One transition per clip pair: replace rather than stack.
    t->transitions.erase(
        std::remove_if(t->transitions.begin(), t->transitions.end(),
                       [&](const TimelineTransition& x) {
                           return x.outgoing == outgoing && x.incoming == incoming;
                       }),
        t->transitions.end());
    uint64_t next_id = 1;
    for (const auto& x : t->transitions) next_id = std::max(next_id, x.id + 1);
    TimelineTransition x;
    x.id = next_id;
    x.outgoing = outgoing;
    x.incoming = incoming;
    x.type = type;
    x.duration = duration > 0.05 ? duration : 0.05;
    t->transitions.push_back(x);
    project_->mark_dirty();
    invalidate_frames();
    return x.id;
}

bool Engine::remove_transition(TrackId track, uint64_t id) {
    if (!project_) return false;
    TrackInfo* t = project_->find_track(track);
    if (!t) return false;
    const auto before = t->transitions.size();
    project_->push_undo();
    t->transitions.erase(
        std::remove_if(t->transitions.begin(), t->transitions.end(),
                       [id](const TimelineTransition& x) { return x.id == id; }),
        t->transitions.end());
    if (t->transitions.size() == before) return false;
    project_->mark_dirty();
    invalidate_frames();
    return true;
}

bool Engine::add_keyframe(TrackId track, ClipId clip, int prop, TimeSec time, double value) {
    if (!project_ || prop < 0 || prop > 3) return false;
    TrackInfo* t = project_->find_track(track);
    if (!t) return false;
    for (auto& c : t->clips) {
        if (c.id != clip) continue;
        project_->push_undo();
        c.motion.ensure_defaults();
        AnimCurve* curve = prop == 0   ? &c.motion.opacity
                           : prop == 1 ? &c.motion.scale
                           : prop == 2 ? &c.motion.pos_x
                                       : &c.motion.pos_y;
        // The defaults seed a single static key; the first authored key replaces it
        // so the curve actually animates instead of holding a constant.
        if (curve->keys().size() == 1) curve->clear();
        curve->add(std::max<TimeSec>(0.0, time), value);
        project_->mark_dirty();
        invalidate_frames();
        return true;
    }
    return false;
}

bool Engine::clear_keyframes(TrackId track, ClipId clip, int prop) {
    if (!project_ || prop < 0 || prop > 3) return false;
    TrackInfo* t = project_->find_track(track);
    if (!t) return false;
    for (auto& c : t->clips) {
        if (c.id != clip) continue;
        project_->push_undo();
        AnimCurve* curve = prop == 0   ? &c.motion.opacity
                           : prop == 1 ? &c.motion.scale
                           : prop == 2 ? &c.motion.pos_x
                                       : &c.motion.pos_y;
        curve->clear();
        project_->mark_dirty();
        invalidate_frames();
        return true;
    }
    return false;
}

uint64_t Engine::add_marker(TimeSec time, const std::string& name) {
    project_->push_undo();
    const uint64_t id = project_->add_marker(time, name);
    project_->mark_dirty();
    return id;
}

bool Engine::remove_marker(uint64_t id) {
    project_->push_undo();
    if (!project_->remove_marker(id)) return false;
    project_->mark_dirty();
    return true;
}

bool Engine::set_project_settings(int width, int height, float fps) {
    if (width <= 0 || height <= 0 || fps <= 0.f) return false;
    resize(width, height);
    project_->meta().fps = fps;
    impl_->fps = fps;
    project_->mark_dirty();
    invalidate_frames();
    return true;
}

bool Engine::set_clip_mask(TrackId track, ClipId clip, int shape, float x, float y, float w,
                           float h, float feather, bool inverted) {
    TrackInfo* t = project_->find_track(track);
    if (!t) return false;
    for (auto& c : t->clips) {
        if (c.id != clip) continue;
        project_->push_undo();
        c.mask_enabled = true;
        c.mask.shape = shape <= 0 ? MaskShape::Rect
                        : shape == 1 ? MaskShape::Ellipse : MaskShape::Full;
        c.mask.x = std::clamp(x, 0.f, 1.f);
        c.mask.y = std::clamp(y, 0.f, 1.f);
        c.mask.w = std::clamp(w, 0.01f, 1.f);
        c.mask.h = std::clamp(h, 0.01f, 1.f);
        c.mask.feather = std::max(0.f, feather);
        c.mask.inverted = inverted;
        project_->mark_dirty();
        invalidate_frames();
        return true;
    }
    return false;
}

bool Engine::clear_clip_mask(TrackId track, ClipId clip) {
    TrackInfo* t = project_->find_track(track);
    if (!t) return false;
    for (auto& c : t->clips) {
        if (c.id != clip) continue;
        project_->push_undo();
        c.mask_enabled = false;
        project_->mark_dirty();
        invalidate_frames();
        return true;
    }
    return false;
}

bool Engine::clip_mask(TrackId track, ClipId clip, float* out6, bool* inverted) const {
    for (const auto& t : project_->tracks()) {
        if (t.id != track) continue;
        for (const auto& c : t.clips) {
            if (c.id != clip) continue;
            if (!c.mask_enabled) return false;
            if (out6) {
                out6[0] = static_cast<float>(static_cast<int>(c.mask.shape));
                out6[1] = c.mask.x;
                out6[2] = c.mask.y;
                out6[3] = c.mask.w;
                out6[4] = c.mask.h;
                out6[5] = c.mask.feather;
            }
            if (inverted) *inverted = c.mask.inverted;
            return true;
        }
    }
    return false;
}

bool Engine::set_clip_grade(TrackId track, ClipId clip, float exposure, float contrast,
                            float saturation, float temperature, float lift, float gamma,
                            float gain) {
    TrackInfo* t = project_->find_track(track);
    if (!t) return false;
    for (auto& c : t->clips) {
        if (c.id != clip) continue;
        project_->push_undo();
        c.grade_enabled = true;
        c.grade[0] = exposure;
        c.grade[1] = contrast > 0.f ? contrast : 1.f;
        c.grade[2] = saturation < 0.f ? 0.f : saturation;
        c.grade[3] = temperature;
        c.grade[4] = lift;
        c.grade[5] = gamma > 0.01f ? gamma : 1.f;
        c.grade[6] = gain < 0.f ? 0.f : gain;
        project_->mark_dirty();
        invalidate_frames();
        return true;
    }
    return false;
}

bool Engine::clear_clip_grade(TrackId track, ClipId clip) {
    TrackInfo* t = project_->find_track(track);
    if (!t) return false;
    for (auto& c : t->clips) {
        if (c.id != clip) continue;
        project_->push_undo();
        c.grade_enabled = false;
        project_->mark_dirty();
        invalidate_frames();
        return true;
    }
    return false;
}

bool Engine::clip_grade(TrackId track, ClipId clip, float* out7) const {
    if (!out7) return false;
    const TrackInfo* t = nullptr;
    for (const auto& x : project_->tracks()) if (x.id == track) t = &x;
    if (!t) return false;
    for (const auto& c : t->clips) {
        if (c.id != clip) continue;
        if (!c.grade_enabled) return false;
        std::memcpy(out7, c.grade, sizeof(float) * 7);
        return true;
    }
    return false;
}

bool Engine::clip_peaks(TrackId track, ClipId clip, int bins, float* out) {
    if (!out || bins <= 0) return false;
    TrackInfo* t = project_->find_track(track);
    if (!t) return false;
    const ClipInfo* c = nullptr;
    for (const auto& x : t->clips) if (x.id == clip) c = &x;
    if (!c || c->is_text) return false;
    std::vector<float> peaks;
    if (!audio_->clip_peaks(*c, bins, peaks).ok()) return false;
    if (peaks.size() != static_cast<size_t>(bins)) return false;
    std::memcpy(out, peaks.data(), sizeof(float) * static_cast<size_t>(bins));
    return true;
}

ClipId Engine::add_text_clip(TrackId track, TimeSec start, TimeSec duration,
                             const std::string& text, const TextStyle& style) {
    if (text.empty()) return 0;
    TrackInfo* t = project_->find_track(track);
    if (!t) return 0;
    project_->push_undo();
    ClipInfo c;
    c.name = text.size() > 32 ? text.substr(0, 32) : text;
    c.start = start;
    c.duration = duration > 0.05 ? duration : 3.0;
    c.source_out = c.duration;
    c.is_text = true;
    c.text = text;
    c.text_style = style;
    c.motion.ensure_defaults();
    const ClipId id = project_->add_clip(track, c);
    invalidate_frames();
    return id;
}

bool Engine::set_clip_text(TrackId track, ClipId clip, const std::string& text,
                           const TextStyle& style) {
    TrackInfo* t = project_->find_track(track);
    if (!t) return false;
    for (auto& c : t->clips) {
        if (c.id != clip) continue;
        project_->push_undo();
        c.is_text = true;
        c.text = text;
        c.text_style = style;
        if (c.name.empty() || c.name == "Text") c.name = text.size() > 32 ? text.substr(0, 32) : text;
        project_->mark_dirty();
        invalidate_frames();
        return true;
    }
    return false;
}

bool Engine::render_title_preview(const std::string& text, const TextStyle& style,
                                  ImageBuffer& out) {
    const int w = project_->meta().width;
    const int h = project_->meta().height;
    return render_text(out, w, h, style, text).ok();
}

bool Engine::undo() {
    if (!project_) return false;
    const bool ok = project_->undo();
    if (ok) invalidate_frames();
    return ok;
}

bool Engine::redo() {
    if (!project_) return false;
    const bool ok = project_->redo();
    if (ok) invalidate_frames();
    return ok;
}

ClipId Engine::add_clip(TrackId track, MediaId media, TimeSec start, TimeSec duration) {
    if (!project_) return 0;
    if (!project_->find_track(track)) return 0;
    project_->push_undo();
    ClipInfo c;
    c.media_id = media;
    c.start = start;
    c.duration = duration > 0.01 ? duration : 1.0;
    c.source_in = 0;
    c.source_out = c.duration;
    c.name = "Clip";
    c.motion.ensure_defaults();
    const ClipId id = project_->add_clip(track, c);
    if (!id) return 0;
    project_->mark_dirty();
    invalidate_frames();
    return id;
}

void Engine::invalidate_frames() {
    if (frame_cache_) frame_cache_->clear();
    impl_->revision++;
}

bool Engine::remove_clip(TrackId track, ClipId clip) {
    if (!project_) return false;
    project_->push_undo();
    const bool ok = project_->remove_clip(track, clip);
    if (ok) invalidate_frames();
    return ok;
}

bool Engine::move_clip(TrackId track, ClipId clip, TimeSec start) {
    if (!project_) return false;
    project_->push_undo();
    const bool ok = project_->move_clip(track, clip, start < 0.0 ? 0.0 : start);
    if (ok) invalidate_frames();
    return ok;
}

bool Engine::trim_clip(TrackId track, ClipId clip, TimeSec start, TimeSec duration) {
    if (!project_) return false;
    project_->push_undo();
    const bool ok = project_->trim_clip(track, clip, start < 0.0 ? 0.0 : start,
                                        duration < 0.05 ? 0.05 : duration);
    if (ok) invalidate_frames();
    return ok;
}

bool Engine::ripple_delete(TrackId track, ClipId clip) {
    if (!project_) return false;
    project_->push_undo();
    const Error e = ripple_delete_clip(*project_, track, clip);
    if (e.ok()) invalidate_frames();
    return e.ok();
}

TrackId Engine::add_track(const std::string& name, int type) {
    if (!project_) return 0;
    project_->push_undo();
    const TrackId id = project_->add_track(name.empty() ? "Track" : name, type);
    if (id) invalidate_frames();
    return id;
}

bool Engine::remove_track(TrackId track) {
    if (!project_) return false;
    project_->push_undo();
    const bool ok = project_->remove_track(track);
    if (ok) invalidate_frames();
    return ok;
}

bool Engine::set_track_flags(TrackId track, bool muted, bool locked, bool visible, bool solo) {
    if (!project_) return false;
    TrackInfo* t = project_->find_track(track);
    if (!t) return false;
    project_->push_undo();
    t->muted = muted;
    t->locked = locked;
    t->visible = visible;
    t->solo = solo;
    project_->mark_dirty();
    invalidate_frames();
    return true;
}

int Engine::add_effect(TrackId track, ClipId clip, const std::string& base_id) {
    if (!project_ || base_id.empty()) return -1;
    TrackInfo* t = project_->find_track(track);
    if (!t) return -1;
    for (auto& c : t->clips) {
        if (c.id != clip) continue;
        project_->push_undo();
        c.motion.ensure_defaults();
        c.motion.effects.push_back(EffectOp{base_id, {}, true});
        project_->mark_dirty();
        invalidate_frames();
        return static_cast<int>(c.motion.effects.size()) - 1;
    }
    return -1;
}

bool Engine::remove_effect(TrackId track, ClipId clip, int index) {
    if (!project_) return false;
    TrackInfo* t = project_->find_track(track);
    if (!t) return false;
    for (auto& c : t->clips) {
        if (c.id != clip) continue;
        if (index < 0 || index >= static_cast<int>(c.motion.effects.size())) return false;
        project_->push_undo();
        c.motion.effects.erase(c.motion.effects.begin() + index);
        project_->mark_dirty();
        invalidate_frames();
        return true;
    }
    return false;
}

bool Engine::set_effect_param(TrackId track, ClipId clip, int index, const std::string& name, float value) {
    if (!project_ || name.empty()) return false;
    TrackInfo* t = project_->find_track(track);
    if (!t) return false;
    for (auto& c : t->clips) {
        if (c.id != clip) continue;
        if (index < 0 || index >= static_cast<int>(c.motion.effects.size())) return false;
        c.motion.effects[index].params[name] = static_cast<double>(value);
        project_->mark_dirty();
        invalidate_frames();
        return true;
    }
    return false;
}

bool Engine::set_effect_enabled(TrackId track, ClipId clip, int index, bool enabled) {
    if (!project_) return false;
    TrackInfo* t = project_->find_track(track);
    if (!t) return false;
    for (auto& c : t->clips) {
        if (c.id != clip) continue;
        if (index < 0 || index >= static_cast<int>(c.motion.effects.size())) return false;
        c.motion.effects[index].enabled = enabled;
        project_->mark_dirty();
        invalidate_frames();
        return true;
    }
    return false;
}

void Engine::set_frame_callback(FrameCallback cb) { impl_->frame_cb = std::move(cb); }

} // namespace ms
