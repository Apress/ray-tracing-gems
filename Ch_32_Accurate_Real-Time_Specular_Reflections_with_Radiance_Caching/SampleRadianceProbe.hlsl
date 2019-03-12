bool SampleRadianceProbe(uint probeIndex,
                         float3 hitPos,
                         out float3 radiance)
{
  CubeMap cube = LoadCube(probeIndex);
  float3 fromCube = hitPos - cube.Position;
  float distSqr = dot(fromCube, fromCube);
  if (distSqr <= cube.RadiusSqr) {
    float3 cubeFace = MaxDir(fromCube); // (1,0,0), (0,1,0) or (0,0,1)
    float hitZInCube = dot(cubeFace, fromCube);
    float p = ProbabilityToSampleSameTexel(cube, hitZInCube, hitPos);
    if (p < ResolutionThreshold) {
      float distanceFromCube = sqrt(distSqr);
      float3 sampleDir = fromCube / distanceFromCube;
      float zSeenByCube =
            ZInCube(cube.Depth.SampleLevel(Sampler, sampleDir, 0));
      // 1/cos(view angle), used to get the distance along the view ray
      float cosCubeViewAngleRcp = distanceFromCube / hitZInCube;
      float dist = abs(hitZInCube - zSeenByCube) * cosCubeViewAngleRcp;
      if (dist <
            OcclusionThresholdFactor * hitZInCube / cube.Resolution) {
        radiance = cube.Radiance.SampleLevel(Sampler, sampleDir, 0);
        return true;
      }
    }
  }
  return false;
}
