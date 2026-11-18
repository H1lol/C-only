// Native crash capture.
//
// A segfault or an abort inside the engine kills the process before the Java
// uncaught-exception handler can run, so the conventional Android result is a
// silent restart. This installs a signal handler on an alternate stack that
// writes a short report — signal, fault address and a return-address backtrace —
// to a file the app can show in Settings.
//
// Everything inside the handler is async-signal-safe: only write(2) and a
// pre-allocated stack area. `_Unwind_Backtrace` is not formally signal-safe but
// is the standard trade-off (it only walks frame records already on the stack).

#include <jni.h>

#if defined(__ANDROID__)

#include <android/log.h>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include <unwind.h>

namespace {

constexpr size_t kAltStackSize = 64 * 1024;
constexpr int kMaxFrames = 24;

char g_alt_stack[kAltStackSize];
int g_fd = -1;
volatile sig_atomic_t g_active = 0;

// ---- async-signal-safe formatting -----------------------------------------

void put(const char* text) {
  if (g_fd >= 0 && text != nullptr) {
    ssize_t ignored = ::write(g_fd, text, std::strlen(text));
    (void)ignored;
  }
}

void put_uint(uint64_t value) {
  char buf[24];
  int i = static_cast<int>(sizeof(buf)) - 1;
  buf[i] = '\0';
  if (value == 0) {
    buf[--i] = '0';
  } else {
    while (value > 0 && i > 0) {
      const uint64_t digit = value % 10;
      buf[--i] = static_cast<char>('0' + digit);
      value /= 10;
    }
  }
  put(&buf[i]);
}

void put_hex(uintptr_t value) {
  static const char* digits = "0123456789abcdef";
  char buf[2 + sizeof(uintptr_t) * 2 + 1];
  int i = static_cast<int>(sizeof(buf)) - 1;
  buf[i] = '\0';
  if (value == 0) {
    buf[--i] = '0';
  } else {
    while (value > 0 && i > 2) {
      buf[--i] = digits[value & 0xF];
      value >>= 4;
    }
  }
  put("0x");
  put(&buf[i]);
}

const char* signal_name(int sig) {
  switch (sig) {
    case SIGSEGV: return "SIGSEGV";
    case SIGBUS: return "SIGBUS";
    case SIGILL: return "SIGILL";
    case SIGFPE: return "SIGFPE";
    case SIGABRT: return "SIGABRT";
    default: return "signal";
  }
}

struct FrameWalk {
  int depth;
};

_Unwind_Reason_Code on_frame(_Unwind_Context* ctx, void* arg) {
  auto* walk = static_cast<FrameWalk*>(arg);
  if (walk->depth >= kMaxFrames) return _URC_END_OF_STACK;
  const uintptr_t pc = static_cast<uintptr_t>(_Unwind_GetIP(ctx));
  if (pc != 0) {
    put("  #");
    put_uint(static_cast<uint64_t>(walk->depth));
    put("  ");
    put_hex(pc);
    put("\n");
    ++walk->depth;
  }
  return _URC_NO_REASON;
}

void crash_handler(int sig, siginfo_t* info, void* /*context*/) {
  if (g_active) {
    // A second fault inside the handler: stop trying and let the system take it.
    ::signal(sig, SIG_DFL);
    ::raise(sig);
    return;
  }
  g_active = 1;

  put("\n--- Motion Studio native crash ---\n");
  put("signal: ");
  put(signal_name(sig));
  put(" (");
  put_uint(static_cast<uint64_t>(sig));
  put(")\n");
  if ((sig == SIGSEGV || sig == SIGBUS) && info != nullptr) {
    put("fault address: ");
    put_hex(reinterpret_cast<uintptr_t>(info->si_addr));
    put("\n");
  }
  put("backtrace (return addresses):\n");
  FrameWalk walk{0};
  _Unwind_Backtrace(on_frame, &walk);
  if (walk.depth == 0) put("  (no frames)\n");
  put("--- end native crash ---\n");

  // Behave like a normal crash from here: the system writes its own tombstone
  // and the platform restart/ANR machinery still sees it.
  ::signal(sig, SIG_DFL);
  ::raise(sig);
}

} // namespace

extern "C" JNIEXPORT jboolean JNICALL
Java_com_motionstudio_app_native_NativeEngine_nativeInstallCrashHandler(JNIEnv* env, jobject,
                                                                       jstring dir) {
  if (dir == nullptr) return JNI_FALSE;
  const char* path = env->GetStringUTFChars(dir, nullptr);
  if (path == nullptr) return JNI_FALSE;

  char full[512];
  std::snprintf(full, sizeof(full), "%s/native-crash.txt", path);
  env->ReleaseStringUTFChars(dir, path);

  const int fd = ::open(full, O_WRONLY | O_CREAT | O_APPEND, 0600);
  if (fd < 0) {
    __android_log_print(ANDROID_LOG_WARN, "MotionStudio", "crash handler: cannot open %s", full);
    return JNI_FALSE;
  }
  g_fd = fd;

  stack_t alt{};
  alt.ss_sp = g_alt_stack;
  alt.ss_size = sizeof(g_alt_stack);
  alt.ss_flags = 0;
  if (::sigaltstack(&alt, nullptr) != 0) {
    __android_log_print(ANDROID_LOG_WARN, "MotionStudio", "crash handler: sigaltstack failed");
  }

  struct sigaction action{};
  std::memset(&action, 0, sizeof(action));
  action.sa_sigaction = crash_handler;
  action.sa_flags = SA_SIGINFO | SA_ONSTACK;
  ::sigemptyset(&action.sa_mask);

  const int signals[] = {SIGSEGV, SIGBUS, SIGILL, SIGFPE, SIGABRT};
  int installed = 0;
  for (int sig : signals) {
    if (::sigaction(sig, &action, nullptr) == 0) ++installed;
  }
  __android_log_print(ANDROID_LOG_INFO, "MotionStudio", "crash handler installed on %d signals",
                      installed);
  return installed > 0 ? JNI_TRUE : JNI_FALSE;
}

#else

extern "C" JNIEXPORT jboolean JNICALL
Java_com_motionstudio_app_native_NativeEngine_nativeInstallCrashHandler(JNIEnv*, jobject, jstring) {
  return JNI_FALSE;
}

#endif
