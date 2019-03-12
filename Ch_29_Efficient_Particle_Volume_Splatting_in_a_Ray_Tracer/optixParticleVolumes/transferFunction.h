
#pragma once

#include <optixu/optixu_math_namespace.h>

inline __device__ float4 lerp4f(float4 a, float4 b, float c)
{
    return a * (1.f - c) + b * c;
}

inline __device__ float lerp1f(float a, float b, float c)
{
    return a * (1.f - c) + b * c;
}

inline __device__ float4 tf(float v, float t, const int tf_type)
{
  float4 color;

  if (tf_type == 1)
  {
    if (v < .5f)
      color = lerp4f( make_float4(1,1,0,0), make_float4(1,1,1,0.5f), v * 2.f);
    else
      color = lerp4f( make_float4(1,1,1,0.5f), make_float4(1,0,0,1), v * 2.f - 1.f);
  }
  else if (tf_type == 2)
  {
    if (v < .5f)
      color = lerp4f( make_float4(0,0,1,0), make_float4(1,1,1,0.5f), v * 2.f);
    else
      color = lerp4f( make_float4(1,1,1,0.5f), make_float4(1,0,0,1), v * 2.f - 1.f);
  }
  else if (tf_type == 3)
  {
    if (v < .33f)
      color = lerp4f( make_float4(1.f,0.f,1.f,0.f), make_float4(0.f,0.f,1.f,0.33f), (v-0.f) * 3.f);
    else if (v < .66f)
      color = lerp4f( make_float4(0.f,0.f,1.f,.33f), make_float4(0.f,1.f,1.f,0.66f), (v-0.33f) * 3.f);
    else
      color = lerp4f( make_float4(0.f,1.f,0.f,0.5f), make_float4(1.f,1.f,1.f,1.f), (v-0.66f) * 3.f);
  }
  else
  {
    color = lerp4f( make_float4(0,0,1,0), make_float4(1,0,0,1), v);
  }

  //redshift
#if 1
  if (t > 0.f)
  {
    const float alpha = color.w;
    const float r = 1.f - expf(-t);
    if (r < .5f)
      color = lerp4f( make_float4(1,1,1,0), color, r * 2.f);
    else
      color = lerp4f( color, make_float4(1,0,0,1), r * 2.f - 1.f);
    color.w = alpha;
  }
#endif

  return color;

}
