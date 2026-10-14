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
- `RenderView` carries the World, camera, targets, viewport, and scissor for render submission. Effects use isolated per-view resources; compatibility global names route to the currently scoped view.

## API changes

- `World::GetGameObject` returns an owning wrapper (`unique_ptr<GameObject>`) instead of a pointer to somebody else's wrapper.
- `World::GetRegistry` is read-only. Use `World::View`, component operations, and entity operations instead of changing registry structure directly.
- AssetManager must be given a `ModelBuilder` before model loading; EngineCore performs this registration.
- The application has a `BeginFrame` hook. EditorApp uses it for ImGui; applications without editor UI do not require an editor object.
- Successful process exit now returns zero. `--smoke-test` exercises runtime resource regressions and exits normally after 120 frames.

## Verified behavior

`Tests/run-framework-tests.cmd` checks scene push/pop/replace, pushes into an empty scene stack, component callbacks, unnamed entities, wrapper destruction, duplicate names, entity reuse, cross-World equality, and World lifetime invalidation.

`--smoke-test` checks repeated non-DDS decode failure, texture copy/move ownership, and descriptor reuse after fence completion, then renders 120 frames and shuts down normally. FeatureSmokeChecks additionally verifies XML scenes/prefabs, C# and Jolt integration, shader reload transactions, picking and rendered UI pixels.

Build logs and validation artifacts are stored under `generated/`; they are regenerable and may be cleaned.

## Current implementation boundaries

Detailed capability and usage status lives in [FEATURES.md](FEATURES.md); this document describes responsibility boundaries only.

- AtomEngine builds the editor; AtomGame imports the shared runtime source list but excludes editor/ImGui code and uses a standalone game entry point. Build outputs and tracking are isolated.
- Singleton/static services remain; complete initialization-failure rollback needs further work.
- MeshComponent still owns GPU instance buffers; complete render-proxy extraction is pending.
- RenderGraph validates imported-resource dependencies and inserts barriers; no transient resource pool or memory aliasing.
- Per-view render targets, effect history and passes are isolated, with serial submission on one render thread.
- Background CPU asset preparation exists; GPU upload and some internal resource dependencies can still block the engine thread.
- Scene/prefab persistence, undo/redo, prefab propagation/overrides, scene camera/environment persistence and managed hot reload are implemented. See FEATURES.md for limitations.
- FXAA is enabled by default; temporal anti-aliasing is removed.

These are code/document inspection conclusions, not a claim that all regressions were rerun on 2026-10-14.

## Project and publishing responsibilities

`GameProject` validates and atomically saves editor-only .atproj definitions; `ProjectPanel` handles editing and build actions. `ProjectPublisher` collects runtime dependencies, writes an AES-256-GCM encrypted asset archive, embeds its key/digest in the game build, and replaces complete output directories, with the PowerShell entry in Tools. `AssetArchive` mounts and authenticates the archive before renderer initialization; resource readers decrypt in memory. `GameLaunchSettings` reads only the runtime scene catalog, window parameters and rendering defaults from the archive. Published games have no .atproj dependency or loose Asset directory. Scene loading stages a World before committing; the engine resets scripts/physics/audio only at that commit boundary. Failure retains the old World and script instances. See [PROJECT_WORKFLOW.md](PROJECT_WORKFLOW.md).
