#pragma once

#include "ms_types.hpp"
#include "ms_project.hpp"
#include "ms_timeline_player.hpp"
#include "ms_audio_playback.hpp"
#include "ms_audio_fx.hpp"
#include "ms_image.hpp"
#include "ms_lut.hpp"
#include "ms_node_graph.hpp"
#include "ms_gpu.hpp"
#include "ms_performance.hpp"
#include "ms_autosave.hpp"
#include <string>
#include <functional>

namespace ms {

class IRenderBackend;
class IMediaDecoder;
class EffectPipeline;

class Engine {
public:
    static Engine& instance();

    // Lifecycle
    bool init(int width, int height);
    bool new_project(const std::string& name, int width, int height, float fps);
    void shutdown();
    bool is_initialized() const;

    // Project
    bool load_project_json(const std::string& json);
    std::string project_to_json();

    // Playback
    void seek(TimeSec t);
    void play();
    void pause();
    void toggle_play();
    PreviewState state() const;

    // Rendering
    // Called every frame from the GL thread / Flutter texture callback
    bool render_frame();
    bool copy_last_frame(void* rgba, size_t bytes) const;
    size_t last_frame_bytes() const;

    ProjectSession& project() { return *project_; }
    const ProjectSession& project() const { return *project_; }

    // Size change (orientation / window)
    void resize(int width, int height);

    // Timeline audio (mix + master insert chain) pulled by the platform
    // audio output. Shares the same playhead as video playback.
    Error read_audio(float* interleaved, int frames, int channels = 2);
    AudioPlaybackInfo audio_info() const;
    AudioEffectChain& audio_effects();

    // ---- Document-persisted master state ----------------------------------
    // The master grade and the master audio insert chain are engine state, but
    // the project document owns them: they survive save/load/restart. Structure
    // edits of the chain are undoable; knob values are persisted without an
    // undo entry each, so one slider drag cannot bury the timeline edits.
    /** Writes the live grade + chain into the document (no undo entry). */
    void sync_master_state();
    /** Pushes the document's master state back into the engine after a load. */
    void apply_master_state();
    int  add_audio_insert(const AudioFxParams& params);
    bool remove_audio_insert(int index);
    void clear_audio_inserts();
    bool set_audio_insert(int index, const AudioFxParams& params);
    bool set_audio_insert_enabled(int index, bool enabled);
    int  audio_insert_count() const;

    // Effect helpers
    bool add_effect_to_clip(ClipId clip, const std::string& base_id);
    bool set_effect_param(EffectId effect, const std::string& name, float value);
    Error save_project(const std::string& path);
    Error autosave_project(const std::string& path);
    PerformanceStats performance() const;

    // Undo/redo. These drop the frame cache, so a moved clip cannot leave a
    // stale preview behind.
    bool undo();
    bool redo();

    /** Adds a media clip at `start`. Undoable, and drops the frame cache. */
    ClipId add_clip(TrackId track, MediaId media, TimeSec start, TimeSec duration);

    // Timeline editing. Each call is undoable and drops the frame cache.
    bool remove_clip(TrackId track, ClipId clip);
    bool move_clip(TrackId track, ClipId clip, TimeSec start);
    bool trim_clip(TrackId track, ClipId clip, TimeSec start, TimeSec duration);
    bool ripple_delete(TrackId track, ClipId clip);
    TrackId add_track(const std::string& name, int type);
    bool remove_track(TrackId track);
    bool set_track_flags(TrackId track, bool muted, bool locked, bool visible, bool solo);
    /** Track fader (linear, 1 = unity). Heard in the timeline audio mix. */
    bool set_track_gain(TrackId track, float gain);

    // Clip effect stack (index = position in the clip's stack).
    int  add_effect(TrackId track, ClipId clip, const std::string& base_id);
    bool remove_effect(TrackId track, ClipId clip, int index);
    bool set_effect_param(TrackId track, ClipId clip, int index, const std::string& name, float value);
    bool set_effect_enabled(TrackId track, ClipId clip, int index, bool enabled);
    void invalidate_frames();

    // Master grade (applied to the preview before the look) and scopes.
    void set_grade(float exposure, float contrast, float saturation, float temperature,
                   float lift, float gamma, float gain);
    void clear_grade();
    bool has_grade() const { return grade_active_; }
    /** Fills 7 floats (exposure, contrast, saturation, temperature, lift, gamma,
     *  gain) and reports whether a master grade is active. */
    bool get_grade(float* out7) const;
    /** Fills 4*bins ints: R, G, B, luma. bins <= 256. */
    bool scopes_histogram(int* out, int bins) const;

    // Timeline markers
    uint64_t add_marker(TimeSec time, const std::string& name = "");
    bool remove_marker(uint64_t id);

    /** Resizes the project and sets its frame rate without touching edits. */
    bool set_project_settings(int width, int height, float fps);

