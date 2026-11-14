# Motion Studio project save and recovery

## Save locations

Motion Studio stores project files and recovery snapshots under the app's private application-support directory:

- `MotionStudio/Projects/<project-key>.msproj` — manual save.
- `MotionStudio/Recovery/<project-key>.autosave.msproj` — timed recovery snapshot.

Writes use a temporary file, flush it to disk, retain a `.bak` copy of the previous destination where applicable, and then rename the temporary file into place.

## Timing

- The editor checks for dirty state every 10 minutes.
- Clean projects are not rewritten.
- The app attempts an autosave on inactive, paused or detached lifecycle events and when the editor is disposed.
- A user can turn autosave off in Settings. Manual Save remains available from the editor's More menu.

## Recovery selection

The starter panel stores its recent and archived project lists locally using SharedPreferences. Newly created projects are inserted into Recents before the editor is opened, so the project can still be found after a process restart and its autosave can be recovered.

On opening a project, Motion Studio compares the `savedAt` timestamp of the manual project and the recovery file. It restores the newer valid document. Corrupt JSON recovery files are ignored so they do not block opening a valid manual save.

## Serialized editor state

Schema version 1 includes the project setup, layer name/type/colour/visibility/lock state/timing/keyframe count, applied effect names, playhead position, selected workspace and layer, timeline zoom, transform values, snap/grid/guides/loop state and inspector tab.

Media file payloads and external references, complete effect parameters, graph-node parameter values, and live C++ engine state are not yet included in this Flutter-side document. The autosave implementation is a recovery layer for the existing editor state, not a claim that every subsystem has full project round-trip support.
