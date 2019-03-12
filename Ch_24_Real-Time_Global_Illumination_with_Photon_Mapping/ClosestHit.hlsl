[shader("closesthit")]
void closestHitShader(inout Payload p : SV_RayPayload,
    in IntersectionAttributes attribs : SV_IntersectionAttributes)
{
    // Load surface attributes for the hit.
    surface_attributes surface = LoadSurface(attribs);
    
    float3 ray_direction = WorldRayDirection();
    float3 hit_pos = WorldRayOrigin() + ray_direction * t;
    float3 incoming_power = from_rbge5999(p.power);
    float3 outgoing_power = .0f;
    
    RandomStruct r;
    r.seed = p.random.x;
    r.key = p.random.y;
    
    // Russian roulette check
    float3 outgoing_direction = .0f;
    float3 store_power = .0f;
    bool keep_going = russian_roulette(incoming_power, ray_direction,
        surface, r, outgoing_power, out_going_direction, store_power);
    
    repack_the_state_to_payload(r.key, outgoing_power,
    outgoing_direction, keep_going);
    
    validate_and_add_photon(surface, hit_pos, store_power,
            ray_direction, t);
}
