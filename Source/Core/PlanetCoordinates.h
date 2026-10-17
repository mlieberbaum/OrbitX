#pragma once
#include <cmath>
#include <algorithm>
namespace OrbitX::Core {
constexpr double PlanetPi=3.14159265358979323846;
struct PlanetVector {double x{},y{},z{};};
inline PlanetVector operator+(PlanetVector a,PlanetVector b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
inline PlanetVector operator-(PlanetVector a,PlanetVector b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
inline PlanetVector operator*(PlanetVector a,double s){return {a.x*s,a.y*s,a.z*s};}
inline double Dot(PlanetVector a,PlanetVector b){return a.x*b.x+a.y*b.y+a.z*b.z;}
inline PlanetVector Cross(PlanetVector a,PlanetVector b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
inline PlanetVector Unit(PlanetVector v){return v*(1/std::sqrt(Dot(v,v)));}
// Right handed: +X = lon 0 equator, +Y = lon 90 E equator, +Z = north.
inline PlanetVector SurfaceDirection(double latitude,double longitude){
    return {std::cos(latitude)*std::cos(longitude),std::cos(latitude)*std::sin(longitude),std::sin(latitude)};
}
struct PlanetCamera {
    PlanetVector position,up,right,forward;
    static PlanetCamera Orbit(double yaw,double pitch,double distance){
        double lon=yaw-PlanetPi*.5,lat=-pitch;
        auto normal=SurfaceDirection(lat,lon);
        // Analytic tangent up preserves continuous pole crossings; no fixed-world-up singularity.
        PlanetVector up{-std::sin(lat)*std::cos(lon),-std::sin(lat)*std::sin(lon),std::cos(lat)};
        auto forward=normal*(-1);
        return {normal*distance,up,Unit(Cross(forward,up)),forward};
    }
    bool SphereInView(PlanetVector center,double radius,double tanY,double aspect) const {
        auto relative=center-position;double depth=Dot(relative,forward);
        if(depth+radius<0)return false;
        double tanX=tanY*aspect;
        return depth*tanX-std::abs(Dot(relative,right))>=-radius*std::sqrt(1+tanX*tanX)
            && depth*tanY-std::abs(Dot(relative,up))>=-radius*std::sqrt(1+tanY*tanY);
    }
};
}
