/* 
* Copyright (c) 2018, NVIDIA CORPORATION. All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions
* are met:
*  * Redistributions of source code must retain the above copyright
*    notice, this list of conditions and the following disclaimer.
*  * Redistributions in binary form must reproduce the above copyright
*    notice, this list of conditions and the following disclaimer in the
*    documentation and/or other materials provided with the distribution.
*  * Neither the name of NVIDIA CORPORATION nor the names of its
*    contributors may be used to endorse or promote products derived
*    from this software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
* EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
* PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
* CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
* EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
* PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
* PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
* OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
* OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <optix.h>
#include <optixu/optixu_math_namespace.h>
#include <optixu/optixu_aabb_namespace.h>

using namespace optix;

rtBuffer<float4>    positions_buffer;

rtDeclareVariable(float2,       particle_rbf,    attribute particle_rbf, );
rtDeclareVariable(optix::Ray,   ray,                rtCurrentRay, );
rtDeclareVariable(float,        fixed_radius, ,  );


RT_PROGRAM void particle_intersect( int primIdx )
{
    const float4 pos = positions_buffer[primIdx];
    const float3 pos3 = make_float3(pos.x, pos.y, pos.z);
    const float t = length(pos3 - ray.origin);
    const float3 samplePos = ray.origin + ray.direction * t;

    if( (length(pos3 - samplePos) < fixed_radius) && rtPotentialIntersection(t) )
    {
      particle_rbf.x = t;
      particle_rbf.y = __int_as_float(primIdx);
      rtReportIntersection( 0 );
    }
}

//for accel build
RT_PROGRAM void particle_bounds( int primIdx, float result[6] )
{
    const float4 position = positions_buffer[ primIdx ];
    const float radius = fixed_radius;

    optix::Aabb *aabb = (optix::Aabb *) result;

    aabb->m_min.x = position.x - radius;
    aabb->m_min.y = position.y - radius;
    aabb->m_min.z = position.z - radius;

    aabb->m_max.x = position.x + radius;
    aabb->m_max.y = position.y + radius;
    aabb->m_max.z = position.z + radius;
}

