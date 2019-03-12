[shader("intersection")]
void mhIntersectNaive()
{
  HitAttribs hitAttrib;
  uint nhits = intersectTriangle(PrimitiveIndex(), hitAttrib);
  if (nhits > 0)
  {
    // Process candidate intersection.
    uint2 pixelIdx  = DispatchRaysIndex();
    uint2 pixelDims = DispatchRaysDimensions();
    uint  hitStride = pixelDims.x*pixelDims.y;
    float tval      = hitAttrib.tval;

    // Find index at which to store candidate intersection.
    uint hi = getHitBufferIndex(min(nhits, gNquery),
                                pixelIdx, pixelDims);
    // OMITTED: Equivalent to lines 13-35 of previous listing.

    uint hcIdx = getHitBufferIndex(0, pixelIdx, pixelDims);
    ++gHitCount[hcIdx];
  }
}
