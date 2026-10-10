cbuffer Scene : register(b0) { float4x4 gMVP; float4x4 gModel; float4 gSunDir; };
cbuffer Tile : register(b1) { float4 gUv; float4 gColor; float4 gDebug; float4 gGeo; };
Texture2D gTex : register(t0); SamplerState gSamp : register(s0);
struct VSIn { float3 pos:POSITION; float3 normal:NORMAL; float2 uv:TEXCOORD; };
struct PSIn { float4 pos:SV_POSITION; float3 normal:NORMAL; float2 uv:TEXCOORD; };
PSIn VSMain(VSIn i){
    PSIn o;
    float lon=gGeo.x+i.uv.x*gGeo.z,lat=gGeo.y-i.uv.y*gGeo.w;
    float slon,clon,slat,clat;sincos(lon,slon,clon);sincos(lat,slat,clat);
    // Right handed body fixed: +X prime meridian, +Y east, +Z north.
    float3 normal=float3(clat*clon,clat*slon,slat);
    float3 position=normal*(1-i.pos.z*gDebug.z);
    o.pos=mul(float4(position,1),gMVP);o.normal=normalize(mul(float4(normal,0),gModel).xyz);o.uv=i.uv;return o;
}
float4 PSMain(PSIn i):SV_TARGET {
    float3 albedo=gTex.Sample(gSamp,gUv.xy+i.uv*gUv.zw).rgb;
#ifdef PLANET_DEBUG_MODE
    float light=1;
#else
    float light=max(dot(normalize(i.normal),normalize(gSunDir.xyz)),0.0);
#endif
    float3 color=albedo*light;
    if(gDebug.y>0)color=lerp(color,gColor.rgb,0.16);
    float2 edge=min(i.uv,1-i.uv)/max(fwidth(i.uv),float2(1e-7,1e-7));
    if(gDebug.x>0 && min(edge.x,edge.y)<1.25)color=gColor.rgb;
    return float4(color,1);
}
