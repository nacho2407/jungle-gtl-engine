float3 ApplyAmbientDirectionalLambert(float3 albedo, float3 normalWS)
{
    float3 N = normalize(normalWS);
    float3 L = normalize(-DirectionalLight.Direction);

    float3 ambient = albedo * AmbientLight.Color.rgb * AmbientLight.Intensity;
    float ndotl = saturate(dot(N, L));
    float3 directional = albedo * DirectionalLight.Color.rgb * DirectionalLight.Intensity
  * ndotl;

    return ambient + directional;
}