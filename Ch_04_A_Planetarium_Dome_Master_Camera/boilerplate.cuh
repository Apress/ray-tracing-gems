//
// Ray Tracing Gems sample code for 
//   "A Planetarium Dome Master Camera"
// 
// This code is a simplified derivative of the 
// C- and CUDA-based implementations in 
// the Tachyon ray tracing engine and the 
// VMD molecular visualization software.
// 
// Questions should be directed to the author
// John E. Stone, developer of Tachyon and VMD 
//
#include <optix.h>
#include <optixu/optixu_math_namespace.h>
#include <optixu/optixu_matrix_namespace.h>
#include <optixu/optixu_aabb_namespace.h>

using namespace optix;

// output image framebuffer, accumulation buffer, and ray launch parameters
rtBuffer<uchar4, 2> framebuffer;
rtBuffer<float4, 2> accumulation_buffer;
rtDeclareVariable(float, accumulation_normalization_factor, , );
rtDeclareVariable(uint2, launch_index, rtLaunchIndex, );
rtDeclareVariable(uint2, launch_dim, rtLaunchDim, );

rtDeclareVariable(unsigned int, progressiveSubframeIndex, rtSubframeIndex, );
rtDeclareVariable(unsigned int, accumCount, , );

static int __inline__ __device__ subframe_count() {
  return (accumCount + progressiveSubframeIndex);
}

// epsilon value to use to avoid self-intersection
rtDeclareVariable(float, scene_epsilon, , );

// max ray recursion depth
rtDeclareVariable(int, max_depth, , );

// max number of transparent surfaces (max_trans <= max_depth)
rtDeclareVariable(int, max_trans, , );

// camera parameters
rtDeclareVariable(float,  cam_zoom, , );
rtDeclareVariable(float3, cam_pos, , );
rtDeclareVariable(float3, cam_U, , );
rtDeclareVariable(float3, cam_V, , );
rtDeclareVariable(float3, cam_W, , );

// stereoscopic camera parameters
rtDeclareVariable(float,  cam_stereo_eyesep, , );

// camera depth-of-field parameters
rtDeclareVariable(float,  cam_dof_aperture_rad, , );
rtDeclareVariable(float,  cam_dof_focal_dist, , );

rtDeclareVariable(int, aa_samples, , );

// various shading related per-ray state
rtDeclareVariable(unsigned int, radiance_ray_type, , );
rtDeclareVariable(unsigned int, shadow_ray_type, , );
rtDeclareVariable(rtObject, root_object, , );
rtDeclareVariable(rtObject, root_shadower, , );


//
// Various random number routines adapted from Tachyon
//
#define MYRT_RAND_MAX 4294967296.0f       /* Max random value from rt_rand  */
#define MYRT_RAND_MAX_INV .00000000023283064365f   /* normalize rt_rand  */

//
// Quick and dirty 32-bit LCG random number generator [Fishman 1990]:
//   A=1099087573 B=0 M=2^32
//   Period: 10^9
// Fastest gun in the west, but fails many tests after 10^6 samples,
// and fails all statistics tests after 10^7 samples.
// It fares better than the Numerical Recipes LCG.  This is the fastest
// power of two rand, and has the best multiplier for 2^32, found by
// brute force[Fishman 1990].  Test results:
//   http://www.iro.umontreal.ca/~lecuyer/myftp/papers/testu01.pdf
//   http://www.shadlen.org/ichbin/random/
//
static __device__ __inline__
unsigned int myrt_rand(unsigned int &idum) {
  // on machines where int is 32-bits, no need to mask
  idum *= 1099087573;
  return idum;
}


