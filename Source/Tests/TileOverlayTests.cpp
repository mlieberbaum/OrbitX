#include "../Core/TileOverlay.h"
#include "../Core/ScriptPanelLayout.h"
#include <cassert>
#include <iostream>
using namespace OrbitX::Core;
int main(){
    unsigned mode=1;for(unsigned expected:{2u,3u,1u,2u,3u,1u}){mode=NextTileOverlayMode(mode);assert(mode==expected);}
    for(int level=4;level<=19;++level){
        const int rows=1<<(level-4);
        assert(!HasTileEdgeCoordinates({level,0,0}));assert(!HasTileEdgeCoordinates({level,rows-1,0}));
        if(rows<=2)continue;
        for(int row:{1,rows/2,rows-2})for(int column:{0,rows,2*rows-1}){
            TileId id{level,row,column};assert(HasTileEdgeCoordinates(id));const auto b=Bounds(id);const auto labels=TileEdgeCoordinates(id);
            assert(labels[0].isLatitude&&labels[0].latitude==b.north&&labels[0].longitude==(b.west+b.east)*.5);
            assert(labels[1].isLatitude&&labels[1].latitude==b.south);
            assert(!labels[2].isLatitude&&labels[2].longitude==b.west&&labels[2].latitude==(b.north+b.south)*.5);
            assert(!labels[3].isLatitude&&labels[3].longitude==b.east);
            for(auto edge:labels){auto n=SurfaceDirection(edge.latitude*PlanetPi/180,edge.longitude*PlanetPi/180);assert(std::abs(Dot(n,n)-1)<1.e-12);}
            if(column<2*rows-1){auto next=TileEdgeCoordinates({level,row,column+1});assert(labels[3].latitude==next[2].latitude&&labels[3].longitude==next[2].longitude);}
        }
    }
    assert(TileCoordinateText({-45,0,true})==L"45.00\u00b0 S");
    assert(TileCoordinateText({0,-123.5,false})==L"123.50\u00b0 W");
    assert(TileCoordinateText({12.34,0,true})==L"12.34\u00b0 N");
    assert(TileCoordinateText({0,123.45,false})==L"123.45\u00b0 E");
    assert(TileCoordinateText({0,0,true})==L"0.00\u00b0 N");
    // A conversion-clean build instantiates both branches of the corrected UI layout.
    for(bool recording:{false,true}){const auto l=ScriptPanelLayout::Build(3840.f,2160.f,recording);assert(l.panel.right<=3840.f);}
    std::cout<<"Three overlay modes, both polar rows, edge coordinates and labels passed\n";
}
