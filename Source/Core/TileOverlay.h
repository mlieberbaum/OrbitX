#pragma once
#include "PlanetQuadtree.h"
#include <array>
#include <iomanip>
#include <locale>
#include <sstream>
namespace OrbitX::Core {
inline constexpr unsigned NextTileOverlayMode(unsigned mode){return mode%3+1;}
struct TileEdgeCoordinate {
    double latitude{},longitude{};
    bool isLatitude=false;
    double Value()const{return isLatitude?latitude:longitude;}
};
// Both polar bands retain tint/LOD numbers but contribute no coordinate labels.
// Interior rows label each edge midpoint.
inline std::array<TileEdgeCoordinate,4> TileEdgeCoordinates(TileId id){
    const auto b=Bounds(id);const double lat=(b.north+b.south)*.5,lon=(b.west+b.east)*.5;
    return {{{b.north,lon,true},{b.south,lon,true},{lat,b.west,false},{lat,b.east,false}}};
}
inline bool HasTileEdgeCoordinates(TileId id){return id.row>0&&id.row<(1<<(id.level-4))-1;}
inline std::wstring TileCoordinateText(const TileEdgeCoordinate& edge){
    std::wostringstream out;out.imbue(std::locale::classic());
    const double value=edge.Value();const wchar_t hemisphere=edge.isLatitude?(value<0?L'S':L'N'):(value<0?L'W':L'E');
    out<<std::fixed<<std::setprecision(2)<<std::abs(value)<<L"\u00b0 "<<hemisphere;return out.str();
}
}
