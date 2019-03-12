void VS(
    float3 Position : SV_Position,
    uint instanceID : SV_InstanceID,
    out vs_to_ps Output)
{
    unpacked_photon up = unpack_photon(PhotonBuffer[instanceID]);
    float3 photon_position = up.position;
    float3 photon_position_in_view = mul(WorldToViewMatrix,
    float4(photon_position, 1)).xyz;
    kernel_output o = kernel_modification_for_vertex_position(Position,
    up.normal, -up.direction, photon_position_in_view, up.ray_length);
    
    float3 p = pp + o.vertex_position;
    
    Output.Position = mul(WorldToViewClipMatrix, float4(p, 1));
    Output.Power = up.power / o.ellipse_area;
    Output.Direction = -up.direction;
}

[earlydepthstencil]
void PS(
vs_to_ps Input,
out float4 OutputColorXYZAndDirectionX : SV_Target,
out float2 OutputDirectionYZ : SV_Target1)
{
    float depth = DepthTexture[Input.Position.xy];
    float gbuffer_linear_depth = LinearDepth(ViewConstants, depth);
    float kernel_linear_depth = LinearDepth(ViewConstants, 
        Input.Position.z);
    float d = abs(gbuffer_linear_depth - kernel_linear_depth);
    
    clip(d > (KernelCompress * MAX_DEPTH) ? -1 : 1);
    
    float3 power = Input.Power;
    float total_power = dot(power.xyz, float3(1.0f, 1.0f, 1.0f));
    float3 weighted_direction = total_power * Input.Direction;
    
    OutputColorXYZAndDirectionX = float4(power, weighted_direction.x);
    OutputDirectionYZ = weighted_direction.yz;
}
