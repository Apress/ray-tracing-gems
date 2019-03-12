// These two functions suffice to implement the terminator fix. The second one can
// be used as a multiplier for either the incoming light or the BSDF evaluation.

// Return alpha^2 parameter from normal divergence
float bump_alpha2(float3 N, float3 Nbump)
{
    float cos_d = min(fabsf(dot(N, Nbump)), 1.0f);
    float tan2_d = (1 - cos_d * cos_d) / (cos_d * cos_d);
    return clamp(0.125f * tan2_d, 0.0f, 1.0f);
}

// Shadowing factor
float bump_shadowing_function(float3 N, float3 Ld, float alpha2)
{
    float cos_i = max(fabsf(dot(N, Ld)), 1e-6f);
    float tan2_i = (1 - cos_i * cos_i) / (cos_i * cos_i);
    return 2.0f / (1 + sqrtf(1 + alpha2 * tan2_i));
}
