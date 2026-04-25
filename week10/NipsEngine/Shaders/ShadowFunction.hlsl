float ComputeShadowPCF(
    float3 lightSpacePos,
    float4 ScaleOffset,
    int kernelHalfSize,
    SamplerComparisonState shadowSampler,
    Texture2D shadowMap,
    float bias
)
{
    float2 uv = lightSpacePos.xy * float2(0.5, -0.5) + 0.5;
    uv = ScaleOffset.xy * uv + ScaleOffset.zw;
    
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
            compareDepth - bias
        );
            count++;
        }

    return shadow / count;
}