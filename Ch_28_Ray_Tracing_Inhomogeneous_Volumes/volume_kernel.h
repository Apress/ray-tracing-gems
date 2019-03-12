//
// CUDA volume path tracing kernel interface
//

struct Kernel_params {
    // Display
    uint2 resolution;
    float exposure_scale;
    unsigned int *display_buffer;

    // Progressive rendering state
    unsigned int iteration;
    float3 *accum_buffer;

    // Limit on path length
    unsigned int max_interactions;

    // Camera
    float3 cam_pos;
    float3 cam_dir;
    float3 cam_right;
    float3 cam_up;
    float  cam_focal;

    // Environment
    unsigned int environment_type;
    cudaTextureObject_t env_tex;

    // Volume definition
    unsigned int volume_type;
    float max_extinction;
    float albedo; // sigma / kappa
};

extern "C" __global__ void volume_rt_kernel(
   const Kernel_params kernel_params);

