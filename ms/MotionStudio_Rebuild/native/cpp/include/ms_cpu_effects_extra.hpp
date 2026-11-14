#pragma once
#include "ms_cpu_effects.hpp"
namespace ms {
Error cpu_invert(const ImageBuffer& in, ImageBuffer& out);
Error cpu_threshold(const ImageBuffer& in, ImageBuffer& out, float level);
Error cpu_chromakey_simple(const ImageBuffer& in, ImageBuffer& out,
                           float kr, float kg, float kb, float tol);
}
