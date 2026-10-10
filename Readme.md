# OrbitX

## Moon quadtree prototype

Open `Source/OrbitX.sln` in Visual Studio, select **Debug_Planet | x64**, and press **F5**.
The existing v145 toolset and Windows SDK requirements apply. `Debug_Planet` defines
`_DEBUG`, `NOMINMAX`, and `PLANET_DEBUG_MODE`; planetary textures are fully illuminated.
Standard Debug/Release retain directional Sun lighting.

`Runtime/Config/OrbitX.cfg` supplies `TextureRoot` (normally `D:\OrbitX\Textures`).
The existing `Texture` entry in `Runtime/Config/Bodies/Moon.cfg` remains the global
fallback image and scenario-preview source. Keep it available. Extracted DDS files
are discovered under `TextureRoot/Moon/Surf`, preserving Orbiter's
`04/000000/000000.dds` level/row/column hierarchy. No imagery belongs in Git.
Directories 01–03 contain older global maps; this prototype uses the configured
image as the global fallback and starts the quadtree at directory 04.

Launch **Moon View**, then use:

| Control | Action |
| --- | --- |
| Ctrl+I | Toggle thin tile boundaries (default off) |
| Ctrl+O | Independently toggle 16% tile tint (default off) |
| Ctrl+L | Freeze/unfreeze selected leaf set; camera still moves |
| Right drag / wheel | Existing camera rotation / zoom |

In Debug_Planet, diagnostics appear below the existing upper-right text. They
separate highest rendered **texture source** level from highest resident DDS
level, show drawn geometry leaves, resident/pending/failed textures, and actual
DDS GPU allocation size. With either overlay enabled, center inspection shows
selected geometry level/indices, geographic bounds/center, source level, and
actual resolved path. Level 0 means the configured global image. Boundary colors
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
the Moon renderer is its first integration. Surface folder metadata is indexed
at startup. BC1, BC2, BC3 and supported DX10 BC7 DDS mip chains are uploaded as-is.
Malformed/unsupported tiles log once and retain ancestor/global fallback.

The DDS cache has a 512 MiB allocation budget and 2,048 descriptor slots; currently
drawn textures are pinned and other textures are evicted by last use. Global
texture, mesh, and transient uploads are excluded from the reported DDS allocation
size. The selector uses projected tile size and conservative horizon culling;
it does not yet cull against the camera frustum. Drawn leaf count means patches
submitted to DX12, including patches later clipped by the viewport.

Loading is deliberately bounded to four synchronous DDS load attempts per
frame. Queue fences protect resource/descriptor reuse, so initial loading and LOD
changes can stall. Asynchronous streaming is future work. Fallback highlighting
and elevation are deferred.

The portable test executable in `Source/Tests/PlanetSurfaceTests.cpp` checks
Orbiter indexing, sparse hierarchy discovery, selection/freeze, BC mip parsing,
and malformed files. It can be built separately with any C++20 compiler; it is
not part of the application project. It was run with AddressSanitizer and
UndefinedBehaviorSanitizer (leak tracing disabled for this container).
Visual Studio compilation, HLSL compilation, and live DX12 rendering must still
be checked on Windows with the actual DDS database.

Suggested Windows acceptance check: build all three configurations; launch Moon
View in Debug_Planet; verify both hemispheres and poles, zoom through LOD changes,
exercise each toggle independently, freeze and rotate, and inspect paths. Check
`Runtime/Logs/OrbitX.log` for DDS failures or debug-layer errors. Debug/Release
should retain the existing terminator.

Orbiter reference: `orbitersim/orbiter`, `OVP/D3D9Client/Tilemgr2.cpp` (`Extents`)
and `OVP/D3D9Client/Surfmgr2.cpp` (`SurfTile::PreLoad`): internal level zero maps to
directory 04, with one latitude band and two longitude bands; row zero is north
and longitude starts at −180°.
