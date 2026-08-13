// WildvineEngine fallback G-Buffer geometry shader.
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

cbuffer CBPerMaterial : register(b2)
{
    float4 BaseColor;
    float Metallic;
    float Roughness;
    float AO;
    float NormalScale;
    float EmissiveStrength;
    float AlphaCutoff;
    float _MaterialPad0;
    float _MaterialPad1;
    float _MaterialPad2;
    float _MaterialPad3;
    float _MaterialPad4;
    float _MaterialPad5;
};

Texture2D AlbedoTexture    : register(t0);
Texture2D NormalTexture    : register(t1);
Texture2D MetallicTexture  : register(t2);
Texture2D RoughnessTexture : register(t3);
Texture2D AOTexture        : register(t4);
Texture2D EmissiveTexture  : register(t5);
SamplerState MaterialSampler : register(s0);

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
    float4 Position  : SV_POSITION;
    float3 WorldPos  : TEXCOORD0;
    float3 Normal    : TEXCOORD1;
    float2 TexCoord  : TEXCOORD2;
    float3 Tangent   : TEXCOORD3;
    float3 Bitangent : TEXCOORD4;
};

struct GBufferOutput
{
    float4 AlbedoMetallic  : SV_Target0;
    float4 NormalRoughness : SV_Target1;
    float4 WorldAO         : SV_Target2;
    float4 EmissiveAlpha   : SV_Target3;
};

VSOutput VS(VSInput input)
{
    VSOutput output;
    float4 worldPosition = mul(float4(input.Position, 1.0f), World);
    float4 viewPosition = mul(worldPosition, View);
    output.Position = mul(viewPosition, Projection);
    output.WorldPos = worldPosition.xyz;
    output.Normal = normalize(mul(float4(input.Normal, 0.0f), World).xyz);
    output.Tangent = normalize(mul(float4(input.Tangent, 0.0f), World).xyz);
    output.Bitangent = normalize(mul(float4(input.Bitangent, 0.0f), World).xyz);
    output.TexCoord = input.TexCoord;
    return output;
}

GBufferOutput PS(VSOutput input)
{
    GBufferOutput output;

    float4 albedo = AlbedoTexture.Sample(MaterialSampler, input.TexCoord) * BaseColor;
    if (AlphaCutoff > 0.0f)
    {
        clip(albedo.a - AlphaCutoff);
    }

    float metallic = saturate(Metallic * MetallicTexture.Sample(MaterialSampler, input.TexCoord).r);
    float roughness = saturate(Roughness * RoughnessTexture.Sample(MaterialSampler, input.TexCoord).r);
    float ao = saturate(AO * AOTexture.Sample(MaterialSampler, input.TexCoord).r);
    float3 emissive = EmissiveTexture.Sample(MaterialSampler, input.TexCoord).rgb * EmissiveStrength;

    float3 tangentNormal = NormalTexture.Sample(MaterialSampler, input.TexCoord).xyz * 2.0f - 1.0f;
    tangentNormal.xy *= NormalScale;
    tangentNormal = normalize(tangentNormal);
    float3 baseNormal = normalize(input.Normal);
    float3 tangent = normalize(input.Tangent);
    float3 bitangent = normalize(input.Bitangent);
    float3 mappedNormal = normalize(tangentNormal.x * tangent + tangentNormal.y * bitangent + tangentNormal.z * baseNormal);

    output.AlbedoMetallic = float4(albedo.rgb, metallic);
    output.NormalRoughness = float4(mappedNormal, roughness);
    output.WorldAO = float4(input.WorldPos, ao);
    output.EmissiveAlpha = float4(emissive, albedo.a);
    return output;
}
