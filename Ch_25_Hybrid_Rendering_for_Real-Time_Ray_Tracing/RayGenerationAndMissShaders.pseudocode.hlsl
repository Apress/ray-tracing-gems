// HLSL pseudocode---does not compile.
[shader("raygeneration")]
void shadowRaygen()
{
  uint2 launchIndex = DispatchRaysIndex();
  uint2 launchDim = DispatchRaysDimensions();
  uint2 pixelPos = launchIndex +
      uint2(g_pass.launchOffsetX, g_pass.launchOffsetY);
  const float depth = g_depth[pixelPos];

  // Skip sky pixels.
  if (depth == 0.0)
  {
    g_output[pixelPos] = float4(0, 0, 0, 0);
    return;
  }

  // Compute position from depth buffer.
  float2 uvPos = (pixelPos + 0.5) * g_raytracing.viewDimensions.zw;
  float4 csPos = float4(uvToCs(uvPos), depth, 1);
  float4 wsPos = mul(g_raytracing.clipToWorld, csPos);
  float3 position = wsPos.xyz / wsPos.w;

  // Initialize the Halton sequence.
  HaltonState hState =
      haltonInit(hState, pixelPos, g_raytracing.frameIndex);

  // Generate random numbers to rotate the Halton sequence.
  uint frameseed =
      randomInit(pixelPos, launchDim.x, g_raytracing.frameIndex);
  float rnd1 = frac(haltonNext(hState) + randomNext(frameseed));
  float rnd2 = frac(haltonNext(hState) + randomNext(frameseed));
  
  // Generate a random direction based on the cone angle.
  // The wider the cone, the softer (and noisier) the shadows are.
  // uniformSampleCone() from [pbrt]
  float3 rndDirection = uniformSampleCone(rnd1, rnd2, cosThetaMax);
  
  // Prepare a shadow ray.
  RayDesc ray;
  ray.Origin = position;
  ray.Direction = g_sunLight.L;
  ray.TMin = max(1.0f, length(position)) * 1e-3f;
  ray.TMax = tmax;
  ray.Direction = mul(rndDirection, createBasis(L));
  
  // Initialize the payload; assume that we have hit something.
  ShadowData shadowPayload;
  shadowPayload.miss = false;
  
  // Launch a ray.
  // Tell the API that we are skipping hit shaders. Free performance!
  TraceRay(rtScene,
      RAY_FLAG_SKIP_CLOSEST_HIT_SHADER,
      RaytracingInstanceMaskAll, HitType_Shadow, SbtRecordStride,
      MissType_Shadow, ray, shadowPayload);
  
  // Read the payload. If we have missed, the shadow value is white.
  g_output[pixelPos] = shadowPayload.miss ? 1.0f : 0.0f;
}

[shader("miss")]
void shadowMiss(inout ShadowData payload : SV_RayPayload)
{
  payload.miss = true;
}
