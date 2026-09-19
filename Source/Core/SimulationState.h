#pragma once
#include <DirectXMath.h>

namespace OrbitX::Core {
struct DVec3 { double x{}, y{}, z{}; };
inline DVec3 operator+(DVec3 a, DVec3 b){ return {a.x+b.x,a.y+b.y,a.z+b.z}; }
inline DVec3 operator-(DVec3 a, DVec3 b){ return {a.x-b.x,a.y-b.y,a.z-b.z}; }

struct SimulationState {
    // Frozen v0 state supplied by user. Units: km, ICRF, SSB.
    DVec3 sunSSB   {-1067599.06168377, -395988.983665861, -138071.310109251};
    DVec3 earthSSB {-27566633.2905461, 132361428.68102, 57418646.1377974};
    DVec3 moonEarth{-291608.388457196, -266716.829237424, -76102.4813232016};
    DVec3 moonSSB() const { return earthSSB + moonEarth; }
    const char* epochText = "2000 JAN 01 12:00:00 TDB";
};
}
