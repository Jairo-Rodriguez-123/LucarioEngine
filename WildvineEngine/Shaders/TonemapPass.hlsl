// WildvineEngine V16 - HDR tonemapping full-screen pass.
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
float3 ACESFilm(float3 x)
{
    const float a=2.51, b=0.03, c=2.43, d=0.59, e=0.14;
    return saturate((x*(a*x+b))/(x*(c*x+d)+e));
}
float4 PS(VSOutput input):SV_TARGET
{
    float3 color=SceneTexture.Sample(SceneSampler,input.TexCoord).rgb;
    color*=max(Exposure,0.01);
    color=ACESFilm(color);
    if (abs(Gamma-1.0)>0.001) color=pow(max(color,0.0),1.0/max(Gamma,0.1));
    return float4(color,1.0);
}
