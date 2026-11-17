// Exercises the master audio insert chain through the C ABI the app uses.
// The DSP itself is covered by ms_audio_fx_test; this checks the knobs the
// mixer panel reaches: add / remove / clear / enable / named parameters, and
// the track fader.
#include "../bridge/ms_bridge.h"

#include <cmath>
#include <cstdio>
#include <iostream>

static int fails = 0;
#define CHECK(x, name) do { if (!(x)) { std::cerr << "FAIL " << name "\n"; ++fails; } \
                            else std::cout << "ok " << name "\n"; } while (0)

int main() {
    CHECK(ms_engine_init(1920, 1080) == 0, "engine starts");

    ms_audio_fx_clear();
    CHECK(ms_audio_fx_count() == 0, "chain starts empty");

    const int eq = ms_audio_fx_add(1);
    CHECK(eq >= 0, "EQ insert added");
    CHECK(ms_audio_fx_count() == 1, "insert count follows add");
    CHECK(ms_audio_fx_kind(eq) == 1, "insert reports its kind");
    CHECK(ms_audio_fx_enabled(eq) == 1, "new insert is enabled");

    CHECK(ms_audio_fx_set_param(eq, "low_gain_db", 6.0f) == 0, "named parameter accepted");
    CHECK(std::abs(ms_audio_fx_get_param(eq, "low_gain_db", 0.0f) - 6.0f) < 1e-4f,
          "named parameter reads back");
    CHECK(ms_audio_fx_get_param(eq, "low_freq_hz", -1.0f) > 0.0f, "defaults survive an edit");
    CHECK(ms_audio_fx_set_param(eq, "not_a_knob", 1.0f) == -1, "unknown parameter rejected");
    CHECK(std::abs(ms_audio_fx_get_param(eq, "not_a_knob", 42.0f) - 42.0f) < 1e-4f,
          "unknown parameter returns the fallback");
    CHECK(ms_audio_fx_set_param(7, "wet", 0.5f) == -1, "out-of-range insert rejected");

    CHECK(ms_audio_fx_set_enabled(eq, 0) == 0, "insert can be bypassed");
    CHECK(ms_audio_fx_enabled(eq) == 0, "bypass reads back");
    ms_audio_fx_set_enabled(eq, 1);

    CHECK(ms_audio_fx_add(2) == 1, "compressor appended at the end");
    CHECK(ms_audio_fx_add(0) == 2, "gain appended at the end");
    CHECK(ms_audio_fx_count() == 3, "three inserts in the chain");
    CHECK(ms_audio_fx_add(99) == -1, "unknown kind rejected");
    CHECK(ms_audio_fx_kind(9) == -1, "out-of-range kind query rejected");

    CHECK(ms_audio_fx_remove(1) == 0, "middle insert removed");
    CHECK(ms_audio_fx_count() == 2, "count follows remove");
    CHECK(ms_audio_fx_kind(1) == 0, "later inserts shift down");
    CHECK(ms_audio_fx_remove(9) == -1, "removing a missing insert fails cleanly");

    CHECK(ms_audio_fx_clear() == 0, "chain cleared");
    CHECK(ms_audio_fx_count() == 0, "cleared chain is empty");

    // The older single-knob API still works and lands in the same chain.
    CHECK(ms_audio_set_master_gain(-6.0f) == 0, "master gain writes the chain");
    CHECK(ms_audio_fx_count() == 1 && ms_audio_fx_kind(0) == 0, "master gain is insert 0");
    CHECK(std::abs(ms_audio_fx_get_param(0, "gain_db", 0.0f) + 6.0f) < 1e-4f,
          "master gain is readable as gain_db");

    // Track fader.
    CHECK(ms_project_new("Audio bridge", 1920, 1080, 30) == 0, "project replaced");
    const int track = ms_track_add("A1", 1);
    CHECK(track > 0, "audio track added");
    CHECK(ms_track_gain(track, 0.5f) == 0, "track gain accepted");
    CHECK(ms_track_gain(9999, 0.5f) == -1, "track gain rejects a missing track");

    // Without a session track the mix still runs and the chain applies.
    float pcm[2 * 64];
    const int rc = ms_audio_read_pcm(pcm, 64, 2);
    CHECK(rc == 0, "timeline audio still pulls PCM with inserts loaded");

    ms_audio_fx_clear();
    ms_engine_shutdown();
    std::cout << (fails ? "AUDIO BRIDGE TEST FAILED\n" : "AUDIO BRIDGE TEST PASSED\n");
    return fails ? 1 : 0;
}
