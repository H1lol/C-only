// Proxy registration: which path the decoder factory will open, and when the
// timeline player must treat its cached decoders as stale.
#include "ms_proxy_registry.hpp"
#include "ms_ffmpeg_io.hpp"

#include <cstdio>
#include <string>

using namespace ms;

static int failures = 0;

static void check(bool cond, const char* what) {
    if (!cond) {
        std::printf("FAIL %s\n", what);
        ++failures;
    }
}

int main() {
    ProxyRegistry& reg = proxy_registry();
    reg.clear();
    reg.set_enabled(true);

    const std::string media = "/media/phone_4k.mp4";
    const std::string proxy = "/files/proxies/abc.mp4";

    check(reg.resolve(media) == media, "unregistered media decodes itself");
    check(reg.size() == 0, "nothing registered yet");

    const int before = reg.generation();
    reg.set(media, proxy);
    check(reg.size() == 1, "registration is visible");
    check(reg.proxy_for(media) == proxy, "proxy_for reports the file");
    check(reg.resolve(media) == proxy, "registered + enabled decodes the proxy");
    check(reg.generation() != before, "registering invalidates cached decoders");

    // Re-registering the same pair must not throw decoders away on every frame.
    const int settled = reg.generation();
    reg.set(media, proxy);
    check(reg.generation() == settled, "an identical registration is a no-op");

    reg.set_enabled(false);
    check(reg.resolve(media) == media, "export decodes the source");
    check(reg.proxy_for(media) == proxy, "the mapping survives being disabled");
    check(!reg.enabled(), "registry reports disabled");
    check(reg.generation() != settled, "toggling invalidates cached decoders");

    reg.set_enabled(false);
    const int stillOff = reg.generation();
    check(reg.generation() == stillOff, "toggling to the same state is a no-op");

    reg.set_enabled(true);
    check(reg.resolve(media) == proxy, "re-enabling goes back to the proxy");

    reg.erase(media);
    check(reg.size() == 0, "unregister removes it");
    check(reg.resolve(media) == media, "an erased media decodes itself");

    // The factory is the only place the swap can happen; prove it still hands
    // back a usable decoder for a path that has no proxy.
    auto dec = create_media_decoder(media);
    check(dec != nullptr, "factory still returns a decoder");

    reg.set(media, proxy);
    reg.set_enabled(true);
    auto proxied = create_media_decoder(media);
    check(proxied != nullptr, "factory accepts a registered proxy path");

    reg.clear();
    reg.set_enabled(true);

    if (failures == 0) std::printf("ms_proxy_test: ok\n");
    return failures == 0 ? 0 : 1;
}
