void validate_and_add_photon(Surface_attributes surface,
    float3 position_in_world, float3 power,
    float3 incoming_direction, float t)
{
    if (is_in_camera_frustum(position) &&
        is_normal_direction_to_camera(surface.normal))
    {
        uint tile_index = 
            get_tile_index_in_flattened_buffer(position_in_world);
        uint photon_index;
        // Offset in the photon buffer and the indirect argument
        DrawArgumentBuffer.InterlockedAdd(4, 1, photon_index);
        // Photon is packed and stored with correct offset.
        add_photon_to_buffer(position_in_world, power, surface.normal, 
            power, incoming_direction, photon_index, t);
        // Tile-based photon density estimation
        DensityEstimationBuffer.InterlockedAdd(tile_i * 4, 1);
    }
}
