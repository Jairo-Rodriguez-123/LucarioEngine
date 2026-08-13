// WildvineEngine fallback forward/PBR shader.
// Entry points required by ShaderProgram: VS and PS.

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
Texture2D ShadowTexture    : register(t6);
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

float4 PS(VSOutput input) : SV_TARGET
{
    float4 sampledAlbedo = AlbedoTexture.Sample(MaterialSampler, input.TexCoord);
    float4 surface = sampledAlbedo * BaseColor;

    if (AlphaCutoff > 0.0f)
    {
        clip(surface.a - AlphaCutoff);
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
    float3 N = normalize(tangentNormal.x * tangent + tangentNormal.y * bitangent + tangentNormal.z * baseNormal);
    float3 L = normalize(-LightDir);
    float nDotL = saturate(dot(N, L));

    // Simple, stable lighting fallback. Metallic/roughness are kept in the
    // material contract even though this fallback intentionally stays cheap.
    float diffuseWeight = lerp(1.0f, 0.55f, metallic);
    float roughnessAmbient = lerp(0.20f, 0.10f, roughness);
    float3 ambient = surface.rgb * ao * roughnessAmbient;
    float3 diffuse = surface.rgb * LightColor * nDotL * diffuseWeight;
    float3 color = ambient + diffuse + emissive;

    return float4(color, surface.a);
}
