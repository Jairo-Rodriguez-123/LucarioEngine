// WildvineEngine V16 - Bloom full-screen pass.
cbuffer PostProcessData : register(b0)
{
    float2 InverseResolution;
    float Exposure;
    float BloomThreshold;
    float BloomIntensity;
    float FXAAStrength;
    int BloomEnabled;
    int TonemappingEnabled;
    int FXAAEnabled;
    int PostProcessEnabled;
    float Gamma;
    float _Padding0;
};
Texture2D SceneTexture : register(t0);
SamplerState SceneSampler : register(s0);
struct VSInput { float3 Position:POSITION; float3 Normal:NORMAL; float3 Tangent:TANGENT; float3 Bitangent:BITANGENT; float2 TexCoord:TEXCOORD0; };
struct VSOutput { float4 Position:SV_POSITION; float2 TexCoord:TEXCOORD0; };
VSOutput VS(VSInput input) { VSOutput o; o.Position=float4(input.Position.xy,0,1); o.TexCoord=input.TexCoord; return o; }
float Luminance(float3 c) { return dot(c, float3(0.2126,0.7152,0.0722)); }
float3 SampleScene(float2 uv) { return SceneTexture.Sample(SceneSampler, saturate(uv)).rgb; }
float3 Bright(float3 color)
{
    float brightness=Luminance(color);
    float knee=max(BloomThreshold*0.35,0.0001);
    float soft=saturate((brightness-BloomThreshold+knee)/(2.0*knee));
    float contribution=max(brightness-BloomThreshold,0.0)+soft*soft*knee;
    return color*(contribution/max(brightness,0.0001));
}
float4 PS(VSOutput input):SV_TARGET
{
    float2 uv=input.TexCoord;
    float3 base=SampleScene(uv);
    float2 px=InverseResolution;
    float3 bloom=Bright(base)*0.18;
    bloom += Bright(SampleScene(uv+float2( px.x,0)))*0.10;
    bloom += Bright(SampleScene(uv+float2(-px.x,0)))*0.10;
    bloom += Bright(SampleScene(uv+float2(0, px.y)))*0.10;
    bloom += Bright(SampleScene(uv+float2(0,-px.y)))*0.10;
    float2 p2=px*2.5;
    bloom += Bright(SampleScene(uv+float2( p2.x, p2.y)))*0.075;
    bloom += Bright(SampleScene(uv+float2(-p2.x, p2.y)))*0.075;
    bloom += Bright(SampleScene(uv+float2( p2.x,-p2.y)))*0.075;
    bloom += Bright(SampleScene(uv+float2(-p2.x,-p2.y)))*0.075;
    float2 p4=px*5.0;
    bloom += Bright(SampleScene(uv+float2( p4.x,0)))*0.05;
    bloom += Bright(SampleScene(uv+float2(-p4.x,0)))*0.05;
    bloom += Bright(SampleScene(uv+float2(0, p4.y)))*0.05;
    bloom += Bright(SampleScene(uv+float2(0,-p4.y)))*0.05;
    return float4(base + bloom*BloomIntensity,1.0);
}
