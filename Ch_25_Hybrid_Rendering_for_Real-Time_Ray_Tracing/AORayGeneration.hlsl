// Partial code for AO ray generation shader, truncated for brevity.
// The full shader is otherwise essentially identical to the shadow
// ray generation.
float result = 0;

for (uint i = 0; i < numRays; i++)
{
  // Select a random direction for our AO ray.
  float rnd1 = frac(haltonNext(hState) + randomNext(frameSeed));
  float rnd2 = frac(haltonNext(hState) + randomNext(frameSeed));
  float3 rndDir = cosineSampleHemisphere(rnd1, rnd2);
  
  // Rotate the hemisphere.
  // Up is in the direction of the pixel surface normal.
  float3 rndWorldDir = mul(rndDir, createBasis(gbuffer.worldNormal));
  
  // Create a ray and payload.
  ShadowData shadowPayload;
  shadowPayload.miss = false;
  
  RayDesc ray;
  ray.Origin = position;
  ray.Direction = rndWorldDir;
  ray.TMin = g_aoConst.minRayLength;
  ray.TMax = g_aoConst.maxRayLength;
  
  // Trace our ray;
  // use the shadow miss, since we only care if we miss or not.
  TraceRay(g_rtScene,
           RAY_FLAG_SKIP_CLOSEST_HIT_SHADER|
               RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH,
           RaytracingInstanceMaskAll,
           HitType_Shadow,
           SbtRecordStride,
           MissType_Shadow,
           ray,
           shadowPayload);
  
  result += shadowPayload.miss ? 1 : 0;
}

result /= numRays;
