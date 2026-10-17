#pragma once
#include <cstdint>
#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <vector>
namespace OrbitX::Graphics {
struct DdsSurface {
    uint32_t width{},height{},mips{},blockBytes{},format{};
    std::vector<uint8_t> bytes;
    static DdsSurface Read(const std::filesystem::path& path) {
        std::ifstream f(path,std::ios::binary|std::ios::ate);
        if(!f) throw std::runtime_error("Cannot open DDS");
        auto size=f.tellg();if(size<128 || size>64*1024*1024) throw std::runtime_error("Invalid DDS length");
        f.seekg(0);std::vector<uint8_t> raw(static_cast<size_t>(size));f.read(reinterpret_cast<char*>(raw.data()),size);
        if(!f)throw std::runtime_error("DDS read failed");
        auto word=[&](size_t offset){uint32_t v;std::memcpy(&v,raw.data()+offset,4);return v;};
        if(word(0)!=0x20534444 || word(4)!=124 || word(76)!=32 || (word(112)&0xFE00)) throw std::runtime_error("Expected a 2D DDS surface");
        DdsSurface d;d.height=word(12);d.width=word(16);d.mips=std::max(1u,word(28));
        auto code=word(84);size_t offset=128;
        if(code==0x31545844) {d.format=71;d.blockBytes=8;} // DXT1 / BC1_UNORM
        else if(code==0x33545844) {d.format=74;d.blockBytes=16;}
        else if(code==0x35545844) {d.format=77;d.blockBytes=16;}
        else if(code==0x30315844 && raw.size()>=148) {
            d.format=word(128);offset=148;
            if(word(132)!=3 || word(140)!=1 || (word(136)&4)) throw std::runtime_error("Unsupported DDS array/cube");
            if(d.format==71||d.format==72)d.blockBytes=8;
            else if(d.format==74||d.format==75||d.format==77||d.format==78||d.format==98||d.format==99)d.blockBytes=16;
        }
        if(!d.blockBytes || !d.width || !d.height || d.width>16384 || d.height>16384 || d.mips>15) throw std::runtime_error("Unsupported DDS format/dimensions");
        size_t required=0;auto w=d.width,h=d.height;uint32_t maxMips=1;for(auto n=std::max(w,h);n>1;n/=2)++maxMips;
        if(d.mips>maxMips)throw std::runtime_error("Invalid mip count");
        for(uint32_t i=0;i<d.mips;i++){required+=size_t((w+3)/4)*((h+3)/4)*d.blockBytes;w=std::max(1u,w/2);h=std::max(1u,h/2);}
        if(raw.size()-offset<required) throw std::runtime_error("Truncated DDS mip data");
        d.bytes.assign(raw.begin()+offset,raw.begin()+offset+required);return d;
    }
};
}
