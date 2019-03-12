float uniform_scaling(float3 pp_in_view, float ray_length)
{
    // Tile-based culling as photon density estimation
    int n_p = load_number_of_photons_in_tile(pp_in_view);
    float r = .1f;
    
    if (layers > .0f)
    {
        // Equation 24.5
        float a_view = pp_in_view.z * pp_in_view.z * TileAreaConstant;
        r = sqrt(a_view / (PI * n_p));
    }
    // Equation 24.6
    float s_d = clamp(r, DYNAMIC_KERNEL_SCALE_MIN, 
        DYNAMIC_KERNEL_SCALE_MAX) * n_tile;
    
    // Equation 24.2
    float s_l = clamp(ray_length / MAX_RAY_LENGTH, .1f, 1.0f);
    return s_d * s_l;
}
