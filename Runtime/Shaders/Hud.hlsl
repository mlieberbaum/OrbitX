struct V { float2 pos:POSITION; float4 color:COLOR; };
struct P { float4 pos:SV_POSITION; float4 color:COLOR; };
P VSMain(V i){ P o; o.pos=float4(i.pos,0,1); o.color=i.color; return o; }
float4 PSMain(P i):SV_TARGET { return i.color; }
