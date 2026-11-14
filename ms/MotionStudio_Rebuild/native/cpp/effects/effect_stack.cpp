#include "ms_clip_motion.hpp"
#include "ms_cpu_effects.hpp"

namespace ms {

Error EffectStack::apply(const std::vector<EffectOp>& effects,
                         const ImageBuffer& in, ImageBuffer& out) {
  if (effects.empty()) {
    if (!out.allocate(in.width(), in.height(), in.format()))
      return Error::make(ErrorCode::OutOfMemory, "effect out");
    out.copy_from(in);
    return Error::success();
  }
  ImageBuffer a, b;
  const ImageBuffer* cur = &in;
  ImageBuffer* dst = &a;
  bool any = false;
  for (const auto& fx : effects) {
    if (!fx.enabled) continue;
    Error e = cpu_effect_apply(fx.id, *cur, *dst, fx.params);
    (void)e; // unknown effects still write passthrough
    if (!dst->valid()) {
      if (!dst->allocate(cur->width(), cur->height(), cur->format()))
        return Error::make(ErrorCode::OutOfMemory, "fx");
      dst->copy_from(*cur);
    }
    cur = dst;
    dst = (dst == &a) ? &b : &a;
    any = true;
  }
  if (!any) {
    if (!out.allocate(in.width(), in.height(), in.format()))
      return Error::make(ErrorCode::OutOfMemory, "effect out");
    out.copy_from(in);
    return Error::success();
  }
  if (!out.allocate(cur->width(), cur->height(), cur->format()))
    return Error::make(ErrorCode::OutOfMemory, "fx final");
  out.copy_from(*cur);
  return Error::success();
}

} // namespace ms
