[shader("anyhit")]
void mhAnyHitNodeC(inout mhRayPayload rayPayload,
     BuiltinIntersectionAttribs attribs)
{
  // Process candidate intersection.
  // OMITTED: Equivalent to lines 5-37 of first listing, NaiveAnyHit.hlsl.

  // If we store the candidate intersection at any index other than
  // the last valid hit position, reject the intersection.
  uint hitPos = hi / hitStride;
  if (hitPos != gNquery - 1)
    IgnoreHit();

  // Otherwise, induce node culling by (implicitly) returning and
  //  accepting RayTCurrent() as the new ray interval endpoint.
}