//
// TEA, a tiny encryption algorithm.
// D. Wheeler and R. Needham, 2nd Intl. Workshop Fast Software Encryption,
// LNCS, pp. 363-366, 1994.
//
// GPU Random Numbers via the Tiny Encryption Algorithm
// F. Zafar, M. Olano, and A. Curtis.
// HPG '10 Proceedings of the Conference on High Performance Graphics,
// pp. 133-141, 2010.
//
template<unsigned int N> static __host__ __device__ __inline__
unsigned int tea(unsigned int val0, unsigned int val1) {
  unsigned int v0 = val0;
  unsigned int v1 = val1;
  unsigned int s0 = 0;

  for( unsigned int n = 0; n < N; n++ ) {
    s0 += 0x9e3779b9;
    v0 += ((v1<<4)+0xa341316c)^(v1+s0)^((v1>>5)+0xc8013ea4);
    v1 += ((v0<<4)+0xad90777d)^(v0+s0)^((v0>>5)+0x7e95761e);
  }

  return v0;
}


// Generate an offset to jitter AA samples in the image plane, adapted
// from the code in Tachyon
static __device__ __inline__
void jitter_offset2f(unsigned int &pval, float2 &xy) {
  xy.x = (myrt_rand(pval) * MYRT_RAND_MAX_INV) - 0.5f;
  xy.y = (myrt_rand(pval) * MYRT_RAND_MAX_INV) - 0.5f;
}


// Generate an offset to jitter DoF samples in the Circle of Confusion,
// adapted from a C implementation in Tachyon
static __device__ __inline__
void jitter_disc2f(unsigned int &pval, float2 &xy, float radius) {
#if 1
  // Since the GPU RT sometimes uses super cheap LCG RNGs,
  // it is best to avoid using sample picking, which can fail if
  // we use a multiply-only RNG and we hit a zero in the PRN sequence.
  // The special functions are slow, but have bounded runtime and
  // branch divergence.
  float   r=(myrt_rand(pval) * MYRT_RAND_MAX_INV);
  float phi=(myrt_rand(pval) * MYRT_RAND_MAX_INV) * 2.0f * M_PIf;
  __sincosf(phi, &xy.x, &xy.y); // fast approximation
  xy *= sqrtf(r) * radius;
#else
  // Pick uniform samples that fall within the disc --
  // this scheme can hang in an endless loop if a poor quality
  // RNG is used and it gets stuck in a short PRN sub-sequence
  do {
    xy.x = 2.0f * (myrt_rand(pval) * MYRT_RAND_MAX_INV) - 1.0f;
    xy.y = 2.0f * (myrt_rand(pval) * MYRT_RAND_MAX_INV) - 1.0f;
  } while ((xy.x*xy.x + xy.y*xy.y) > 1.0f);
  xy *= radius;
#endif
}



//
// CUDA device function for computing the new ray origin
// and ray direction, given the radius of the circle of confusion disc,
// and an orthonormal basis for each ray.
//
static __device__ __inline__
void dof_ray(const float3 &ray_origin_orig, float3 &ray_origin,
             const float3 &ray_direction_orig, float3 &ray_direction,
             unsigned int &randseed, const float3 &up, const float3 &right) {
  float3 focuspoint = ray_origin_orig + ray_direction_orig * cam_dof_focal_dist;
  float2 dofjxy;
  jitter_disc2f(randseed, dofjxy, cam_dof_aperture_rad);
  ray_origin = ray_origin_orig + dofjxy.x*right + dofjxy.y*up;
  ray_direction = normalize(focuspoint - ray_origin);
}


//
// Ray gen accumulation buffer helper routines
//
static void __inline__ __device__ accumulate_color(float3 &col,
                                                   float alpha = 1.0f) {
//  if (progressive_enabled) {
    col *= accumulation_normalization_factor;
    alpha *= accumulation_normalization_factor;

    // for optix-vca progressive mode accumulation is handled in server code
    accumulation_buffer[launch_index]  = make_float4(col, alpha);
//  } else {
//    // For batch mode we accumulate ourselves
//    accumulation_buffer[launch_index] += make_float4(col, alpha);
//  }
}



struct PerRayData_radiance {
  float3 result;     // final shaded surface color
  float alpha;       // alpha value to back-propagate to framebuffer
  float importance;  // importance of recursive ray tree
  int depth;         // current recursion depth
  int transcnt;      // transmission ray surface count/depth
};


