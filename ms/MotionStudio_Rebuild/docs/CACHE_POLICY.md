# Motion Studio cache policy

- Cache budget: 1 GiB by default.
- Cleanup threshold: strictly greater than 75% of that budget (768 MiB).
- Checks: once at app startup and every five minutes while running.
- Cache root: the app's operating-system temporary directory under `MotionStudioCache`.
- Cleanup scope: only the contents of that dedicated generated-cache directory.

The cache manager does not traverse or delete the application-support project store.
Projects, `.msproj` files, autosaves, recovery snapshots, original/imported media,
and exported videos must remain outside the generated-cache directory.

If a future feature writes disposable previews or thumbnails, it should place them
under `MotionStudioCache`. User-authored or irreplaceable data must never be placed
there.
