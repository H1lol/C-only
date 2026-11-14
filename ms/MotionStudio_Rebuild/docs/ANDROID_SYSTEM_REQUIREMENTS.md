# Motion Studio Android — Minimum System Requirements

## Minimum supported target

| Component | Minimum target |
|---|---|
| Android version | Android 10 (API level 29) or newer |
| System memory | 4 GB RAM |
| Free internal storage | 4 GB available before starting a project |
| CPU | 64-bit Android device recommended; multi-core processor strongly preferred |
| Display | Touchscreen; 360 dp logical width is the practical phone-layout target |
| Graphics | Android-supported GPU acceleration; 3D and effects performance depends heavily on GPU capability |

The Android Gradle `minSdk` is set to 29 so the application does not advertise support below Android 10.

## Recommended for a smoother editing experience

- Android 12 or newer.
- 6–8 GB RAM or more, especially for large compositions, multiple video layers, high frame rates or 3D scenes.
- At least 8 GB free internal storage; more is strongly recommended when working with long or high-bitrate media.
- A recent 64-bit multi-core processor and a modern GPU.
- Keep source footage and exports on fast storage where possible.

These are Motion Studio's target device guidelines, not measured performance guarantees. Real playback speed and render times depend on codec, resolution, frame rate, effects, composition complexity, thermals and device GPU/CPU.

## Storage guidance

The 4 GB free-space minimum is a starting baseline, not a promise that a 4 GB project or export will fit. Source media, generated previews, waveform/cache data, recovery files and exports all consume additional space. Leave extra storage available and move or remove old exports and caches when required.

## Autosave and recovery

- Autosave runs every **10 minutes** while there are unsaved editor changes.
- Motion Studio also attempts a recovery snapshot when Android moves the app to the background or the editor is closing.
- Recovery data is stored in app-private storage and is not intended to replace a user-controlled backup.
- When a project is opened, a recovery snapshot is restored if it is newer than the last manually saved project file.
- Autosave can still lose the most recent edits if Android or the device loses power before a save completes; users should use manual Save before major changes or long render sessions.

The current autosave serializes the Flutter-side editor document. Full media payloads, all effect parameter state, and complete C++ engine/session serialization still need to be unified through the native project bridge.
