// WildvineEngine fallback deferred lighting shader.
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

cbuffer DeferredLightingDebugData : register(b1)
{
    int DebugViewMode;
    float ShadowStrength;
    float _DebugPad0;
    float _DebugPad1;
};

Texture2D GBufferAlbedoMetallic  : register(t0);
Texture2D GBufferNormalRoughness : register(t1);
Texture2D GBufferWorldAO         : register(t2);
Texture2D GBufferEmissiveAlpha   : register(t3);
Texture2D ShadowTexture          : register(t6);
SamplerState LightingSampler     : register(s0);

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
    float2 TexCoord : TEXCOORD0;
};

VSOutput VS(VSInput input)
{
    VSOutput output;
    output.Position = float4(input.Position.xy, 0.0f, 1.0f);
    output.TexCoord = input.TexCoord;
    return output;
}

float4 PS(VSOutput input) : SV_TARGET
{
    float4 albedoMetallic = GBufferAlbedoMetallic.Sample(LightingSampler, input.TexCoord);
    float4 normalRoughness = GBufferNormalRoughness.Sample(LightingSampler, input.TexCoord);
    float4 worldAO = GBufferWorldAO.Sample(LightingSampler, input.TexCoord);
    float4 emissiveAlpha = GBufferEmissiveAlpha.Sample(LightingSampler, input.TexCoord);

    // Empty G-buffer pixels retain alpha = 0 in the emissive target.
    if (emissiveAlpha.a <= 0.0001f)
    {
        return float4(0.10f, 0.10f, 0.10f, 1.0f);
    }

    float3 albedo = albedoMetallic.rgb;
    float metallic = saturate(albedoMetallic.a);
    float3 normal = normalize(normalRoughness.xyz);
    float roughness = saturate(normalRoughness.a);
    float ao = saturate(worldAO.a);

    if (DebugViewMode == 2)
        return float4(albedo, 1.0f);
    if (DebugViewMode == 3)
        return float4(normal * 0.5f + 0.5f, 1.0f);
    if (DebugViewMode == 4)
        return float4(roughness.xxx, 1.0f);
    if (DebugViewMode == 5)
        return float4(metallic.xxx, 1.0f);

    float3 N = normal;
    float3 L = normalize(-LightDir);
    float nDotL = saturate(dot(N, L));

    float diffuseWeight = lerp(1.0f, 0.55f, metallic);
    float ambientWeight = lerp(0.20f, 0.10f, roughness);
    float3 ambient = albedo * ao * ambientWeight;
    float3 diffuse = albedo * LightColor * nDotL * diffuseWeight;
    float3 color = ambient + diffuse + emissiveAlpha.rgb;

    if (DebugViewMode == 1)
    {
        // Shadow-factor debug fallback. Until a comparison sampler is wired,
        // show fully lit rather than failing shader creation.
        return float4(1.0f, 1.0f, 1.0f, 1.0f);
    }

    return float4(color, 1.0f);
}
