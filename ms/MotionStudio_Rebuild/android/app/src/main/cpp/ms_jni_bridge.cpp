// JNI bridge: the Kotlin UI drives the C++ engine through the C ABI declared in
// native/cpp/bridge/ms_bridge.h. Nothing engine-specific is reimplemented here.
#include <jni.h>
#include <android/bitmap.h>
#include <android/log.h>
#include <string>
#include "bridge/ms_bridge.h"

#define LOG_TAG "MotionStudioJNI"
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace {

std::string to_std(JNIEnv* env, jstring value) {
    if (!value) return {};
    const char* chars = env->GetStringUTFChars(value, nullptr);
    std::string out = chars ? chars : "";
    if (chars) env->ReleaseStringUTFChars(value, chars);
    return out;
}

jdoubleArray to_doubles(JNIEnv* env, const double* values, jsize count) {
    jdoubleArray arr = env->NewDoubleArray(count);
    if (arr) env->SetDoubleArrayRegion(arr, 0, count, values);
    return arr;
}

jstring to_jstring(JNIEnv* env, const std::string& value) {
    return env->NewStringUTF(value.c_str());
}

} // namespace

extern "C" {

JNIEXPORT jboolean JNICALL
Java_com_motionstudio_app_native_NativeEngine_nativeInit(JNIEnv*, jobject, jint width, jint height) {
    return ms_engine_init(width, height) == 0 ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_com_motionstudio_app_native_NativeEngine_nativeNewProject(JNIEnv* env, jobject, jstring name,
                                                               jint width, jint height, jdouble fps) {
    const std::string n = to_std(env, name);
    return ms_project_new(n.c_str(), width, height, fps) == 0 ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL
Java_com_motionstudio_app_native_NativeEngine_nativeShutdown(JNIEnv*, jobject) {
    ms_engine_shutdown();
}

JNIEXPORT jboolean JNICALL
Java_com_motionstudio_app_native_NativeEngine_nativeProjectLoad(JNIEnv* env, jobject, jstring json) {
    const std::string j = to_std(env, json);
    return ms_project_load(j.c_str()) == 0 ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jstring JNICALL
Java_com_motionstudio_app_native_NativeEngine_nativeProjectToJson(JNIEnv* env, jobject) {
    char* json = ms_project_to_json();
    if (!json) return to_jstring(env, "{}");
    std::string out(json);
    ms_free_string(json);
    return to_jstring(env, out);
}

JNIEXPORT jboolean JNICALL
Java_com_motionstudio_app_native_NativeEngine_nativeSaveProject(JNIEnv* env, jobject, jstring path) {
    const std::string p = to_std(env, path);
    return ms_project_save(p.c_str()) == 0 ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_com_motionstudio_app_native_NativeEngine_nativeAutosaveProject(JNIEnv* env, jobject, jstring path) {
    const std::string p = to_std(env, path);
    return ms_project_autosave(p.c_str()) == 0 ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL
Java_com_motionstudio_app_native_NativeEngine_nativeSeek(JNIEnv*, jobject, jdouble seconds) {
    ms_seek(seconds);
}

JNIEXPORT void JNICALL
Java_com_motionstudio_app_native_NativeEngine_nativePlay(JNIEnv*, jobject) { ms_play(); }

JNIEXPORT void JNICALL
Java_com_motionstudio_app_native_NativeEngine_nativePause(JNIEnv*, jobject) { ms_pause(); }

JNIEXPORT jboolean JNICALL
Java_com_motionstudio_app_native_NativeEngine_nativeRenderFrame(JNIEnv*, jobject) {
    return ms_render_frame() == 0 ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jlong JNICALL
Java_com_motionstudio_app_native_NativeEngine_nativeFrameBytes(JNIEnv*, jobject) {
    return static_cast<jlong>(ms_frame_bytes());
}

JNIEXPORT jboolean JNICALL
Java_com_motionstudio_app_native_NativeEngine_nativeCopyFrame(JNIEnv* env, jobject, jobject bitmap) {
    if (!bitmap) return JNI_FALSE;
    AndroidBitmapInfo info{};
    if (AndroidBitmap_getInfo(env, bitmap, &info) != ANDROID_BITMAP_RESULT_SUCCESS) return JNI_FALSE;
    if (info.format != ANDROID_BITMAP_FORMAT_RGBA_8888) {
        LOGE("copyFrame: bitmap must be RGBA_8888");
        return JNI_FALSE;
    }
    void* pixels = nullptr;
    if (AndroidBitmap_lockPixels(env, bitmap, &pixels) != ANDROID_BITMAP_RESULT_SUCCESS) return JNI_FALSE;
    const size_t bytes = static_cast<size_t>(info.stride) * info.height;
    const int rc = ms_copy_frame_rgba(pixels, bytes);
    AndroidBitmap_unlockPixels(env, bitmap);
    return rc == 0 ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jint JNICALL
Java_com_motionstudio_app_native_NativeEngine_nativeImportMedia(JNIEnv* env, jobject, jstring path) {
    const std::string p = to_std(env, path);
    return ms_import_media(p.c_str());
}

JNIEXPORT jint JNICALL
Java_com_motionstudio_app_native_NativeEngine_nativeAddClip(JNIEnv*, jobject, jint trackId,
                                                            jint mediaId, jdouble start,
                                                            jdouble duration) {
    return ms_add_clip(trackId, mediaId, start, duration);
}

JNIEXPORT jboolean JNICALL
Java_com_motionstudio_app_native_NativeEngine_nativeSplitClip(JNIEnv*, jobject, jint trackId,
                                                              jint clipId, jdouble at) {
    return ms_split_clip(trackId, clipId, at) == 0 ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_com_motionstudio_app_native_NativeEngine_nativeUndo(JNIEnv*, jobject) {
    return ms_undo() == 0 ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_com_motionstudio_app_native_NativeEngine_nativeRedo(JNIEnv*, jobject) {
    return ms_redo() == 0 ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jdoubleArray JNICALL
Java_com_motionstudio_app_native_NativeEngine_nativePreviewState(JNIEnv* env, jobject) {
    MSAudioState unused{};
    MSPreviewState s = ms_get_preview_state();
    (void)unused;
    const double values[4] = {s.time_seconds, static_cast<double>(s.width),
                              static_cast<double>(s.height), s.is_playing ? 1.0 : 0.0};
    return to_doubles(env, values, 4);
}

JNIEXPORT jdoubleArray JNICALL
Java_com_motionstudio_app_native_NativeEngine_nativeAudioState(JNIEnv* env, jobject) {
    MSAudioState s = ms_audio_state();
    const double values[5] = {static_cast<double>(s.sample_rate), static_cast<double>(s.channels),
                              s.duration, s.time, s.is_playing ? 1.0 : 0.0};
    return to_doubles(env, values, 5);
}

JNIEXPORT jboolean JNICALL
Java_com_motionstudio_app_native_NativeEngine_nativeReadAudioPcm(JNIEnv* env, jobject,
                                                                 jfloatArray buffer, jint frames,
                                                                 jint channels) {
    if (!buffer || frames <= 0 || channels <= 0) return JNI_FALSE;
    const jsize len = env->GetArrayLength(buffer);
    if (len < frames * channels) return JNI_FALSE;
    jfloat* data = env->GetFloatArrayElements(buffer, nullptr);
    if (!data) return JNI_FALSE;
    const int rc = ms_audio_read_pcm(reinterpret_cast<float*>(data), frames, channels);
    env->ReleaseFloatArrayElements(buffer, data, 0);
    return rc == 0 ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_com_motionstudio_app_native_NativeEngine_nativeSetMasterGain(JNIEnv*, jobject, jfloat db) {
    return ms_audio_set_master_gain(db) == 0 ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_com_motionstudio_app_native_NativeEngine_nativeLookLoad(JNIEnv* env, jobject, jstring path) {
    if (!path) return JNI_FALSE;
    const char* utf = env->GetStringUTFChars(path, nullptr);
    if (!utf) return JNI_FALSE;
    const bool ok = ms_look_load(utf) == 0;
    env->ReleaseStringUTFChars(path, utf);
    return ok ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL
Java_com_motionstudio_app_native_NativeEngine_nativeLookClear(JNIEnv*, jobject) {
    ms_look_clear();
}

JNIEXPORT void JNICALL
Java_com_motionstudio_app_native_NativeEngine_nativeLookSetStrength(JNIEnv*, jobject, jfloat strength) {
    ms_look_set_strength(strength);
}

JNIEXPORT jfloat JNICALL
Java_com_motionstudio_app_native_NativeEngine_nativeLookStrength(JNIEnv*, jobject) {
    return ms_look_strength();
}

JNIEXPORT void JNICALL
Java_com_motionstudio_app_native_NativeEngine_nativeGradeSet(JNIEnv*, jobject, jfloat exposure,
    jfloat contrast, jfloat saturation, jfloat temperature, jfloat lift, jfloat gamma, jfloat gain) {
    ms_grade_set(exposure, contrast, saturation, temperature, lift, gamma, gain);
}

JNIEXPORT void JNICALL
Java_com_motionstudio_app_native_NativeEngine_nativeGradeClear(JNIEnv*, jobject) {
    ms_grade_clear();
}

JNIEXPORT jboolean JNICALL
Java_com_motionstudio_app_native_NativeEngine_nativeScopesHistogram(JNIEnv* env, jobject, jintArray out, jint bins) {
    if (!out || bins <= 0) return JNI_FALSE;
    if (env->GetArrayLength(out) < bins * 4) return JNI_FALSE;
    jint* buf = env->GetIntArrayElements(out, nullptr);
    if (!buf) return JNI_FALSE;
    const bool ok = ms_scopes_histogram(buf, bins) == 0;
    env->ReleaseIntArrayElements(out, buf, 0);
    return ok ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jint JNICALL
Java_com_motionstudio_app_native_NativeEngine_nativeTransitionAdd(JNIEnv*, jobject, jint track,
        jint outgoing, jint incoming, jint type, jdouble duration) {
    return ms_transition_add(track, outgoing, incoming, type, duration);
}

JNIEXPORT jboolean JNICALL
Java_com_motionstudio_app_native_NativeEngine_nativeTransitionRemove(JNIEnv*, jobject, jint track, jint id) {
    return ms_transition_remove(track, id) == 0 ? JNI_TRUE : JNI_FALSE;
}


JNIEXPORT jint JNICALL
Java_com_motionstudio_app_native_NativeEngine_nativeCompNodeCount(JNIEnv*, jobject) {
    return ms_comp_node_count();
}

JNIEXPORT jboolean JNICALL
Java_com_motionstudio_app_native_NativeEngine_nativeCompNodeAt(JNIEnv* env, jobject, jint index,
                                                               jintArray out) {
    if (!out || env->GetArrayLength(out) < 5) return JNI_FALSE;
    jint id = 0, op = 0, enabled = 0;
    jfloat x = 0, y = 0;
    if (ms_comp_node_at(index, &id, &op, &enabled, &x, &y) != 0) return JNI_FALSE;
    const jint values[5] = {id, op, enabled, static_cast<jint>(x), static_cast<jint>(y)};
    env->SetIntArrayRegion(out, 0, 5, values);
    return JNI_TRUE;
}

JNIEXPORT jfloatArray JNICALL
Java_com_motionstudio_app_native_NativeEngine_nativeCompNodePos(JNIEnv* env, jobject, jint index) {
    jint id = 0, op = 0, enabled = 0;
    jfloat x = 0, y = 0;
    if (ms_comp_node_at(index, &id, &op, &enabled, &x, &y) != 0) return nullptr;
    jfloat values[2] = {x, y};
    jfloatArray arr = env->NewFloatArray(2);
    if (arr) env->SetFloatArrayRegion(arr, 0, 2, values);
    return arr;
}

JNIEXPORT jint JNICALL
Java_com_motionstudio_app_native_NativeEngine_nativeCompEdgeCount(JNIEnv*, jobject) {
    return ms_comp_edge_count();
}

JNIEXPORT jboolean JNICALL
Java_com_motionstudio_app_native_NativeEngine_nativeCompEdgeAt(JNIEnv* env, jobject, jint index,
                                                               jintArray out) {
    if (!out || env->GetArrayLength(out) < 3) return JNI_FALSE;
    jint from = 0, to = 0, slot = 0;
    if (ms_comp_edge_at(index, &from, &to, &slot) != 0) return JNI_FALSE;
    const jint values[3] = {from, to, slot};
    env->SetIntArrayRegion(out, 0, 3, values);
    return JNI_TRUE;
}

JNIEXPORT jint JNICALL
Java_com_motionstudio_app_native_NativeEngine_nativeCompAddNode(JNIEnv*, jobject, jint op,
                                                                jfloat x, jfloat y) {
    return ms_comp_add_node(op, x, y);
}

JNIEXPORT jboolean JNICALL
Java_com_motionstudio_app_native_NativeEngine_nativeCompRemoveNode(JNIEnv*, jobject, jint id) {
    return ms_comp_remove_node(id) == 0 ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_com_motionstudio_app_native_NativeEngine_nativeCompConnect(JNIEnv*, jobject, jint from,
                                                                jint to, jint slot) {
    return ms_comp_connect(from, to, slot) == 0 ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_com_motionstudio_app_native_NativeEngine_nativeCompDisconnect(JNIEnv*, jobject, jint from,
                                                                   jint to, jint slot) {
    return ms_comp_disconnect(from, to, slot) == 0 ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_com_motionstudio_app_native_NativeEngine_nativeCompSetParam(JNIEnv* env, jobject, jint id,
                                                                 jstring name, jdouble value) {
    const char* utf = name ? env->GetStringUTFChars(name, nullptr) : nullptr;
    const bool ok = utf && ms_comp_set_param(id, utf, value) == 0;
    if (utf) env->ReleaseStringUTFChars(name, utf);
    return ok ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jdouble JNICALL
Java_com_motionstudio_app_native_NativeEngine_nativeCompGetParam(JNIEnv* env, jobject, jint id,
                                                                 jstring name, jdouble fallback) {
    const char* utf = name ? env->GetStringUTFChars(name, nullptr) : nullptr;
    double value = fallback;
    const bool ok = utf && ms_comp_get_param(id, utf, &value) == 0;
    if (utf) env->ReleaseStringUTFChars(name, utf);
    return ok ? value : fallback;
}

JNIEXPORT jboolean JNICALL
Java_com_motionstudio_app_native_NativeEngine_nativeCompSetMedia(JNIEnv* env, jobject, jint id,
                                                                 jstring path) {
    const char* utf = path ? env->GetStringUTFChars(path, nullptr) : nullptr;
    const int rc = ms_comp_set_media(id, utf ? utf : "");
    if (utf) env->ReleaseStringUTFChars(path, utf);
    return rc == 0 ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jstring JNICALL
Java_com_motionstudio_app_native_NativeEngine_nativeCompGetMedia(JNIEnv* env, jobject, jint id) {
    char buf[1024];
    if (ms_comp_get_media(id, buf, sizeof(buf)) < 0) return env->NewStringUTF("");
    return env->NewStringUTF(buf);
}

JNIEXPORT jboolean JNICALL
Java_com_motionstudio_app_native_NativeEngine_nativeCompSetEnabled(JNIEnv*, jobject, jint id,
                                                                   jboolean enabled) {
    return ms_comp_set_enabled(id, enabled ? 1 : 0) == 0 ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_com_motionstudio_app_native_NativeEngine_nativeCompSetPosition(JNIEnv*, jobject, jint id,
                                                                    jfloat x, jfloat y) {
    return ms_comp_set_position(id, x, y) == 0 ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_com_motionstudio_app_native_NativeEngine_nativeCompSetSink(JNIEnv*, jobject, jint id) {
    return ms_comp_set_sink(id) == 0 ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jint JNICALL
Java_com_motionstudio_app_native_NativeEngine_nativeCompSink(JNIEnv*, jobject) {
    return ms_comp_sink();
}

JNIEXPORT jboolean JNICALL
Java_com_motionstudio_app_native_NativeEngine_nativeCompSetActive(JNIEnv*, jobject,
                                                                  jboolean active) {
    return ms_comp_set_active(active ? 1 : 0) == 0 ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_com_motionstudio_app_native_NativeEngine_nativeCompActive(JNIEnv*, jobject) {
    return ms_comp_active() ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jstring JNICALL
Java_com_motionstudio_app_native_NativeEngine_nativeDecoderBackend(JNIEnv* env, jobject) {
    const char* name = ms_decoder_backend();
    return env->NewStringUTF(name ? name : "unknown");
}

JNIEXPORT jint JNICALL
Java_com_motionstudio_app_native_NativeEngine_nativeMarkerAdd(JNIEnv* env, jobject, jdouble time,
        jstring name) {
    const char* n = name ? env->GetStringUTFChars(name, nullptr) : nullptr;
    const int id = ms_marker_add(time, n);
    if (n) env->ReleaseStringUTFChars(name, n);
    return id;
}

JNIEXPORT jboolean JNICALL
Java_com_motionstudio_app_native_NativeEngine_nativeMarkerRemove(JNIEnv*, jobject, jint id) {
    return ms_marker_remove(id) == 0 ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_com_motionstudio_app_native_NativeEngine_nativeProjectSettings(JNIEnv*, jobject, jint width,
        jint height, jdouble fps) {
    return ms_project_settings(width, height, fps) == 0 ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_com_motionstudio_app_native_NativeEngine_nativeClipMaskSet(JNIEnv*, jobject, jint track,
        jint clip, jint shape, jfloat x, jfloat y, jfloat w, jfloat h, jfloat feather,
        jboolean inverted) {
    return ms_clip_mask_set(track, clip, shape, x, y, w, h, feather, inverted ? 1 : 0) == 0
               ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_com_motionstudio_app_native_NativeEngine_nativeClipMaskClear(JNIEnv*, jobject, jint track,
        jint clip) {
    return ms_clip_mask_clear(track, clip) == 0 ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_com_motionstudio_app_native_NativeEngine_nativeClipMaskGet(JNIEnv* env, jobject, jint track,
        jint clip, jfloatArray out, jintArray invertedOut) {
    if (!out || env->GetArrayLength(out) < 6) return JNI_FALSE;
    float tmp[6] = {};
    int inv = 0;
    if (ms_clip_mask_get(track, clip, tmp, &inv) != 0) return JNI_FALSE;
    env->SetFloatArrayRegion(out, 0, 6, tmp);
    if (invertedOut && env->GetArrayLength(invertedOut) >= 1) env->SetIntArrayRegion(invertedOut, 0, 1, &inv);
    return JNI_TRUE;
}

JNIEXPORT jboolean JNICALL
Java_com_motionstudio_app_native_NativeEngine_nativeClipGradeSet(JNIEnv*, jobject, jint track,
        jint clip, jfloat exposure, jfloat contrast, jfloat saturation, jfloat temperature,
        jfloat lift, jfloat gamma, jfloat gain) {
    return ms_clip_grade_set(track, clip, exposure, contrast, saturation, temperature, lift,
                             gamma, gain) == 0 ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_com_motionstudio_app_native_NativeEngine_nativeClipGradeClear(JNIEnv*, jobject, jint track,
        jint clip) {
    return ms_clip_grade_clear(track, clip) == 0 ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_com_motionstudio_app_native_NativeEngine_nativeClipGradeGet(JNIEnv* env, jobject, jint track,
        jint clip, jfloatArray out) {
    if (!out || env->GetArrayLength(out) < 7) return JNI_FALSE;
    float tmp[7];
    if (ms_clip_grade_get(track, clip, tmp) != 0) return JNI_FALSE;
    env->SetFloatArrayRegion(out, 0, 7, tmp);
    return JNI_TRUE;
}

JNIEXPORT jboolean JNICALL
Java_com_motionstudio_app_native_NativeEngine_nativeClipPeaks(JNIEnv* env, jobject, jint track,
        jint clip, jfloatArray out, jint bins) {
    if (!out || bins <= 0 || bins > 512) return JNI_FALSE;
    if (env->GetArrayLength(out) < bins) return JNI_FALSE;
    float tmp[512];
    if (ms_clip_peaks(track, clip, tmp, bins) != 0) return JNI_FALSE;
    env->SetFloatArrayRegion(out, 0, bins, tmp);
    return JNI_TRUE;
}

JNIEXPORT jboolean JNICALL
Java_com_motionstudio_app_native_NativeEngine_nativeTextSetFontDir(JNIEnv* env, jobject, jstring dir) {
    const char* d = dir ? env->GetStringUTFChars(dir, nullptr) : nullptr;
    const int rc = ms_text_set_font_dir(d);
    if (d) env->ReleaseStringUTFChars(dir, d);
    return rc == 0 ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jint JNICALL
Java_com_motionstudio_app_native_NativeEngine_nativeTextAddClip(JNIEnv* env, jobject, jint track,
        jdouble start, jdouble duration, jstring text, jstring font, jdouble size,
        jstring color, jint align, jint anchor, jdouble outline, jstring outline_color,
        jdouble shadow, jdouble line_spacing, jdouble tracking, jdouble box_width,
        jboolean uppercase) {
    const char* t = env->GetStringUTFChars(text, nullptr);
    const char* f = env->GetStringUTFChars(font, nullptr);
    const char* c = env->GetStringUTFChars(color, nullptr);
    const char* oc = env->GetStringUTFChars(outline_color, nullptr);
    const int id = ms_text_add_clip(track, start, duration, t, f, size, c, align, anchor,
                                    outline, oc, shadow, line_spacing, tracking, box_width,
                                    uppercase ? 1 : 0);
    env->ReleaseStringUTFChars(outline_color, oc);
    env->ReleaseStringUTFChars(color, c);
    env->ReleaseStringUTFChars(font, f);
    env->ReleaseStringUTFChars(text, t);
    return id;
}

JNIEXPORT jboolean JNICALL
Java_com_motionstudio_app_native_NativeEngine_nativeTextUpdateClip(JNIEnv* env, jobject, jint track,
        jint clip, jstring text, jstring font, jdouble size, jstring color, jint align,
        jint anchor, jdouble outline, jstring outline_color, jdouble shadow,
        jdouble line_spacing, jdouble tracking, jdouble box_width, jboolean uppercase) {
    const char* t = env->GetStringUTFChars(text, nullptr);
    const char* f = env->GetStringUTFChars(font, nullptr);
    const char* c = env->GetStringUTFChars(color, nullptr);
    const char* oc = env->GetStringUTFChars(outline_color, nullptr);
    const int rc = ms_text_update_clip(track, clip, t, f, size, c, align, anchor, outline, oc,
                                       shadow, line_spacing, tracking, box_width,
                                       uppercase ? 1 : 0);
    env->ReleaseStringUTFChars(outline_color, oc);
    env->ReleaseStringUTFChars(color, c);
    env->ReleaseStringUTFChars(font, f);
    env->ReleaseStringUTFChars(text, t);
    return rc == 0 ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_com_motionstudio_app_native_NativeEngine_nativeKeyframeAdd(JNIEnv*, jobject, jint track, jint clip,
                                                              jint prop, jdouble time, jdouble value) {
    return ms_keyframe_add(track, clip, prop, time, value) == 0 ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_com_motionstudio_app_native_NativeEngine_nativeKeyframeClear(JNIEnv*, jobject, jint track, jint clip, jint prop) {
    return ms_keyframe_clear(track, clip, prop) == 0 ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_com_motionstudio_app_native_NativeEngine_nativeClipRemove(JNIEnv*, jobject, jint track, jint clip) {
    return ms_clip_remove(track, clip) == 0 ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_com_motionstudio_app_native_NativeEngine_nativeClipMove(JNIEnv*, jobject, jint track, jint clip, jdouble start) {
    return ms_clip_move(track, clip, start) == 0 ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_com_motionstudio_app_native_NativeEngine_nativeClipTrim(JNIEnv*, jobject, jint track, jint clip, jdouble start, jdouble duration) {
    return ms_clip_trim(track, clip, start, duration) == 0 ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_com_motionstudio_app_native_NativeEngine_nativeClipRippleDelete(JNIEnv*, jobject, jint track, jint clip) {
    return ms_clip_ripple_delete(track, clip) == 0 ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jint JNICALL
Java_com_motionstudio_app_native_NativeEngine_nativeTrackAdd(JNIEnv* env, jobject, jstring name, jint type) {
    const char* utf = name ? env->GetStringUTFChars(name, nullptr) : nullptr;
    const int id = ms_track_add(utf ? utf : "Track", type);
    if (utf) env->ReleaseStringUTFChars(name, utf);
    return id;
}

JNIEXPORT jboolean JNICALL
Java_com_motionstudio_app_native_NativeEngine_nativeTrackRemove(JNIEnv*, jobject, jint track) {
    return ms_track_remove(track) == 0 ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_com_motionstudio_app_native_NativeEngine_nativeTrackFlags(JNIEnv*, jobject, jint track, jint muted,
                                                              jint locked, jint visible, jint solo) {
    return ms_track_flags(track, muted, locked, visible, solo) == 0 ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jint JNICALL
Java_com_motionstudio_app_native_NativeEngine_nativeEffectAdd(JNIEnv* env, jobject, jint track, jint clip, jstring base_id) {
    const char* utf = base_id ? env->GetStringUTFChars(base_id, nullptr) : nullptr;
    const int index = ms_effect_add(track, clip, utf);
    if (utf) env->ReleaseStringUTFChars(base_id, utf);
    return index;
}

JNIEXPORT jboolean JNICALL
Java_com_motionstudio_app_native_NativeEngine_nativeEffectRemove(JNIEnv*, jobject, jint track, jint clip, jint index) {
    return ms_effect_remove(track, clip, index) == 0 ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_com_motionstudio_app_native_NativeEngine_nativeEffectSetParam(JNIEnv* env, jobject, jint track, jint clip,
                                                                  jint index, jstring name, jfloat value) {
    const char* utf = name ? env->GetStringUTFChars(name, nullptr) : nullptr;
    const bool ok = utf && ms_effect_set_param(track, clip, index, utf, value) == 0;
    if (utf) env->ReleaseStringUTFChars(name, utf);
    return ok ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_com_motionstudio_app_native_NativeEngine_nativeEffectSetEnabled(JNIEnv*, jobject, jint track, jint clip,
                                                                    jint index, jint enabled) {
    return ms_effect_set_enabled(track, clip, index, enabled) == 0 ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jdouble JNICALL
Java_com_motionstudio_app_native_NativeEngine_nativePerfFrameMs(JNIEnv*, jobject) {
    return ms_perf_frame_ms();
}

JNIEXPORT jdouble JNICALL
Java_com_motionstudio_app_native_NativeEngine_nativePerfFps(JNIEnv*, jobject) {
    return ms_perf_fps();
}

} // extern "C"
