# OrbitX

## Moon quadtree prototype

Open `Source/OrbitX.sln` in Visual Studio, select **Env_Test_Release | x64**, and press **F5**.
The existing v145 toolset and Windows SDK requirements apply. `Env_Test_Release` defines
`NDEBUG`, `NOMINMAX`, and `PLANET_DEBUG_MODE`; planetary textures are fully illuminated.
Release and Env_Test_Release use `/O2`, whole-program/link-time optimization and optimized
shaders. `Env_Test_Debug` provides the same fully illuminated environmental view and
diagnostics with `_DEBUG`, disabled optimization, debug runtime libraries and the
graphics debug layer. Ordinary Debug retains debug libraries and the graphics debug layer.
Standard Debug/Release retain directional Sun lighting.

`Runtime/Config/OrbitX.cfg` supplies the shared `TextureRoot`. The Moon DDS
hierarchy has its own setting in `Runtime/Config/Bodies/Moon.cfg`:

```ini
SurfaceTiles = D:\OrbitX\Textures\Moon\Surf
```

An absolute value is independent of TextureRoot; a relative value is resolved
against TextureRoot. If omitted, the default is `TextureRoot/Moon/Surf`. Preserve
Orbiter's `04/000000/000000.dds` level/row/column hierarchy. Startup requires both
`04/000000/000000.dds` and `04/000000/000001.dds`, preloads them, and pins them for
DDS-only ancestor fallback. Missing folders/roots or unreadable roots produce a
startup error. **Moon.jpg is neither loaded nor required**, including for the
scenario thumbnail; optional menu artwork is independent. No imagery belongs in
Git. Older global directories 01–03 are not used by this quadtree.

FPS is displayed below TIME 1X in every build, with a half-second averaging
interval. It is visible only in the active simulation, never in menus or submenus. This reflects render-loop throughput,
including presentation pacing. Graphics provides persistent **VSync OFF / ON**;
OFF is the default and uses Present(0) with tearing support where available.
There is no application FPS cap. ON synchronizes presentation to the display.

Launch **Moon View**, then use:

| Control | Action |
| --- | --- |
| Ctrl+O | Toggle 16% tile tint, thin boundaries, and colored center LOD numbers together (default off) |
| Ctrl+L | Freeze/unfreeze selected leaf set; camera still moves |
| Right drag / wheel | Camera rotation / Orbiter planetary wheel zoom |
| Ctrl+F | Toggle the transparent FPS history graph (default on) |

Ctrl+I has no action. Center numbers identify each selected patch's geometry LOD
and use the same color as its tint and boundaries. Centers behind the lunar limb
or outside the viewport are not labeled; the HUD reports texture-source LOD separately.

In Env_Test_Release, diagnostics appear below the existing upper-right text. They
separate highest rendered **texture source** level from highest resident DDS
level, show drawn geometry leaves, resident/pending/failed textures, and actual
DDS GPU allocation size. Timings separate selection, upload CPU work, background
DDS reads, scene CPU/GPU execution, HUD CPU submission, and presentation/frame-buffer waits. These are
smoothed measurements; GPU scene time excludes the Direct2D HUD. With either overlay enabled, center inspection shows
selected geometry level/indices, geographic bounds/center, source level, and
actual resolved DDS path. Boundary colors
follow geometry leaf level, even when its image comes from a coarser ancestor.
The eight approved colors repeat; the HUD provides exact levels.

Freeze fixes geometry selection, preserving all textures referenced by those
leaves. Outstanding ancestor/leaf loads can finish and replace fallback imagery
while frozen. Moving the camera can expose regions outside the frozen selection;
unfreeze to select them. Border lines are computed on real tile UV edges in the
surface pixel shader, avoiding extra geometry and depth fighting. Skirts cover
mixed tessellation cracks.

### Prototype limits and validation

This milestone draws spherical terrain only, with the existing fixed Moon radius
and orientation. The body-independent selector accepts radius/camera parameters;
the Moon renderer is its first integration. All body geometry, surface mapping, camera vectors and lighting use right-handed
coordinates. Planet-fixed +X is the equator at longitude 0°, +Y the equator at
90° east, and +Z the north pole. Longitude increases east and texture rows
increase south. Analytic camera up permits continuous pole crossings. This
prototype retains the configured identity body-to-ICRF orientation; an actual
lunar attitude model is a separate milestone.

