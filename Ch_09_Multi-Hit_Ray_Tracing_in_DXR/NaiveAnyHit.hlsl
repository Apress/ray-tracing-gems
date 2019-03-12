[shader("anyhit")]
void mhAnyHitNaive(inout mhRayPayload rayPayload,
                   BuiltinIntersectionAttribs attribs)
{
  // Process candidate intersection.
  uint2 pixelIdx  = DispatchRaysIndex();
  uint2 pixelDims = DispatchRaysDimensions();
  uint  hitStride = pixelDims.x*pixelDims.y;
  float tval      = RayTCurrent();

  // Find index at which to store candidate intersection.
  uint hi = getHitBufferIndex(min(rayPayload.nhits, gNquery),
                              pixelIdx, pixelDims);
  uint lo = hi - hitStride;
  while (hi > 0 && tval < gHitT[lo])
  {
    // Move data to the right ...
    gHitT      [hi] = gHitT      [lo];
    gHitDiffuse[hi] = gHitDiffuse[lo];
    gHitNdotV  [hi] = gHitNdotV  [lo];

    // ... and try next position.
    hi -= hitStride;
    lo -= hitStride;
  }

  // Get diffuse color and face normal at current hit point.
  uint   primIdx = PrimitiveIndex();
  float4 diffuse = getDiffuseSurfaceColor(primIdx);
  float3 Ng      = getGeometricFaceNormal(primIdx);

  // Store hit data, possibly beyond index of the N <= Nquery closest
  // intersections (i.e., at hitPos == Nquery).
  gHitT      [hi] = tval;
  gHitDiffuse[hi] = diffuse;
  gHitNdotV  [hi] =
      abs(dot(normalize(Ng), normalize(WorldRayDirection())));

  ++rayPayload.nhits;

  // Reject the intersection and continue traversal with the incoming
  // ray interval.
  IgnoreHit();
}
