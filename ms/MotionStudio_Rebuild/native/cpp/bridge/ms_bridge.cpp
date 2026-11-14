#include "ms_bridge.h"
#include "../include/ms_engine.hpp"
#include <cstdlib>
#include <cstring>

extern "C" {
int ms_engine_init(int width, int height) { return ms::Engine::instance().init(width, height) ? 0 : -1; }
int ms_project_new(const char* name, int width, int height, double fps) {
    return ms::Engine::instance().new_project(name ? name : "Untitled", width, height,
                                              static_cast<float>(fps)) ? 0 : -1;
}
void ms_engine_shutdown() { ms::Engine::instance().shutdown(); }
void ms_seek(double t) { ms::Engine::instance().seek(t); }
void ms_play() { ms::Engine::instance().play(); }
void ms_pause() { ms::Engine::instance().pause(); }
MSPreviewState ms_get_preview_state() {
    auto s = ms::Engine::instance().state();
    return {s.time, s.width, s.height, s.playing ? 1 : 0};
}
int ms_render_frame() { return ms::Engine::instance().render_frame() ? 0 : -1; }
int ms_project_load(const char* json) { return json && ms::Engine::instance().load_project_json(json) ? 0 : -1; }
int ms_project_save(const char* path) { return path && ms::Engine::instance().save_project(path).ok() ? 0 : -1; }
int ms_project_autosave(const char* path) { return path && ms::Engine::instance().autosave_project(path).ok() ? 0 : -1; }
double ms_perf_frame_ms() { return ms::Engine::instance().performance().frame_ms; }
double ms_perf_fps() { return ms::Engine::instance().performance().fps; }
unsigned long long ms_perf_cache_hits() { return (unsigned long long)ms::Engine::instance().performance().cache_hits; }
unsigned long long ms_perf_cache_misses() { return (unsigned long long)ms::Engine::instance().performance().cache_misses; }
char* ms_project_to_json() {
    std::string s = ms::Engine::instance().project_to_json();
    char* p = static_cast<char*>(std::malloc(s.size() + 1));
    if (p) std::memcpy(p, s.c_str(), s.size() + 1);
    return p;
}
void ms_free_string(char* p) { std::free(p); }
size_t ms_frame_bytes() { return ms::Engine::instance().last_frame_bytes(); }
int ms_copy_frame_rgba(void* rgba, size_t bytes) { return ms::Engine::instance().copy_last_frame(rgba, bytes) ? 0 : -1; }
int ms_import_media(const char* path) { return path ? static_cast<int>(ms::Engine::instance().project().import_media(path)) : -1; }
int ms_add_clip(int track_id, int media_id, double start, double duration) {
    // Goes through Engine so the edit is undoable and the frame cache is dropped.
    return static_cast<int>(ms::Engine::instance().add_clip(
        static_cast<ms::TrackId>(track_id), static_cast<ms::MediaId>(media_id), start, duration));
}
int ms_split_clip(int track_id, int clip_id, double at) {
    auto& e = ms::Engine::instance();
    const bool ok = e.project().split_clip(static_cast<ms::TrackId>(track_id),
                                          static_cast<ms::ClipId>(clip_id), at);
    if (ok) e.invalidate_frames();
    return ok ? 0 : -1;
}
int ms_undo() { return ms::Engine::instance().undo() ? 0 : -1; }
int ms_redo() { return ms::Engine::instance().redo() ? 0 : -1; }
MSAudioState ms_audio_state() {
    auto i = ms::Engine::instance().audio_info();
    return {i.sample_rate, i.channels, i.duration, i.time, i.playing ? 1 : 0};
}
int ms_audio_read_pcm(float* interleaved, int frames, int channels) {
    return ms::Engine::instance().read_audio(interleaved, frames, channels).ok() ? 0 : -1;
}
int ms_look_load(const char* path) {
    return (path && ms::Engine::instance().load_look(path)) ? 0 : -1;
}
void ms_look_clear() { ms::Engine::instance().clear_look(); }
void ms_look_set_strength(float strength) { ms::Engine::instance().set_look_strength(strength); }
float ms_look_strength() { return ms::Engine::instance().look_strength(); }
void ms_grade_set(float exposure, float contrast, float saturation, float temperature,
                  float lift, float gamma, float gain) {
    ms::Engine::instance().set_grade(exposure, contrast, saturation, temperature, lift, gamma, gain);
}
void ms_grade_clear() { ms::Engine::instance().clear_grade(); }
int ms_scopes_histogram(int* out, int bins) {
    return ms::Engine::instance().scopes_histogram(out, bins) ? 0 : -1;
}
int ms_transition_add(int track_id, int outgoing, int incoming, int type, double duration) {
    return static_cast<int>(ms::Engine::instance().add_transition(track_id, outgoing, incoming,
                                                                 type, duration));
}
int ms_transition_remove(int track_id, int id) {
    return ms::Engine::instance().remove_transition(track_id, static_cast<uint64_t>(id)) ? 0 : -1;
}
static ms::TextStyle make_text_style(const char* font, double size, const char* color_hex,
                                     int align, int anchor, double outline,
                                     const char* outline_hex, double shadow,
                                     double line_spacing, double tracking,
                                     double box_width, int uppercase) {
    ms::TextStyle s;
    if (font && *font) s.font = font;
    s.size = size > 0 ? static_cast<float>(size) : 120.f;
    if (color_hex && *color_hex) ms::set_text_color(s, color_hex);
    s.align = align == 0 ? ms::TextAlign::Left
              : align == 2 ? ms::TextAlign::Right : ms::TextAlign::Center;
    s.anchor = anchor == 0 ? ms::TextAnchor::Top
               : anchor == 2 ? ms::TextAnchor::Bottom : ms::TextAnchor::Middle;
    s.outline = outline > 0 ? static_cast<float>(outline) : 0.f;
    if (outline_hex && *outline_hex) {
        ms::TextStyle oc = s;
        ms::set_text_color(oc, outline_hex);
        s.outline_r = oc.r;
        s.outline_g = oc.g;
        s.outline_b = oc.b;
    }
    s.shadow = shadow > 0 ? static_cast<float>(shadow) : 0.f;
    s.line_spacing = line_spacing > 0 ? static_cast<float>(line_spacing) : 1.12f;
    s.tracking = static_cast<float>(tracking);
    s.box_width = box_width > 0 ? static_cast<float>(box_width) : 0.f;
    s.uppercase = uppercase != 0;
    return s;
}

int ms_marker_add(double time, const char* name) {
    return static_cast<int>(ms::Engine::instance().add_marker(time, name ? name : ""));
}
int ms_marker_remove(int id) {
    return ms::Engine::instance().remove_marker(static_cast<uint64_t>(id)) ? 0 : -1;
}
int ms_project_settings(int width, int height, double fps) {
    return ms::Engine::instance().set_project_settings(width, height, static_cast<float>(fps))
               ? 0 : -1;
}

int ms_clip_mask_set(int track_id, int clip_id, int shape, float x, float y, float w,
                     float h, float feather, int inverted) {
    return ms::Engine::instance().set_clip_mask(track_id, clip_id, shape, x, y, w, h, feather,
                                                inverted != 0) ? 0 : -1;
}
int ms_clip_mask_clear(int track_id, int clip_id) {
    return ms::Engine::instance().clear_clip_mask(track_id, clip_id) ? 0 : -1;
}
int ms_clip_mask_get(int track_id, int clip_id, float* out6, int* inverted) {
    bool inv = false;
    const bool ok = ms::Engine::instance().clip_mask(track_id, clip_id, out6, &inv);
    if (inverted) *inverted = inv ? 1 : 0;
    return ok ? 0 : -1;
}

int ms_clip_grade_set(int track_id, int clip_id, float exposure, float contrast,
                      float saturation, float temperature, float lift, float gamma,
                      float gain) {
    return ms::Engine::instance().set_clip_grade(track_id, clip_id, exposure, contrast,
                                                 saturation, temperature, lift, gamma,
                                                 gain) ? 0 : -1;
}
int ms_clip_grade_clear(int track_id, int clip_id) {
    return ms::Engine::instance().clear_clip_grade(track_id, clip_id) ? 0 : -1;
}
int ms_clip_grade_get(int track_id, int clip_id, float* out7) {
    return ms::Engine::instance().clip_grade(track_id, clip_id, out7) ? 0 : -1;
}

int ms_clip_peaks(int track_id, int clip_id, float* out, int bins) {
    return ms::Engine::instance().clip_peaks(track_id, clip_id, bins, out) ? 0 : -1;
}

int ms_text_set_font_dir(const char* dir) {
    ms::set_text_font_dir(dir ? dir : "");
    return 0;
}

int ms_text_add_clip(int track_id, double start, double duration, const char* text,
                     const char* font, double size, const char* color_hex,
                     int align, int anchor, double outline, const char* outline_hex,
                     double shadow, double line_spacing, double tracking,
                     double box_width, int uppercase) {
    const ms::TextStyle style = make_text_style(font, size, color_hex, align, anchor, outline,
                                                outline_hex, shadow, line_spacing, tracking,
                                                box_width, uppercase);
    return static_cast<int>(ms::Engine::instance().add_text_clip(
        track_id, start, duration, text ? text : "", style));
}

int ms_text_update_clip(int track_id, int clip_id, const char* text, const char* font,
                        double size, const char* color_hex, int align, int anchor,
                        double outline, const char* outline_hex, double shadow,
                        double line_spacing, double tracking, double box_width,
                        int uppercase) {
    const ms::TextStyle style = make_text_style(font, size, color_hex, align, anchor, outline,
                                                outline_hex, shadow, line_spacing, tracking,
                                                box_width, uppercase);
    return ms::Engine::instance().set_clip_text(track_id, clip_id, text ? text : "", style)
               ? 0 : -1;
}

int ms_text_measure(const char* text, const char* font, double size, double box_width,
                    int uppercase, double* out_w, double* out_h, int* out_lines) {
    ms::TextStyle style;
    if (font && *font) style.font = font;
    style.size = size > 0 ? static_cast<float>(size) : 120.f;
    style.box_width = box_width > 0 ? static_cast<float>(box_width) : 0.f;
    style.uppercase = uppercase != 0;
    ms::TextMetrics m;
    if (!ms::measure_text(style, text ? text : "", m)) return -1;
    if (out_w) *out_w = m.width;
    if (out_h) *out_h = m.height;
    if (out_lines) *out_lines = m.lines;
    return 0;
}

int ms_keyframe_add(int track_id, int clip_id, int prop, double time, double value) {
    return ms::Engine::instance().add_keyframe(track_id, clip_id, prop, time, value) ? 0 : -1;
}
int ms_keyframe_clear(int track_id, int clip_id, int prop) {
    return ms::Engine::instance().clear_keyframes(track_id, clip_id, prop) ? 0 : -1;
}
int ms_clip_remove(int track_id, int clip_id) {
    return ms::Engine::instance().remove_clip(track_id, clip_id) ? 0 : -1;
}
int ms_clip_move(int track_id, int clip_id, double start) {
    return ms::Engine::instance().move_clip(track_id, clip_id, start) ? 0 : -1;
}
int ms_clip_trim(int track_id, int clip_id, double start, double duration) {
    return ms::Engine::instance().trim_clip(track_id, clip_id, start, duration) ? 0 : -1;
}
int ms_clip_ripple_delete(int track_id, int clip_id) {
    return ms::Engine::instance().ripple_delete(track_id, clip_id) ? 0 : -1;
}
int ms_track_add(const char* name, int type) {
    return static_cast<int>(ms::Engine::instance().add_track(name ? name : "Track", type));
}
int ms_track_remove(int track_id) {
    return ms::Engine::instance().remove_track(track_id) ? 0 : -1;
}
int ms_track_flags(int track_id, int muted, int locked, int visible, int solo) {
    return ms::Engine::instance().set_track_flags(track_id, muted != 0, locked != 0,
                                                  visible != 0, solo != 0) ? 0 : -1;
}
int ms_effect_add(int track_id, int clip_id, const char* base_id) {
    return base_id ? ms::Engine::instance().add_effect(track_id, clip_id, base_id) : -1;
}
int ms_effect_remove(int track_id, int clip_id, int index) {
    return ms::Engine::instance().remove_effect(track_id, clip_id, index) ? 0 : -1;
}
int ms_effect_set_param(int track_id, int clip_id, int index, const char* name, float value) {
    return (name && ms::Engine::instance().set_effect_param(track_id, clip_id, index, name, value))
               ? 0
               : -1;
}
int ms_effect_set_enabled(int track_id, int clip_id, int index, int enabled) {
    return ms::Engine::instance().set_effect_enabled(track_id, clip_id, index, enabled != 0) ? 0 : -1;
}
int ms_audio_set_master_gain(float gain_db) {
    ms::AudioEffectChain& chain = ms::Engine::instance().audio_effects();
    ms::AudioFxParams gain;
    gain.kind = ms::AudioFxKind::Gain;
    gain.gain_db = gain_db;
    gain.enabled = true;
    if (chain.parameters().empty()) chain.add(gain);
    else if (!chain.set(0, gain)) return -1;
    return 0;
}
}