Surface folder metadata is indexed at startup, without reading tile payloads.
Only the two mandatory root DDS textures are loaded before the menu appears.
OrbitX currently reads an extracted Surf directory, not moon.tree archives. The former 8192-pixel JPG resampling cap has been removed. DDS tiles
retain their native dimensions/mips, up to 16384 pixels per axis per resource;
the combined quadtree surface is not limited to that size. BC1, BC2, BC3 and supported DX10 BC7 DDS mip chains are uploaded as-is.
Malformed/unsupported detail tiles log once and retain DDS ancestor fallback.

The DDS cache has a 1 GiB allocation budget and 8,192 tile descriptor slots; currently
drawn textures and current selection dependencies (ancestors and required siblings)
are protected. Other textures are evicted by last use, subject to GPU fence safety.
When no safe victim exists, uploads defer rather than evict required coverage. Mesh and transient uploads are excluded from the reported DDS allocation
size. The selector uses projected tile size, conservative horizon culling and
camera-frustum sphere tests. Drawn leaf count means patches
submitted to DX12, including patches later clipped by the viewport.

A CPU worker reads/validates DDS files, prioritizing coarse coverage and then the
view center. Obsolete queued work is canceled. Completed read-ahead is bounded
to 32 results or 64 MiB plus the final tile. A dedicated upload queue uses three
fenced, reusable packets; the render thread submits up to 16 results per frame
with a 2 ms CPU target (one individual allocation/copy can exceed it). It never
waits for a tile upload to complete. Textures become eligible for drawing only
after their upload fence completes. Descriptor eviction waits for the last
render fence using that texture. Standard frame-buffer reuse can still wait
for rendering, independently of tile uploads.

Parents remain visible until available child DDS coverage is ready. Four
immutable GPU-local parameter grids are shared across patches; texture arrivals
change bindings/UV ranges rather than rebuild geometry. Skirts hide tessellation
seams. Fallback highlighting and elevation remain deferred.

Quadtree selection, request scheduling, upload completion, texture residency,
ancestor fallback, cache eviction and drawing use the implementation from
`e21521a` directly. Selection and draw bindings are refreshed every frame;
upload completion scans all resident texture entries. The subsequent quadtree
caching and upload-packet tracking optimizations have been removed.
The HUD still reuses DirectWrite layouts and formats diagnostic text at 10 Hz;
scene constants are written once per rendered frame.

The transparent lower-center FPS graph retains up to 12 seconds of history,
with 100 ms frame-throughput samples, a dynamic FPS scale and cached line
geometry. Ctrl+F hides it while sampling continues in the simulation. Returning
to menus clears history; no FPS text or graph is drawn in menus. Compare FPS
with the graph both on and off to measure its cost.

Wheel zoom follows Orbiter's external planetary camera equations, including
altitude scaling, the `1e-4` floor, the inverse target-radius floor (metres),
reciprocal inward zoom, surface-distance floor, full signed wheel delta and
Ctrl-wheel suppression.
The former 1800 km lower clamp and 500000 km upper clamp are removed. Float
storage rounds the surface floor upward by less than one float spacing; the
near clipping plane adapts to altitude. Horizontal dragging reverses its yaw
sign; drag sensitivity and vertical behavior remain unchanged.

The portable test executable in `Source/Tests/PlanetSurfaceTests.cpp` checks
Orbiter indexing, sparse hierarchy discovery, selection/freeze, BC mip parsing,
malformed files, independent absolute/relative surface configuration, right-handed
axes/camera/winding, background loader results/errors saved VSync preferences, signed wheel zoom at multiple altitudes and a bounded FPS history. It can be built separately with any C++20 compiler; it is
not part of the application project. It was run with AddressSanitizer and
UndefinedBehaviorSanitizer (leak tracing disabled for this container).
Visual Studio compilation, HLSL compilation, and live DX12 rendering must still
be checked on Windows with the actual DDS database.

Suggested Windows acceptance check: build all three configurations; launch Moon
View in Env_Test_Release; verify both hemispheres and poles, zoom through LOD changes,
exercise each toggle independently, freeze and rotate, and inspect paths. Verify
Sinus Iridum orientation, north-up/east-right at the initial camera, seamless
pole crossings and drag responsiveness while tiles are pending. Toggle VSync
in Graphics, restart to check persistence, and compare timings/FPS with it off. Check
`Runtime/Logs/OrbitX.log` for DDS failures or debug-layer errors. Debug/Release
should retain the existing terminator.

