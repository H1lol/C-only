#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Called from Flutter via FFI / Platform Channel

typedef struct {
    double time_seconds;
    int    width;
    int    height;
    int    is_playing;
} MSPreviewState;

// Initialize the native engine
int ms_engine_init(int width, int height);

// Replace the current document with a fresh project (name / size / fps).
int ms_project_new(const char* name, int width, int height, double fps);

// Shutdown
void ms_engine_shutdown();

// Load a project from JSON string
int ms_project_load(const char* json);

// Get current project as JSON (caller must free)
char* ms_project_to_json();

// Seek
void ms_seek(double time_seconds);

// Play / Pause
void ms_play();
void ms_pause();
int ms_project_save(const char* path);
int ms_project_autosave(const char* path);
double ms_perf_frame_ms();
double ms_perf_fps();
unsigned long long ms_perf_cache_hits();
unsigned long long ms_perf_cache_misses();
void ms_free_string(char* p);
size_t ms_frame_bytes();
int ms_copy_frame_rgba(void* rgba, size_t bytes);
int ms_import_media(const char* path);
int ms_add_clip(int track_id, int media_id, double start, double duration);
int ms_split_clip(int track_id, int clip_id, double at);
int ms_undo();
int ms_redo();

// Get current preview state
MSPreviewState ms_get_preview_state();

// Render one frame into the given texture / surface (platform specific)
int ms_render_frame();

// Timeline audio. The host audio thread pulls interleaved f32 PCM for the
// current playhead; the master insert chain is already applied.
typedef struct {
    int    sample_rate;
    int    channels;
    double duration;
    double time;
    int    is_playing;
} MSAudioState;

MSAudioState ms_audio_state();
int ms_audio_read_pcm(float* interleaved, int frames, int channels);
// Sets the master gain stage of the timeline insert chain (dB, 0 = unity).
int ms_audio_set_master_gain(float gain_db);

// Display look (3D .cube LUT) applied to the rendered preview frame.
// Returns 0 on success, -1 if the file is missing or malformed.
int ms_look_load(const char* path);
void ms_look_clear();
// strength: 0 = no effect, 1 = full look.
void ms_look_set_strength(float strength);
float ms_look_strength();

// ---- Timeline editing (each edit is undoable) ----
int ms_clip_remove(int track_id, int clip_id);
int ms_clip_move(int track_id, int clip_id, double start);
int ms_clip_trim(int track_id, int clip_id, double start, double duration);
int ms_clip_ripple_delete(int track_id, int clip_id);
int ms_track_add(const char* name, int type);
int ms_track_remove(int track_id);
int ms_track_flags(int track_id, int muted, int locked, int visible, int solo);

// ---- Master grade + scopes ----
void ms_grade_set(float exposure, float contrast, float saturation, float temperature,
                  float lift, float gamma, float gain);
void ms_grade_clear();
// Fills 4*bins ints (R, G, B, luma). Returns 0 on success.
int ms_scopes_histogram(int* out, int bins);

// ---- Transitions ----
// Returns the new transition id, or 0 on failure.
int ms_transition_add(int track_id, int outgoing, int incoming, int type, double duration);

// Which decoder backend is live: "mediacodec", "ffmpeg" or "slate".
// "slate" means imported video renders as a placeholder — surface it in the UI.
const char* ms_decoder_backend();

// Timeline markers. Returns the new marker id, or 0.
int ms_marker_add(double time, const char* name);
int ms_marker_remove(int id);
// Project size / frame rate without discarding edits. 0 on success.
int ms_project_settings(int width, int height, double fps);

// Shape mask on a clip. shape: 0 rectangle, 1 ellipse, 2 full frame. x/y/w/h are
// normalized 0..1 layer coordinates, feather is in output pixels.
int ms_clip_mask_set(int track_id, int clip_id, int shape, float x, float y, float w,
                     float h, float feather, int inverted);
int ms_clip_mask_clear(int track_id, int clip_id);
/** Writes shape/x/y/w/h/feather to out6 and the invert flag to inverted. */
int ms_clip_mask_get(int track_id, int clip_id, float* out6, int* inverted);

// Per-clip grade. Applied to the clip before compositing, independent of the
// master grade. Returns 0 on success.
int ms_clip_grade_set(int track_id, int clip_id, float exposure, float contrast,
                      float saturation, float temperature, float lift, float gamma,
                      float gain);
int ms_clip_grade_clear(int track_id, int clip_id);
/** Reads a clip's grade; writes 7 floats and returns 0, or -1 when ungraded. */
int ms_clip_grade_get(int track_id, int clip_id, float* out7);

// Peak envelope for a clip's audio: writes `bins` floats in 0..1. Returns 0 on
// success, -1 when the clip has no decodable audio (text clips, silent media).
int ms_clip_peaks(int track_id, int clip_id, float* out, int bins);

// ---- Text / titles ----
// Points the native rasteriser at the directory holding <family>.ttf files
// (host extracts them from assets once). Returns 0 on success.
int ms_text_set_font_dir(const char* dir);
/** Adds a title clip. font is a family name ("anton", "bebas", ...).
 *  align/anchor are 0=left/top, 1=center/middle, 2=right/bottom.
 *  color_hex/outline_hex are "#rrggbb". Returns the new clip id, or 0. */
int ms_text_add_clip(int track_id, double start, double duration, const char* text,
                     const char* font, double size, const char* color_hex,
                     int align, int anchor, double outline, const char* outline_hex,
                     double shadow, double line_spacing, double tracking,
                     double box_width, int uppercase);
// Updates an existing title clip (same argument layout). Returns 0 on success.
int ms_text_update_clip(int track_id, int clip_id, const char* text, const char* font,
                        double size, const char* color_hex, int align, int anchor,
                        double outline, const char* outline_hex, double shadow,
                        double line_spacing, double tracking, double box_width,
                        int uppercase);
// Text metrics for hit-testing/layout: returns 0 when the font resolves,
// writing width/height/lines into the out pointers.
int ms_text_measure(const char* text, const char* font, double size,
                    double box_width, int uppercase, double* out_w, double* out_h,
                    int* out_lines);
int ms_transition_remove(int track_id, int id);

// ---- Keyframes (prop: 0 opacity, 1 scale, 2 pos_x, 3 pos_y; time clip-local) ----
int ms_keyframe_add(int track_id, int clip_id, int prop, double time, double value);
int ms_keyframe_clear(int track_id, int clip_id, int prop);

// ---- Clip effect stack ----
// Returns the new effect's stack index, or -1 on failure.
int ms_effect_add(int track_id, int clip_id, const char* base_id);
int ms_effect_remove(int track_id, int clip_id, int index);
int ms_effect_set_param(int track_id, int clip_id, int index, const char* name, float value);
int ms_effect_set_enabled(int track_id, int clip_id, int index, int enabled);

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
}
#endif
