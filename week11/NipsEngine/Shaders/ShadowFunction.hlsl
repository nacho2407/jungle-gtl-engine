float ComputeShadowPCF(
    float3 lightSpacePos,
    int kernelHalfSize,
    SamplerComparisonState shadowSampler,
    Texture2D shadowMap // float이 아니라 Texture2D<float>
)
{
    float2 uv = lightSpacePos.xy * float2(0.5, -0.5) + 0.5;
    float compareDepth = lightSpacePos.z;
    float2 shadowMapResolution;
    shadowMap.GetDimensions(shadowMapResolution.x, shadowMapResolution.y);

    float2 texelSize = 1.0 / shadowMapResolution; // 하드코딩 제거

    float shadow = 0.0;
    int count = 0;

    for (int x = -kernelHalfSize; x <= kernelHalfSize; x++)
        for (int y = -kernelHalfSize; y <= kernelHalfSize; y++)
        {
            shadow += shadowMap.SampleCmpLevelZero(
            shadowSampler,
            uv + float2(x, y) * texelSize,
            compareDepth
        );
            count++;
        }

    return shadow / count;
}