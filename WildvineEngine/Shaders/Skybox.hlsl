// WildvineEngine fallback skybox shader.
cbuffer CBSkybox : register(b0)
{
    float4x4 ViewProjection;
};

TextureCube SkyboxTexture : register(t10);
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
    output.Position = clipPosition.xyww;
    output.Direction = input.Position;
    return output;
}

float4 PS(VSOutput input) : SV_TARGET
{
    return SkyboxTexture.Sample(SkyboxSampler, normalize(input.Direction));
}
