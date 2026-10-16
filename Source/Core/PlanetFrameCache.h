#pragma once
#include "PlanetQuadtree.h"
#include <cstddef>
#include <cstdint>
namespace OrbitX::Core {
struct PlanetFrameKey {
    float yaw{},pitch{},distanceKm{},fov{};
    unsigned width{},height{};
    bool frozen=false;
    size_t tiles{},files{};
    uint64_t uploadFence{};
    bool operator==(const PlanetFrameKey&)const=default;
};
// Only a completely settled frame may be remembered. Camera, viewport, freeze,
// residency and external leaf changes all force the original selection path.
class PlanetFrameCache {
public:
    bool Reusable(const PlanetFrameKey& key,const std::vector<TileId>& leaves,bool busy)const{
        return valid&&!busy&&key==previous&&leaves==previousLeaves;
    }
    void Remember(const PlanetFrameKey& key,const std::vector<TileId>& leaves){previous=key;previousLeaves=leaves;valid=true;}
    void Invalidate(){valid=false;}
private:
    bool valid=false;PlanetFrameKey previous;std::vector<TileId> previousLeaves;
};
}
