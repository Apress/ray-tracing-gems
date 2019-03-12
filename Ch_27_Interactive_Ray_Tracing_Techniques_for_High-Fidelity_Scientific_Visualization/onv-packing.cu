// This code snippet lists the key functions required to
// implement normal packing and unpacking using
// octahedron normal vector encoding.  The routines
// convert back and forth between normal vectors represented
// as three single-precision floating-point values
// and a single packed 32-bit unsigned integer encoding.
// Many performance optimizations and improvements are possible here,
// but these routines are easy to try out in your own ray tracing engine.},

#include <optixu/optixu_math_namespace.h> // For make_xxx() functions

// Helper routines that implement the floating-point stages of
// octahedron normal vector encoding
static __host__ __device__ __inline__ float3 OctDecode(float2 projected) {
  float3 n = make_float3(projected.x,
                         projected.y,
                         1.0f - (fabsf(projected.x) + fabsf(projected.y)));
  if (n.z < 0.0f) {
    float oldX = n.x;
    n.x = copysignf(1.0f - fabsf(n.y), oldX);
    n.y = copysignf(1.0f - fabsf(oldX), n.y);
  }
  return n;
}

static __host__ __device__ __inline__ float2 OctEncode(float3 n) {
  const float invL1Norm = 1.0f / (fabsf(n.x) + fabsf(n.y) + fabsf(n.z));
  float2 projected;
  if (n.z < 0.0f) {
    projected = 1.0f - make_float2(fabsf(n.y), fabsf(n.x)) * invL1Norm;
    projected.x = copysignf(projected.x, n.x);
    projected.y = copysignf(projected.y, n.y);
  } else {
    projected = make_float2(n.x, n.y) * invL1Norm;
  }
  return projected;
}


// Helper routines to quantize to or invert the quantization from
// packed unsigned integer representations
static __host__ __device__ __inline__ uint convfloat2uint32(float2 f2) {
  f2 = f2 * 0.5f + 0.5f;
  uint packed;
  packed = ((uint) (f2.x * 65535)) | ((uint) (f2.y * 65535) << 16);
  return packed;
}

static __host__ __device__ __inline__ float2 convuint32float2(uint packed) {
  float2 f2;
  f2.x = (float)((packed      ) & 0x0000ffff) / 65535;
  f2.y = (float)((packed >> 16) & 0x0000ffff) / 65535;
  return f2 * 2.0f - 1.0f;
}


// The routines to be called when preparing geometry buffers
// prior to ray tracing and when decoding them on-the-fly during rendering
static __host__ __device__ __inline__ uint packNormal(const float3& normal) {
  float2 octf2 = OctEncode(normal);
  return convfloat2uint32(octf2);
}

static __host__ __device__ __inline__ float3 unpackNormal(uint packed) {
  float2 octf2 = convuint32float2(packed);
  return OctDecode(octf2);
}
