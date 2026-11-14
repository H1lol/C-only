# Motion Studio desktop reference alignment pass — 2026-11-04

The desktop editor has been refined against the supplied 1648×923 reference image.

## Applied source changes
- The Flutter app now loads the exact uploaded `ic_motion_studio_logo(3).png` bytes at `flutter_ui/assets/branding/ic_motion_studio_logo.png`; the top-level branding asset is identical.
- Desktop workspace order follows the reference: Edit, Animation, Compositing, Nodes, 3D, Color, Audio, Export.
- At widths >=1500 px, the desktop layout uses an 80 px tool rail, 352 px Project panel, 390 px Inspector and 390 px Graph Editor dock. Smaller desktop windows receive reduced dock widths.
- Desktop top bar is 76 px, composition header is 48 px, transport strip is 62 px, and Render Queue / Console footer is 56 px.
- Inspector / Effects / Details tabs are interactive and display distinct panels.
- The phone layout remains a separate responsive layout.

## Explicit limitations
This remains a UI/source pass, not a claim of pixel-perfect runtime equivalence. The preview composition, project-media thumbnails, graph curve and some editor properties remain illustrative or not yet connected to their native engine data. The environment does not contain the Flutter or Dart SDK, so no Flutter compile or runtime screenshot could be executed here.
