void rayHit(inout Result result)
{
  result.RayLength = RayTCurrent();
  result.InstanceId = InstanceId();
  result.PrimitiveId = PrimitiveIndex();
  result.Barycentrics = barycentrics;
}

void rayGen()
{
  float roughness = LoadRoughness(GBufferRoughness);
  uint sampleCount = SamplesRequired(roughness);
  if (roughness < RT_ROUGHNESS_THRESHOLD) {
    float3 ray_o = ConstructRayOrigin(GBufferDepth);
    for (uint sampleIndex = 0;
          sampleIndex < sampleCount; sampleIndex++) {
      float3 ray_d = ImportanceSampleGGX(roughness);

      TraceRay(ray_o, ray_d, results);
      StoreRayIntersectionAttributes(results, index.xy, sampleIndex);
      RayLengthTarget[uint3(index.xy, sampleIndex)] = rayLength;
    }
  }
}
