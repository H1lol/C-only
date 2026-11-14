# LMMS 1.2.2 — design reference only

The uploaded LMMS source was reviewed for audio-workstation workflow ideas (mixer channels, effect racks, automation, metering, and plugin organization). No LMMS implementation files are copied into Motion Studio because this supplied archive carries GPL-2-only licensing, which is not compatible with Motion Studio’s GPL-3-or-later license for a combined derivative.

Motion Studio’s new native master effects are implemented independently in `native/cpp/media/audio_fx.cpp`, using the Sprocket MIT-licensed audio architecture as a reference and standard DSP designs.
