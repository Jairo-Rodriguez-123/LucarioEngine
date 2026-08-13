// Wildvine Engine V17 panoramic/equirectangular skybox shader.
cbuffer CBSkybox : register(b0)
{
    float4x4 ViewProjection;
    // x = horizontal rotation in radians, y = intensity.
    float4 SkyParams;
    float4 SkyTint;
};

Texture2D SkyboxTexture : register(t10);
SamplerState SkyboxSampler : register(s10);

struct VSInput
{
    float3 Position : POSITION;
};

struct VSOutput
{
    float4 Position : SV_POSITION;
    float3 Direction : TEXCOORD0;
};

VSOutput VS(VSInput input)
{
    VSOutput output;
    float4 clipPosition = mul(float4(input.Position, 1.0f), ViewProjection);
    // Force the sky to the far plane while preserving perspective direction.
    output.Position = clipPosition.xyww;
    output.Direction = input.Position;
    return output;
}

float2 DirectionToEquirectangularUV(float3 direction)
{
    const float PI = 3.14159265358979323846f;
    const float TWO_PI = 6.28318530717958647692f;
    float3 d = normalize(direction);

    float longitude = atan2(d.z, d.x) + SkyParams.x;
    float latitude = asin(clamp(d.y, -1.0f, 1.0f));

    float2 uv;
    uv.x = frac(0.5f + longitude / TWO_PI);
    uv.y = 0.5f - latitude / PI;
    return uv;
}

float4 PS(VSOutput input) : SV_TARGET
{
    float2 uv = DirectionToEquirectangularUV(input.Direction);
    float3 color = SkyboxTexture.Sample(SkyboxSampler, uv).rgb;
    color *= SkyTint.rgb * SkyParams.y;
    return float4(color, 1.0f);
}
