//
// CUDA volume path tracing kernel implementation
//

#define _USE_MATH_DEFINES
#include <cmath>
#include "volume_kernel.h"

// 3d vector math utilities.
__device__ inline float3 operator+(const float3& a, const float3& b)
{
    return make_float3(a.x + b.x, a.y + b.y, a.z + b.z);
}
__device__ inline float3 operator-(const float3& a, const float3& b)
{
    return make_float3(a.x - b.x, a.y - b.y, a.z - b.z);
}
__device__ inline float3 operator*(const float3& a, const float s)
{
    return make_float3(a.x * s, a.y * s, a.z * s);
}
__device__ inline float3 operator/(const float3& a, const float s)
{
    return make_float3(a.x / s, a.y / s, a.z / s);
}
__device__ inline void operator+=(float3& a, const float3& b)
{
    a.x += b.x; a.y += b.y; a.z += b.z;
}
__device__ inline void operator*=(float3& a, const float& s)
{
    a.x *= s; a.y *= s; a.z *= s;
}
__device__ inline float3 normalize(const float3 &d)
{
    const float inv_len = 1.0f / sqrtf(d.x * d.x + d.y * d.y + d.z * d.z);
    return make_float3(d.x * inv_len, d.y * inv_len, d.z * inv_len);
}
__device__ inline float dot(const float3 &u, const float3 &v)
{
    return u.x * v.x + u.y * v.y + u.z * v.z;
}

#include <curand_kernel.h>
typedef curandStatePhilox4_32_10_t Rand_state;
#define rand(state) curand_uniform(state)

__device__ inline bool intersect_volume_box(
    float &tmin, const float3 &raypos, const float3 &raydir)
{
    const float x0 = (-0.5f - raypos.x) / raydir.x;
    const float y0 = (-0.5f - raypos.y) / raydir.y;
    const float z0 = (-0.5f - raypos.z) / raydir.z;
    const float x1 = ( 0.5f - raypos.x) / raydir.x;
    const float y1 = ( 0.5f - raypos.y) / raydir.y;
    const float z1 = ( 0.5f - raypos.z) / raydir.z;

    tmin = fmaxf(fmaxf(fmaxf(fminf(z0,z1), fminf(y0,y1)), fminf(x0,x1)), 0.0f);
    const float tmax = fminf(fminf(fmaxf(z0,z1), fmaxf(y0,y1)), fmaxf(x0,x1));
    return (tmin < tmax);
}

__device__ inline bool in_volume(
    const float3 &pos)
{
    return fmaxf(fabsf(pos.x), fmaxf(fabsf(pos.y), fabsf(pos.z))) < 0.5f;
}

__device__ inline float get_extinction(
    const Kernel_params &kernel_params,
    const float3 &p)
{
    if (kernel_params.volume_type == 0) {
        float3 pos = p + make_float3(0.5f, 0.5f, 0.5f);
        const unsigned int steps = 3;
        for (unsigned int i = 0; i < steps; ++i) {
            pos *= 3.0f;
            const int s =
                ((int)pos.x & 1) + ((int)pos.y & 1) + ((int)pos.z & 1);
            if (s >= 2)
                return 0.0f;
        }
        return kernel_params.max_extinction;
    } else {
        const float r = 0.5f * (0.5f - fabsf(p.y));
        const float a = (float)(M_PI * 8.0) * p.y;
        const float dx = (cosf(a) * r - p.x) * 2.0f;
        const float dy = (sinf(a) * r - p.z) * 2.0f;
        return powf(fmaxf((1.0f - dx * dx - dy * dy), 0.0f), 8.0f) * kernel_params.max_extinction;
    }
}

__device__ inline bool sample_interaction(
    Rand_state &rand_state,
    float3 &ray_pos,
    const float3 &ray_dir,
    const Kernel_params &kernel_params)
{
    float t = 0.0f;
    float3 pos;
    do {
        t -= logf(1.0f - rand(&rand_state)) / kernel_params.max_extinction;

        pos = ray_pos + ray_dir * t;
        if (!in_volume(pos))
            return false;
        
    } while (get_extinction(kernel_params, pos) < rand(&rand_state) * kernel_params.max_extinction);

    ray_pos = pos;
    return true;
}

