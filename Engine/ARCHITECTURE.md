# AtomEngine architecture and refactoring status

## Responsibilities

- `EngineCore` orchestrates window/input/audio services, the DX12 device, assets, and renderer initialization. GPU work is drained before application shutdown. Renderer passes and assets are released before device infrastructure.
- `WindowManager` creates the Win32 window and dispatches optional resize and message callbacks. It does not initialize the graphics device or include editor code.
- `EditorApp` owns ImGui and registers rendering/message callbacks for its lifetime. Runtime does not include Editor headers.
- `World` owns EnTT entities. `GameObject` is a caller-owned wrapper, not an entity owner. Wrappers check entity generations and a weak World lifetime token. Destroying a wrapper does not destroy its entity.
- Names are display labels and can be duplicated. `FindByName` returns one matching entity; persistent identity must not depend on a name. Structural registry access is read-only; mutation goes through World APIs.
- `AnimationPlayer` owns per-instance playback and skeleton pose. Cached model skeletons are not modified by playback. MeshComponent currently retains transform evaluation and GPU instance buffers.
- `ModelLoader` imports CPU model data. `ModelGpuBuilder` in the rendering layer builds buffers, material descriptor tables, and pipelines. AssetManager receives that builder through registration rather than implementing renderer policy.
- `TextureRef` uses shared ownership. Synchronous texture loading is serialized; decode failure returns an invalid texture with a fallback descriptor and never enters a busy wait. The cache retains textures until eviction or shutdown.
- GPU resources and shader-visible descriptor ranges are retired with graphics, compute, and copy fences. Freed ranges are coalesced and reused only after fence completion.
- `RenderView` carries the World, camera, targets, viewport, and scissor for render submission. The current effects still use primary-view global buffers.

## API changes

- `World::GetGameObject` returns an owning wrapper (`unique_ptr<GameObject>`) instead of a pointer to somebody else's wrapper.
- `World::GetRegistry` is read-only. Use `World::View`, component operations, and entity operations instead of changing registry structure directly.
- AssetManager must be given a `ModelBuilder` before model loading; EngineCore performs this registration.
- The application has a `BeginFrame` hook. EditorApp uses it for ImGui; applications without editor UI do not require an editor object.
- Successful process exit now returns zero. `--smoke-test` exercises runtime resource regressions and exits normally after 120 frames.

## Verified behavior

`Tests/run-framework-tests.cmd` checks scene push/pop/replace, pushes into an empty scene stack, component callbacks, unnamed entities, wrapper destruction, duplicate names, entity reuse, cross-World equality, and World lifetime invalidation.

`--smoke-test` checks repeated non-DDS decode failure, texture copy/move ownership, and descriptor reuse after fence completion, then renders 120 frames and shuts down normally. FeatureSmokeChecks additionally verifies XML scenes/prefabs, C# and Jolt integration, shader reload transactions, picking and rendered UI pixels.

Build logs are stored under `generated/refactor-debug.log` and `generated/refactor-release.log`.

## Remaining architectural work

This refactoring does not implement the entire long-term architecture discussed in the review:

- Runtime, Core, DX12, and Editor still share one Visual Studio executable project. Build-target separation is pending.
- Systems still include singleton/static services; engine orchestration has been centralized, but instance-owned services and complete initialization-failure rollback are pending.
- MeshComponent still owns GPU instance buffers. Animation sampling is separated, but full render-proxy extraction is pending.
- RenderView is a submission boundary, not independent multi-view rendering. SSAO/motion-blur resources, particles, environment settings, and intermediate buffers still need per-view ownership. A Render Graph is not implemented.
- Resource loading is synchronous and serialized. Per-asset concurrent loading, offline asset cooking, stable asset IDs, and hot reload are pending.
- Shader-visible descriptor recycling is implemented; the separate CPU descriptor allocator remains monotonic.
- GPU retirement assumes resources are destroyed after command lists referencing them have been submitted. Concurrent recording/destruction requires an explicit submission-owned retention mechanism.
- Existing frame presentation still waits for GPU completion. Removing this wait requires frame-buffered upload storage and explicit frames-in-flight synchronization.
- Scene persistence, prefab snapshots, inspector editing, C# scripting and Jolt physics are implemented; editor undo/redo, prefab override propagation and advanced UI layout remain pending. See FEATURES.md for current APIs and limits.

These remaining changes require separate behavioral validation; successful builds and the current regressions do not establish their completion.

## Anti-aliasing

Temporal anti-aliasing has been removed, including its history buffers, shaders, UI, and viewport jitter. No replacement anti-aliasing method is currently enabled. Linear depth and camera reprojection remain available for SSAO and motion blur.
