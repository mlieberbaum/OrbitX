#include "../Core/PlanetFrameCache.h"
#include "../Core/DescriptorSlots.h"
#include <cassert>
#include <chrono>
#include <iostream>
#include <set>
using namespace OrbitX::Core;
int main(){
    DescriptorSlots<8192> slots;std::set<unsigned> used;uint32_t random=12345;
    for(unsigned step=0;step<30000;++step){
        random=random*1664525u+1013904223u;
        if(!used.empty()&&(random%4==0||used.size()==8192)){
            auto it=used.begin();std::advance(it,random%used.size());slots.Release(*it);used.erase(it);
        }else{
            unsigned expected=1;while(used.contains(expected))++expected;
            const auto actual=slots.FindFree();assert(actual==expected);slots.MarkUsed(actual);used.insert(actual);
        }
    }
    DescriptorSlots<65> partial;for(unsigned i=1;i<=65;++i){assert(partial.FindFree()==i);partial.MarkUsed(i);}assert(partial.FindFree()==0);partial.Release(65);assert(partial.FindFree()==65);
    PlanetFrameCache cache;PlanetFrameKey key{.7f,.2f,2200,40,3840,2160,false,2,400,3};std::vector<TileId> leaves={{4,0,0},{4,0,1}};
    assert(!cache.Reusable(key,leaves,false));cache.Remember(key,leaves);assert(cache.Reusable(key,leaves,false));assert(!cache.Reusable(key,leaves,true));
    auto changed=key;changed.yaw+=.01f;assert(!cache.Reusable(changed,leaves,false));changed=key;changed.pitch+=.01f;assert(!cache.Reusable(changed,leaves,false));
    changed=key;changed.distanceKm+=1;assert(!cache.Reusable(changed,leaves,false));changed=key;changed.fov+=1;assert(!cache.Reusable(changed,leaves,false));
    changed=key;changed.width++;assert(!cache.Reusable(changed,leaves,false));changed=key;changed.height++;assert(!cache.Reusable(changed,leaves,false));
    changed=key;changed.frozen=true;assert(!cache.Reusable(changed,leaves,false));changed=key;changed.tiles++;assert(!cache.Reusable(changed,leaves,false));
    changed=key;changed.files++;assert(!cache.Reusable(changed,leaves,false));changed=key;changed.uploadFence++;assert(!cache.Reusable(changed,leaves,false));
    auto changedLeaves=leaves;changedLeaves[0]={5,0,0};assert(!cache.Reusable(key,changedLeaves,false));cache.Invalidate();assert(!cache.Reusable(key,leaves,false));
    std::cout<<"Descriptor slots match lowest-free-set allocation over 30000 operations; settled-frame invalidation checks passed\n";
}
