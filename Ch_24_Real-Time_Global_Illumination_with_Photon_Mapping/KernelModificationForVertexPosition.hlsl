kernel_output kernel_modification_for_vertex_position(float3 vertex, 
    float3 n, float3 light, float3 pp_in_view, float ray_length)
{
    kernel_output o;
    float scaling_uniform =  uniform_scaling(pp_in_view,  ray_length);
    
    float3 l = normalize(light);
    float3 cos_alpha = dot(n, vertex);
    float3 projected_v_to_n = cos_alpha * n;
    float cos_theta = saturate(dot(n, l));
    float3 projected_l_to_n = cos_theta * n;
    
    float3 u = normalize(l - projected_l_to_n);
    
    // Equation 24.7
    o.light_shaping_scale = min(1.0f/cos_theta, MAX_SCALING_CONSTANT);
    
    float3 projected_v_to_u = dot(u, vertex) * u;
    float3 projected_v_to_t = vertex - projected_v_to_u;
    projected_v_to_t -= dot(projected_v_to_t, n) * n;
    
    // Equation 24.8
    float3 scaled_u = projected_v_to_u * light_shaping_scale * 
        scaling_Uniform;
    float3 scaled_t = projected_v_to_t * scaling_uniform;
    o.vertex_position = scaled_u + scaled_t +
        (KernelCompress * projected_v_to_n);
    
    o.ellipse_area = PI * o.scaling_uniform  * o.scaling_uniform *
        o.light_shaping_scale;
    
    return o;
}
