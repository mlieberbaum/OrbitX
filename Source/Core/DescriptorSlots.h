#pragma once
#include <array>
#include <bit>
#include <cassert>
#include <cstdint>
namespace OrbitX::Core {
// Descriptor zero is reserved. Preserve the old allocator's lowest-free-slot
// rule without allocating a temporary std::set for every texture upload.
template<unsigned Capacity> class DescriptorSlots {
    static_assert(Capacity>0);
public:
    unsigned FindFree()const{
        for(unsigned word=0;word<words.size();++word){
            const uint64_t free=~words[word];if(!free)continue;
            const auto slot=word*64+static_cast<unsigned>(std::countr_zero(free))+1;
            return slot<=Capacity?slot:0;
        }
        return 0;
    }
    void MarkUsed(unsigned slot){assert(slot>0&&slot<=Capacity);const auto i=slot-1;assert(!(words[i/64]&(uint64_t(1)<<(i%64))));words[i/64]|=uint64_t(1)<<(i%64);}
    void Release(unsigned slot){assert(slot>0&&slot<=Capacity);const auto i=slot-1;words[i/64]&=~(uint64_t(1)<<(i%64));}
private:
    std::array<uint64_t,(Capacity+63)/64> words{};
};
}