Orbiter reference: `orbitersim/orbiter`, `OVP/D3D9Client/Tilemgr2.cpp` (`Extents`)
and `OVP/D3D9Client/Surfmgr2.cpp` (`SurfTile::PreLoad`): internal level zero maps to
directory 04, with one latitude band and two longitude bands; row zero is north
and longitude starts at −180°.

Streaming architecture reference: [mschweiger/D3D9Client](https://github.com/mschweiger/D3D9Client),
revision `9043da1c2f0cf75827064c04242d096a68d2c8fb`: `Orbitersdk/D3D9Client/Tilemgr2.cpp`
and `Tilemgr2_imp.hpp` (background preload, parent coverage, visibility and tile
lifetime), `Surfmgr2.cpp` (surface patches), and `ZTreeMgr.cpp` (archive index
versus on-demand payload reads). These principles are implemented for DX12
queues and right-handed coordinates; the Orbiter left-handed geometry and
projection conventions are not carried into OrbitX.

Camera reference: [orbitersim/orbiter Camera.cpp](https://github.com/orbitersim/orbiter/blob/3e21f908f2f07fade86ccef4915732b0b15d172b/Src/Orbiter/Camera.cpp),
`ProcessMouse`, `UpdateMouse`, `ShiftDist`, and `ChangeDist`. D3D9's
`D3D9Client.cpp::RenderWndProc` handles wheel input specially for its debug
camera; ordinary external wheel input is handled in the Orbiter core.
[D3D9Pad.cpp](https://github.com/mschweiger/D3D9Client/blob/9043da1c2f0cf75827064c04242d096a68d2c8fb/Orbitersdk/D3D9Client/D3D9Pad.cpp)
reuses font/text resources, while `Tilemgr2_imp.hpp::ProcessNode/RenderNode`
retain selection state separately from drawing. OrbitX retains its `e21521a` tile pipeline; HUD text resources are cached independently.

Performance verification: the supplied RTX 3080 Ti screenshot showed roughly
0.18 ms selection and 0.04 ms scene GPU time at about 1014 FPS. This suggests CPU
selection and HUD work are useful first targets; it is not a complete CPU/GPU
profile. Portable automated checks cover camera/history behavior, and the tile implementation
is compared directly against `e21521a`. Speedup and Windows
rendering must be measured on the user's system. A remaining large HUD CPU time
would justify investigating the Direct2D/D3D11On12 path; a large presentation
wait would justify examining DXGI presentation/frame buffering. Neither change
is assumed necessary before measuring the new counters.


## Environmental camera scripts and Full logging

Load Moon View normally, then press **Ctrl+R** to toggle the green glass panel.
The **Record** tab accepts a script name; **START RECORDING** hides the panel and
records camera states at a target **100 Hz** while the existing drag/wheel controls work.
Ctrl+R reopens it without pausing recording. **STOP & SAVE** closes the panel
and saves the camera script and diagnostic log. Scenario exit and shutdown also save and close an active recording.

The **Run** tab lists saved `.oxscript` files from **OrbitX/Runtime/Scripts**.
Click a row to select it; use the mouse wheel to scroll additional scripts.
**RUN SCRIPT** hides the panel and replays the camera path on its original
monotonic timeline. Every render frame interpolates angles along the shortest
wrapped arc and linearly interpolates distance/FOV between timestamped samples.
Actual sample endpoints are reproduced exactly. Repeated stationary samples keep
your manually recorded loading pauses. There are no checkpoints or automatic
waits for tile loading. LOD-freeze transitions are discrete commands captured at
their input timestamps. The sample target is 10 ms; if rendering falls below
100 Hz, samples use the actual available frame times rather than inventing
historical positions. Sampling is an approximation of the camera between samples,
not an identical recording of every rendered frame.

Logging options are **Benchmark**, **Full**, and **Regression**. Full is the
existing livestream diagnostic trace; Benchmark and Regression remain disabled.
Full mode leaves overlays and the FPS graph active according to user settings.
Playback temporarily owns camera/LOD controls. Ctrl+R can show or hide the panel
without pausing playback; **STOP SCRIPT** cancels and closes the panel. Completion,
cancellation, log failure and scenario exit restore the pre-run camera,
FOV, LOD-freeze state, overlays and graph. If the original LOD was frozen, its
selected leaves and dependency state are restored too. Textures remain cached.

Camera scripts are separate from logs. They contain an initial camera state,
viewport, duration, microsecond-timed samples and LOD-freeze
changes. Legacy format-1 checkpoint markers are accepted but ignored. Geographic latitude/longitude/altitude metadata accompanies exact float
orbit angles, distance and FOV, preserving existing pole-crossing orientation
without changing the camera math. Initial frozen selection and later transitions
into frozen LOD carry selected-leaf snapshots. Replays require the recorded
viewport size; resizing stops an active recording or playback. Script files are
written on stop via a temporary file and renamed on success, never overwriting
an existing recording. Names use a start-time suffix; paths and logs are ignored
by Git. Failed writes are reported. Abrupt termination can lose the in-memory
camera recording. Format validation caps input size at 64 MiB, duration at 24
hours, and samples/frozen-leaf records at one million each.

Playback uses the **existing cache**, recorded in the Full log. It reproduces
camera commands, not the asynchronous read/upload schedule. For comparable cold
runs, restart OrbitX and use the same scenario, viewport, build and texture set;
warm replay intentionally benefits from already-cached tiles. Automated
regression comparison and benchmark measurements will be added separately.

Logs are written to **OrbitX/Runtime/Logs**, using the Visual Studio Runtime working
directory, e.g. `Livestream_2026-10-10_210445_123456.log`. Filenames use recording
start time; timestamps use local system time with six fractional digits and no
timezone suffix. Every line includes a global sequence, render frame number and
monotonic elapsed microseconds, followed by one event. DDS-worker events are
serialized with render-thread events. Timestamp precision does not guarantee that
all events have distinct timestamps; the sequence number resolves ties.

The top-left HUD shows LAT, LON, ALT, DIST and FOV. ALT is camera distance minus
the current Moon sphere radius (1737.4 km), displayed in kilometres to one decimal
place; it is not yet adjusted for terrain elevation.

The format-2 trace includes initial resident/failed/queued/in-flight state,
camera and matrix changes, inputs/settings, loading/cancellation/results, upload
submission and activation, actual evictions with reasons and GPU fence values.
Draw records use `level/row/column` IDs and include actual texture sources and UV
ranges. `DRAW_PATCH` updates a patch; `DRAW_REMOVE` removes it. Request changes use
`REQUEST_ADD`/`REQUEST_REMOVE`. Apply these deltas to reconstruct each frame's
coverage against the same DDS collection. Branch state changes identify the first
missing child when coverage is retained. Each frame has compact cache/LOD/fence and
timing summaries. Unchanged traversal queries, candidate-by-candidate eviction
scans, repeated draw submissions and repeated camera/matrix snapshots are omitted.
This diagnostic log is a state/event trace, not a screenshot/video capture.
Replay uses the separate camera script, not the diagnostic log; the log does not
record every intermediate selection calculation.

Full logging intentionally records synchronously through a thread-safe buffered
stream. Draw, branch, camera and matrix states are compared numerically before
message formatting or logger locking; unchanged states skip both. Change caches
reset for each recording, and removed/reappearing patches emit fresh records.
Recording can still reduce FPS and affect asynchronous completion timing;
use it for diagnosis, not performance measurement. When inactive, log builders are
skipped before formatting/locking/I/O. Open/write/flush failures are reported;
records are never silently dropped from a bounded queue. The completed file has a
`TRACE_STOP` terminal event unless an I/O failure or abrupt process termination
prevents it. Streamed textures remain cached after stopping; playback restores the prior
camera while manual recording leaves the camera where exploration ended.

Portable verification: `Source/Tests/EnvironmentTraceTests.cpp` checks timestamp
format, event sequencing, concurrent writers, close/restart and failure recovery,
panel hit regions, and identical traced/untraced quadtree leaves and requests over
equatorial and polar views. `PlanetSurfaceTests.cpp` remains the existing baseline.
`Source/Tests/CameraScriptTests.cpp` checks exact camera round-tripping through
pole views, frozen-selection snapshots, invalid-file rejection, overwrite
protection, smooth interpolation, angular wrap and recorded pauses. Windows build, panel appearance and
real DDS streaming require local verification.

All four Visual Studio configurations enable **Multi-processor Compilation**
(`/MP`) for parallel C++ compilation. This changes build scheduling only; it does
not add game/render threads or alter runtime behavior.
