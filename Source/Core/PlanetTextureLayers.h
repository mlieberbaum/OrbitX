#pragma once
#include "PlanetQuadtree.h"
#include <array>
#include <optional>

namespace OrbitX::Core {
struct PlanetTextureLayer {
    TileId source;
    std::array<float,4> uv{};
    bool operator==(const PlanetTextureLayer&) const = default;
};

// Draw the selected leaf's geometry with this ancestor's geographic UV range.
inline std::array<float,4> PlanetLayerUv(TileId leaf,TileId source) {
    const auto b=Bounds(leaf),s=Bounds(source);
    return {float((b.west-s.west)/(s.east-s.west)),float((s.north-b.north)/(s.north-s.south)),
            float((b.east-b.west)/(s.east-s.west)),float((b.north-b.south)/(s.north-s.south))};
}

// lookup(id): nullopt = missing, failed, or not ready; true = may have alpha;
// false = known opaque. Return opaque base first, finest available detail last.
// A transparent layer must never become the opaque base just because another
// ancestor is still streaming. The caller retains the resident opaque roots.
template<class Lookup>
std::vector<PlanetTextureLayer> PlanetTextureLayers(TileId leaf,Lookup lookup) {
    std::vector<PlanetTextureLayer> layers;
    for(auto source=leaf;;source=source.Parent()) {
        if(auto alpha=lookup(source)) {
            layers.push_back({source,PlanetLayerUv(leaf,source)});
            if(!*alpha) {std::reverse(layers.begin(),layers.end());return layers;}
        }
        if(source.level<=4)return {}; // No trustworthy opaque coverage.
    }
}
}