__device__ inline float3 trace_volume(
    Rand_state &rand_state,
    float3 &ray_pos,
    float3 &ray_dir,
    const Kernel_params &kernel_params)
{
    float t0;
    float w = 1.0f;
    if (intersect_volume_box(t0, ray_pos, ray_dir)) {

        ray_pos += ray_dir * t0;

        unsigned int num_interactions = 0;
        while (sample_interaction(rand_state, ray_pos, ray_dir, kernel_params))
        {
            // Is the path length exeeded?
            if (num_interactions++ >= kernel_params.max_interactions)
                return make_float3(0.0f, 0.0f, 0.0f);

            w *= kernel_params.albedo;
            // Russian roulette absorption
            if (w < 0.2f) {
                if (rand(&rand_state) > w * 5.0f) {
                    return make_float3(0.0f, 0.0f, 0.0f);
                }
                w = 0.2f;
            }

            // Sample isotropic phase function.
            const float phi = (float)(2.0 * M_PI) * rand(&rand_state);
            const float cos_theta = 1.0f - 2.0f * rand(&rand_state);
            const float sin_theta = sqrtf(1.0f - cos_theta * cos_theta);
            ray_dir = make_float3(
                cosf(phi) * sin_theta,
                sinf(phi) * sin_theta,
                cos_theta);
        }
    }

    // Lookup environment.
    if (kernel_params.environment_type == 0) {
        const float f = (0.5f + 0.5f * ray_dir.y) * w;
        return make_float3(f, f, f);
    } else {
        const float4 texval = tex2D<float4>(
            kernel_params.env_tex,
            atan2f(ray_dir.z, ray_dir.x) * (float)(0.5 / M_PI) + 0.5f,
            acosf(fmaxf(fminf(ray_dir.y, 1.0f), -1.0f)) * (float)(1.0 / M_PI));
        return make_float3(texval.x * w, texval.y * w, texval.z * w);
    }
}

extern "C" __global__ void volume_rt_kernel(
    const Kernel_params kernel_params)
{
    const unsigned int x = blockIdx.x * blockDim.x + threadIdx.x;
    const unsigned int y = blockIdx.y * blockDim.y + threadIdx.y;
    if (x >= kernel_params.resolution.x || y >= kernel_params.resolution.y)
        return;

    // Initialize pseudorandom number generator (PRNG); assume we need no more than 4096 random numbers.
    const unsigned int idx = y * kernel_params.resolution.x + x;
    Rand_state rand_state;
    curand_init(idx, 0, kernel_params.iteration * 4096, &rand_state);

    // Trace from the pinhole camera.
    const float inv_res_x = 1.0f / (float)kernel_params.resolution.x;
    const float inv_res_y = 1.0f / (float)kernel_params.resolution.y;
    const float pr = (2.0f * ((float)x + rand(&rand_state)) * inv_res_x - 1.0f);
    const float pu = (2.0f * ((float)y + rand(&rand_state)) * inv_res_y - 1.0f);
    const float aspect = (float)kernel_params.resolution.y * inv_res_x;
    float3 ray_pos = kernel_params.cam_pos;
    float3 ray_dir = normalize(
        kernel_params.cam_dir * kernel_params.cam_focal + kernel_params.cam_right * pr + kernel_params.cam_up * aspect * pu);
    const float3 value = trace_volume(rand_state, ray_pos, ray_dir, kernel_params);

    // Accumulate.
    if (kernel_params.iteration == 0)
        kernel_params.accum_buffer[idx] = value;
    else
        kernel_params.accum_buffer[idx] = kernel_params.accum_buffer[idx] +
            (value - kernel_params.accum_buffer[idx]) / (float)(kernel_params.iteration + 1);
    
    // Update display buffer (simple Reinhard tonemapper + gamma).
    float3 val = kernel_params.accum_buffer[idx] * kernel_params.exposure_scale;
    val.x *= (1.0f + val.x * 0.1f) / (1.0f + val.x);
    val.y *= (1.0f + val.y * 0.1f) / (1.0f + val.y);
    val.z *= (1.0f + val.z * 0.1f) / (1.0f + val.z);
    const unsigned int r = (unsigned int)(255.0f *
                  fminf(powf(fmaxf(val.x, 0.0f), (float)(1.0 / 2.2)), 1.0f));
    const unsigned int g = (unsigned int)(255.0f *
                  fminf(powf(fmaxf(val.y, 0.0f), (float)(1.0 / 2.2)), 1.0f));
    const unsigned int b = (unsigned int)(255.0f *
                  fminf(powf(fmaxf(val.z, 0.0f), (float)(1.0 / 2.2)), 1.0f));
    kernel_params.display_buffer[idx] = 0xff000000 | (r << 16) | (g << 8) | b;
}

