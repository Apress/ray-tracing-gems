void SamplePrecomputedRadiance()
{
  float roughness = LoadRoughness(GBufferRoughness);
  float3 rayOrigin = ConstructRayOrigin(GBufferDepth);
  float3 L_total = float3(0, 0, 0); // Stochastic reflection
  float3 w_total = float3(0, 0, 0); // Sum of weights
  float primaryRayLengthApprox;
  float minNdotH = 2.0;
  uint cacheMissMask = 0;

  for (uint sampleId = 0;
        sampleId < RequiredSampleCount(roughness); sampleId++) {
    float3 sampleWeight;
    float NdotH;
    float3 rayDir =
          ImportanceSampleGGX(roughness, sampleWeight, NdotH);
    w_total += sampleWeight;
    float rayLength = RayLengthTexture[uint3(threadId, sampleId)];
    if (NdotH < minNdotH)
    {
      minNdotH = NdotH;
      primaryRayLengthApprox = rayLength;
    }
    float3 radiance = 0; // For cache misses, this will remain 0.
    if (rayLength < 0)
      radiance = SampleSkybox(rayDir);
    else if (roughness < RT_ROUGHNESS_THRESHOLD) {
      float3 hitPos = rayOrigin + rayLength * rayDir;
      if (!SampleScreen(hitPos, radiance)) {
        uint c;
        for (c = 0; c < CubeMapCount; c++)
          if (SampleRadianceProbe(c, hitPos, radiance)) break;
        if (c == CubeMapCount)
          cacheMissMask |= (1 << sampleId); // Sample was not found.
      }
    }
    else
      radiance = SampleCubeMapsWithProxyIntersection(rayDir);
    L_total += sampleWeight * radiance;
  }

  // Generate work separately for misses
  // to avoid branching in ray shading.
  uint missCount = bitcount(cacheMissMask);
  AppendToRayShadeInput(missCount, threadId, cacheMissMask);
  L_totalTexture[threadId] = L_total;
  w_totalTexture[threadId] = w_total;
  // Use ray length of the most likely ray to approximate the
  // primary ray intersection for motion.
  ReflectionMotionTexture[threadId] =
        CalculateMotion(primaryReflectionDir, primaryRayLengthApprox);
}
