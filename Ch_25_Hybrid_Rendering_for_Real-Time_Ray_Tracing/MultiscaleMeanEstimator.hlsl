struct MultiscaleMeanEstimatorData
{
  float3 mean;
  float3 shortMean;
  float vbbr;
  float3 variance;
  float inconsistency;
};
  
float3 MultiscaleMeanEstimator(float3 y,
  inout MultiscaleMeanEstimatorData data,
  float shortWindowBlend = 0.08f)
{
  float3 mean = data.mean;
  float3 shortMean = data.shortMean;
  float vbbr = data.vbbr;
  float3 variance = data.variance;
  float inconsistency = data.inconsistency;
  
  // Suppress fireflies.
  {
    float3 dev = sqrt(max(1e-5, variance));
    float3 highThreshold = 0.1 + shortMean + dev * 8;
    float3 overflow = max(0, y - highThreshold);
    y -= overflow;
  }
  
  float3 delta = y - shortMean;
  shortMean = lerp(shortMean, y, shortWindowBlend);
  float3 delta2 = y - shortMean;
  
  // This should be a longer window than shortWindowBlend to avoid bias
  // from the variance getting smaller when the short-term mean does.
  float varianceBlend = shortWindowBlend * 0.5;
  variance = lerp(variance, delta * delta2, varianceBlend);
  float3 dev = sqrt(max(1e-5, variance));
  
  float3 shortDiff = mean - shortMean;
  
  float relativeDiff = dot( float3(0.299, 0.587, 0.114),
        abs(shortDiff) / max(1e-5, dev) );
  inconsistency = lerp(inconsistency, relativeDiff, 0.08);
  
  float varianceBasedBlendReduction =
        clamp( dot( float3(0.299, 0.587, 0.114),
        0.5 * shortMean / max(1e-5, dev) ), 1.0/32, 1 );
  
  float3 catchUpBlend = clamp(smoothstep(0, 1,
        relativeDiff * max(0.02, inconsistency - 0.2)), 1.0/256, 1);
  catchUpBlend *= vbbr;
  
  vbbr = lerp(vbbr, varianceBasedBlendReduction, 0.1);
  mean = lerp(mean, y, saturate(catchUpBlend));
  
  // Output
  data.mean = mean;
  data.shortMean = shortMean;
  data.vbbr = vbbr;
  data.variance = variance;
  data.inconsistency = inconsistency;
  
  return mean;
}
