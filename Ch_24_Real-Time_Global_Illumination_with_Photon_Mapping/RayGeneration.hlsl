struct Payload
{
    // Next ray direction, last element is padding
    half4 direction;
    // RNG state
    uint2 random;
    // Packed photon power
    uint power;
    // Ray length
    float t;
    // Bounce count
    uint bounce;
};

[shader("raygeneration")]
void rayGen()
{
    Payload p;
    RayDesc ray;
    
    // First, we read the initial sample from the RSM.
    ReadRSMSamplePosition(p);
    
    // We check if bounces continue by the bounce count
    // and ray length (zero for terminated trace or miss).
    while (p.bounce < MAX_BOUNCE_COUNT && p.t != 0)
    {
        // We get the ray origin and direction for the state.
        ray.Origin = get_hit_position_in_world(p, ray);
        ray.Direction = p.direction.xyz;
        
        TraceRay(gRtScene, RAY_FLAG_FORCE_OPAQUE, 0xFF, 0,1,0, ray, p);
        p.bounce++;
    }
}
