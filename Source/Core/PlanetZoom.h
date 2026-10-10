#pragma once
#include <algorithm>
namespace OrbitX::Core {
// Orbiter Camera::ProcessMouse -> ShiftDist -> ChangeDist, external planetary mode.
// Distances here are km; Orbiter target->Size() is metres.
inline double PlanetWheelDistance(double distanceKm,double radiusKm,short delta){
    double rdist=distanceKm/radiusKm;
    double scale=std::max(1e-4,(rdist-1.0)/rdist);
    scale=std::max(scale,1.0/(radiusKm*1000.0));
    double shift=-double(delta)*0.001*scale;
    double factor=shift>0 ? 1.0+shift : std::max(1.0/rdist,1.0/(1.0-shift));
    return distanceKm*factor;
}
}
