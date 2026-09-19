cbuffer Scene : register(b0) { float4x4 gMVP; float4x4 gModel; float4 gSunDir; };
Texture2D gTex : register(t0); SamplerState gSamp : register(s0);
struct VSIn { float3 pos:POSITION; float3 normal:NORMAL; float2 uv:TEXCOORD; };
struct PSIn { float4 pos:SV_POSITION; float3 normal:NORMAL; float2 uv:TEXCOORD; };
PSIn VSMain(VSIn i){ PSIn o; o.pos=mul(float4(i.pos,1),gMVP); o.normal=normalize(mul(float4(i.normal,0),gModel).xyz); o.uv=i.uv; return o; }
float4 PSMain(PSIn i):SV_TARGET { float3 albedo=gTex.Sample(gSamp,i.uv).rgb; float ndl=max(dot(normalize(i.normal),normalize(gSunDir.xyz)),0.0); float light=ndl; return float4(albedo*light,1); }
