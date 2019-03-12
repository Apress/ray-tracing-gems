[shader("intersection")]
void mhIntersectNodeC()
{
  HitAttribs hitAttrib;
  uint nhits = intersectTriangle(PrimitiveIndex(), hitAttrib);
  if (nhits > 0)
  {
    // Process candidate intersection.
    // OMITTED: Equivalent to lines 9-20 of second listing, NaiveIntersect.hlsl.

    // Potentially update ray interval endpoint to gHitT[lastIdx] if we
    // wrote new hit data within the range of valid hits [0, Nquery-1].
    uint hitPos = hi / hitStride;
    if (hitPos < gNquery)
    {
      uint lastIdx =
          getHitBufferIndex(gNquery - 1, pixelIdx, pixelDims);
      ReportHit(gHitT[lastIdx], 0, hitAttrib);
    }
  }
}