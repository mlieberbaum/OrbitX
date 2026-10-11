#pragma once
#include <algorithm>
#include <cmath>
#include <compare>
#include <stdexcept>
#include <filesystem>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>
#include "PlanetCoordinates.h"
#include "EnvironmentTrace.h"

namespace OrbitX::Core {
struct TileId {
    int level=4, row=0, column=0;
    auto operator<=>(const TileId&) const = default;
    TileId Parent() const { return {level-1,row/2,column/2}; }
};
struct TileBounds { double south,north,west,east; };
inline TileBounds Bounds(TileId id) {
    const double rows=double(1 << (id.level-4));
    return {90-180*(id.row+1)/rows,90-180*id.row/rows,
            -180+180*id.column/rows,-180+180*(id.column+1)/rows};
}
// Orbiter's internal level zero is directory 04: one latitude band, two longitude bands.
class PlanetQuadtree {
public:
    std::map<TileId,std::filesystem::path> files;
    std::set<TileId> nodes;
    std::vector<TileId> leaves, requests;
    bool frozen=false;
    size_t rejectedFiles=0;
    void Scan(const std::filesystem::path& root) {
        files.clear();nodes.clear();rejectedFiles=0;
        if(!std::filesystem::is_directory(root)) return;
        for(const auto& entry:std::filesystem::recursive_directory_iterator(root)) {
            if(!entry.is_regular_file() || entry.path().extension()!=L".dds") continue;
            try {
                auto relative=entry.path().lexically_relative(root);
                std::vector<std::string> parts;for(const auto& p:relative) parts.push_back(p.string());
                if(parts.size()!=3) {++rejectedFiles;continue;}
                auto number=[](const std::string& s){size_t end=0;int n=std::stoi(s,&end);if(end!=s.size())throw std::invalid_argument("index");return n;};
                TileId id{number(parts[0]),number(parts[1]),number(entry.path().stem().string())};
                if(id.level<4) continue; // Older global directories 01..03 are superseded by the DDS roots.
                if(id.level>19 || id.row<0 || id.row>=(1<<(id.level-4)) || id.column<0 || id.column>=(2<<(id.level-4))) {++rejectedFiles;continue;}
                files[id]=entry.path();nodes.insert(id);
                while(id.level>4) {id=id.Parent();nodes.insert(id);}
            } catch(const std::exception&) {++rejectedFiles;}
        }
    }
    void Select(double cx,double cy,double cz,double radius,double pixelsPerRadian,
                const std::function<bool(TileId)>& resident,
                const std::function<bool(PlanetVector,double)>& inView={},EnvironmentTrace* trace=nullptr) {
        if(frozen && !leaves.empty()) {
            if(trace)trace->Log([&](auto& out){out<<"SELECTION_FROZEN leaves="<<leaves.size();});
            return;
        }
        leaves.clear();requests.clear();
        const double distance=std::sqrt(cx*cx+cy*cy+cz*cz);
        auto request=[&](TileId id){if(files.contains(id)&&!resident(id)){
            requests.push_back(id);
            if(trace)trace->Log([&](auto& out){out<<"TILE_REQUEST tile="<<id.level<<"/"<<id.row<<"/"<<id.column;});
        }};
        std::function<void(TileId)> visit=[&](TileId id) {
            auto b=Bounds(id);constexpr double rad=PlanetPi/180;
            auto normal=SurfaceDirection((b.north+b.south)*.5*rad,(b.east+b.west)*.5*rad);
            double angle=std::acos(std::clamp(Dot(normal,{cx,cy,cz})/distance,-1.0,1.0));
            double extent=std::min(PlanetPi,((b.north-b.south)+(b.east-b.west))*.5*rad);
            if(trace)trace->Log([&](auto& out){out<<"TILE_QUERY tile="<<id.level<<"/"<<id.row<<"/"<<id.column
                <<" angle="<<angle<<" extent="<<extent<<" center="<<normal.x<<","<<normal.y<<","<<normal.z;});
            if(angle-extent>std::acos(std::clamp(radius/distance,0.0,1.0))) {
                if(trace)trace->Log([&](auto& out){out<<"TILE_CULL tile="<<id.level<<"/"<<id.row<<"/"<<id.column<<" reason=HORIZON";});return;
            }
            if(inView && !inView(normal*radius,2*radius*std::sin(extent*.5))) {
                if(trace)trace->Log([&](auto& out){out<<"TILE_CULL tile="<<id.level<<"/"<<id.row<<"/"<<id.column<<" reason=FRUSTUM";});return;
            }
            request(id);
            auto delta=PlanetVector{cx,cy,cz}-normal*radius;
            double nearDistance=std::max(distance-radius,std::sqrt(Dot(delta,delta))-radius*extent);
            double footprint=radius*(b.north-b.south)*rad*pixelsPerRadian/nearDistance;
            bool children=false;for(int r=0;r<2;r++)for(int c=0;c<2;c++)children|=nodes.contains({id.level+1,id.row*2+r,id.column*2+c});
            if(trace)trace->Log([&](auto& out){out<<"LOD_DECISION tile="<<id.level<<"/"<<id.row<<"/"<<id.column
                <<" footprint="<<footprint<<" threshold=384 near_distance="<<nearDistance<<" children="<<children
                <<" leaves_so_far="<<leaves.size()<<" leaf_limit=1500 max_level=19";});
            if(id.level<19 && footprint>384 && children && leaves.size()<1500) {
                bool ready=true;
                for(int r=0;r<2;r++)for(int c=0;c<2;c++) {
                    TileId child{id.level+1,id.row*2+r,id.column*2+c};
                    request(child);if(files.contains(child)&&!resident(child))ready=false;
                    if(trace)trace->Log([&](auto& out){out<<"CHILD_COVERAGE tile="<<child.level<<"/"<<child.row<<"/"<<child.column
                        <<" has_file="<<files.contains(child)<<" resident="<<resident(child);});
                }
                // D3D9 principle: render the parent until its child coverage is ready.
                if(ready){
                    if(trace)trace->Log([&](auto& out){out<<"TILE_REFINE tile="<<id.level<<"/"<<id.row<<"/"<<id.column;});
                    for(int r=0;r<2;r++)for(int c=0;c<2;c++)visit({id.level+1,id.row*2+r,id.column*2+c});return;
                }
                if(trace)trace->Log([&](auto& out){out<<"PARENT_RETAIN tile="<<id.level<<"/"<<id.row<<"/"<<id.column<<" reason=CHILD_NOT_READY";});
            }
            leaves.push_back(id);
            if(trace)trace->Log([&](auto& out){out<<"SELECT_LEAF tile="<<id.level<<"/"<<id.row<<"/"<<id.column;});
        };
        visit({4,0,0});visit({4,0,1});
        std::sort(requests.begin(),requests.end());requests.erase(std::unique(requests.begin(),requests.end()),requests.end());
    }
};
}
