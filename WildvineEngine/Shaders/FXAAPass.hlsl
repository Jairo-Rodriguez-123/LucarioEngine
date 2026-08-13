// WildvineEngine V16 - FXAA full-screen pass.
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
float Luminance(float3 c) { return dot(c,float3(0.2126,0.7152,0.0722)); }
float3 SampleScene(float2 uv) { return SceneTexture.Sample(SceneSampler,saturate(uv)).rgb; }
float4 PS(VSOutput input):SV_TARGET
{
    float2 uv=input.TexCoord;
    float2 px=InverseResolution*max(FXAAStrength,0.05);
    float3 rgbNW=SampleScene(uv+float2(-px.x,-px.y));
    float3 rgbNE=SampleScene(uv+float2( px.x,-px.y));
    float3 rgbSW=SampleScene(uv+float2(-px.x, px.y));
    float3 rgbSE=SampleScene(uv+float2( px.x, px.y));
    float3 rgbM=SampleScene(uv);
    float lumaNW=Luminance(rgbNW), lumaNE=Luminance(rgbNE), lumaSW=Luminance(rgbSW), lumaSE=Luminance(rgbSE), lumaM=Luminance(rgbM);
    float lumaMin=min(lumaM,min(min(lumaNW,lumaNE),min(lumaSW,lumaSE)));
    float lumaMax=max(lumaM,max(max(lumaNW,lumaNE),max(lumaSW,lumaSE)));
    float2 dir;
    dir.x=-((lumaNW+lumaNE)-(lumaSW+lumaSE));
    dir.y= ((lumaNW+lumaSW)-(lumaNE+lumaSE));
    float dirReduce=max((lumaNW+lumaNE+lumaSW+lumaSE)*(0.25*0.03125),0.0078125);
    float rcpDirMin=1.0/(min(abs(dir.x),abs(dir.y))+dirReduce);
    dir=clamp(dir*rcpDirMin,-8.0,8.0)*InverseResolution*max(FXAAStrength,0.05);
    float3 rgbA=0.5*(SampleScene(uv+dir*(1.0/3.0-0.5))+SampleScene(uv+dir*(2.0/3.0-0.5)));
    float3 rgbB=rgbA*0.5+0.25*(SampleScene(uv+dir*-0.5)+SampleScene(uv+dir*0.5));
    float lumaB=Luminance(rgbB);
    float3 result=(lumaB<lumaMin||lumaB>lumaMax)?rgbA:rgbB;
    return float4(result,1.0);
}
