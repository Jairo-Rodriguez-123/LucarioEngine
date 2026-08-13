// WildvineEngine fallback shadow-map shader.
#define MAX_LIGHTS 8

cbuffer CBPerFrame : register(b0)
{
    float4x4 View;
    float4x4 Projection;
    float4x4 LightViewProjection;
    float3 CameraPos;
    float _FramePad0;
    float3 LightDir;
    float _FramePad1;
    float3 LightColor;
    float LightRange;
    float3 LightPosition;
    int LightType;
    float4 LightPositionsRanges[MAX_LIGHTS];
    float4 LightColorsTypes[MAX_LIGHTS];
    float4 LightDirectionsIntensities[MAX_LIGHTS];
    int LightCount;
    float3 _FramePad2;
};

cbuffer CBPerObject : register(b1)
{
    float4x4 World;
};

struct VSInput
{
    float3 Position  : POSITION;
    float3 Normal    : NORMAL;
    float3 Tangent   : TANGENT;
    float3 Bitangent : BITANGENT;
    float2 TexCoord  : TEXCOORD0;
};

struct VSOutput
{
    float4 Position : SV_POSITION;
};

VSOutput VS(VSInput input)
{
    VSOutput output;
    float4 worldPosition = mul(float4(input.Position, 1.0f), World);
    output.Position = mul(worldPosition, LightViewProjection);
    return output;
}

float4 PS(VSOutput input) : SV_TARGET
{
    return float4(1.0f, 1.0f, 1.0f, 1.0f);
}