    // Shape mask on a clip. shape: 0 rect, 1 ellipse, 2 full. x/y/w/h normalized.
    bool set_clip_mask(TrackId track, ClipId clip, int shape, float x, float y, float w,
                       float h, float feather, bool inverted);
    bool clear_clip_mask(TrackId track, ClipId clip);
    /** Fills 6 floats (shape, x, y, w, h, feather) and the invert flag. */
    bool clip_mask(TrackId track, ClipId clip, float* out6, bool* inverted) const;

    /** Per-clip grade (exposure, contrast, saturation, temperature, lift, gamma, gain). */
    bool set_clip_grade(TrackId track, ClipId clip, float exposure, float contrast,
                        float saturation, float temperature, float lift, float gamma,
                        float gain);
    bool clear_clip_grade(TrackId track, ClipId clip);

    // ---- Comp network ------------------------------------------------------
    // When active, the viewport is cooked from the comp graph instead of the
    // timeline player. The graph's Viewer/Write node is the sink. Master grade
    // and the LUT still apply afterwards, as display finishing.
    NodeGraph& comp_graph() { return comp_; }
    const NodeGraph& comp_graph() const { return comp_; }
    bool comp_active() const { return comp_active_; }
    void set_comp_active(bool on);
    /** True when the comp is active and has a sink to cook. */
    bool comp_rendered() const;
    /**
     * Resolves a Read node to an image: an empty media_path reads the timeline
     * sequence at `time_sec` (the comp sees the edit), a path decodes that media
     * at the node's `offset` parameter, scaled to the comp resolution.
     */
    Error read_comp_source(const GraphNode& node, double time_sec, int w, int h,
                           ImageBuffer& out);
    /** Reads the selected clip's grade; false when it has none. */
    bool clip_grade(TrackId track, ClipId clip, float* out7) const;

    /** Peak envelope for a clip's audio, `bins` floats in 0..1. */
    bool clip_peaks(TrackId track, ClipId clip, int bins, float* out);

    // Transitions between two adjacent clips (centred on their cut).
    uint64_t add_transition(TrackId track, ClipId outgoing, ClipId incoming, int type, TimeSec duration);
    bool remove_transition(TrackId track, uint64_t id);

    // Keyframes. prop: 0 opacity, 1 scale, 2 pos_x, 3 pos_y. time is clip-local.
    bool add_keyframe(TrackId track, ClipId clip, int prop, TimeSec time, double value);
    bool clear_keyframes(TrackId track, ClipId clip, int prop);

    // ---- Keyframe curves (prop: 0 opacity, 1 scale, 2 pos_x, 3 pos_y) ----
    // Index is the key's position in time order. Values are clip-local seconds.
    int keyframe_count(TrackId track, ClipId clip, int prop);
    bool keyframe_at(TrackId track, ClipId clip, int prop, int index, double* time,
                     double* value, int* interp);
    bool set_keyframe(TrackId track, ClipId clip, int prop, int index, double time,
                      double value, int interp);
    bool set_keyframe_interp(TrackId track, ClipId clip, int prop, int index, int interp);
    /** Fills `out` with `bins` samples of the curve between t0 and t1 (clip-local). */
    bool sample_keyframe(TrackId track, ClipId clip, int prop, double t0, double t1,
                         float* out, int bins);

    // Text / titles. A text clip renders through stb_truetype instead of a decoder.
    ClipId add_text_clip(TrackId track, TimeSec start, TimeSec duration,
                         const std::string& text, const TextStyle& style);
    bool set_clip_text(TrackId track, ClipId clip, const std::string& text,
                       const TextStyle& style);
    /** Rasterises the title into `out` at the project size (used by export/preview). */
    bool render_title_preview(const std::string& text, const TextStyle& style, ImageBuffer& out);

    // Display look (3D LUT) applied to the rendered preview frame.
    bool load_look(const std::string& path);
    void clear_look();
    void set_look_strength(float strength);
    float look_strength() const { return look_strength_; }
    bool has_look() const { return look_ != nullptr; }

    // Callbacks (set from Flutter / platform)
    using FrameCallback = std::function<void(const PreviewState&)>;
    void set_frame_callback(FrameCallback cb);

private:
    Engine();
    ~Engine();
    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;

    struct Impl;
    std::unique_ptr<Impl> impl_;
    std::unique_ptr<ProjectSession> project_;
    std::unique_ptr<TimelinePlayer> player_;
    std::unique_ptr<TimelineAudioPlayback> audio_;
    std::unique_ptr<ImageBuffer> last_frame_;
    std::unique_ptr<FrameCache> frame_cache_;
    std::unique_ptr<Lut3D> look_;
    float look_strength_ = 1.f;
    NodeGraph comp_;
    bool comp_active_ = false;
    float grade_[7] = {0.f, 1.f, 1.f, 0.f, 0.f, 1.f, 1.f};
    bool grade_active_ = false;
    PerformanceMonitor perf_;
    AutosaveController autosave_;
};

} // namespace ms
