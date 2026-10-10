# OrbitX

## Moon quadtree prototype

Open `Source/OrbitX.sln` in Visual Studio, select **Release_Planet | x64**, and press **F5**.
The existing v145 toolset and Windows SDK requirements apply. `Release_Planet` defines
`NDEBUG`, `NOMINMAX`, and `PLANET_DEBUG_MODE`; planetary textures are fully illuminated.
Release and Release_Planet use `/O2`, whole-program/link-time optimization and optimized
shaders. Ordinary Debug retains debug libraries and the graphics debug layer.
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
interval. It also appears in menus. This reflects render-loop throughput,
including presentation pacing. Graphics provides persistent **VSync OFF / ON**;
OFF is the default and uses Present(0) with tearing support where available.
There is no application FPS cap. ON synchronizes presentation to the display.

Launch **Moon View**, then use:

| Control | Action |
| --- | --- |
| Ctrl+I | Toggle thin tile boundaries (default off) |
| Ctrl+O | Independently toggle 16% tile tint (default off) |
| Ctrl+L | Freeze/unfreeze selected leaf set; camera still moves |
| Left drag / wheel | Existing camera rotation / zoom |

In Release_Planet, diagnostics appear below the existing upper-right text. They
separate highest rendered **texture source** level from highest resident DDS
level, show drawn geometry leaves, resident/pending/failed textures, and actual
DDS GPU allocation size. Timings separate selection, upload CPU work, background
DDS reads, scene GPU execution, and presentation/frame-buffer waits. These are
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

The DDS cache has a 512 MiB allocation budget and 2,048 descriptor slots; currently
drawn textures are pinned and other textures are evicted by last use. Mesh and transient uploads are excluded from the reported DDS allocation
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

The portable test executable in `Source/Tests/PlanetSurfaceTests.cpp` checks
Orbiter indexing, sparse hierarchy discovery, selection/freeze, BC mip parsing,
malformed files, independent absolute/relative surface configuration, right-handed
axes/camera/winding, background loader results/errors and saved VSync preferences. It can be built separately with any C++20 compiler; it is
not part of the application project. It was run with AddressSanitizer and
UndefinedBehaviorSanitizer (leak tracing disabled for this container).
Visual Studio compilation, HLSL compilation, and live DX12 rendering must still
be checked on Windows with the actual DDS database.

Suggested Windows acceptance check: build all three configurations; launch Moon
View in Release_Planet; verify both hemispheres and poles, zoom through LOD changes,
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
