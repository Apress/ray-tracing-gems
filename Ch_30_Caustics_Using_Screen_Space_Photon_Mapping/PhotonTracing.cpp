void PhotonTracing(float2 LaunchIndex)
{
    // Initialize rays for a point light.
    Ray = UniformSampleSphere(LaunchIndex.xy);

    // Ray tracing
    for (int i = 0; i < MaxIterationCount; i++)
    {
        // Result of (reconstructed) surface data being hit by the ray.
        Result = rtTrace(Ray);

        bool bHit = Result.HitT >= 0.0;
        if (!bHit)
            break;

        // Storing conditions are described in Section 30.3.1.
        if (CheckToStorePhoton(Result))
        {
            // Storing a photon is described in Section 30.3.1.
            StorePhoton(Result);
        }

        // Photon is reflected if the surface has low enough roughness.
        if (Result.Roughness <= RoughnessThresholdForReflection)
        {
            FRayHitInfo Result = rtTrace(Ray)
            bool bHit = Result.HitT >= 0.0;
            if (bHit && CheckToStorePhoton(Result))
                StorePhoton(Result);
        }
        Ray = RefractPhoton(Ray, Result);
    }
}
