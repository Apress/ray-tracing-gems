/* 
 * Copyright (c) 2016, NVIDIA CORPORATION. All rights reserved.
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

#pragma once 

#include <optixu/optixpp_namespace.h>
#include <optixu/optixu_matrix_namespace.h>
#include <sutilapi.h>

namespace sutil
{

// Camera state for raygen program
class Camera
{
    public:

    // Note: using generic float* rather than float3, to avoid potential link issues.  Some samples have float3
    // in the global namespace and some have it in the optix namespace.
    SUTILAPI Camera( unsigned int width, unsigned int height, 
            const float* camera_eye,
            const float* camera_lookat,
            const float* camera_up,
            optix::Variable eye, optix::Variable u, optix::Variable v, optix::Variable w) 

        : m_width( width ),
        m_height( height ),
        m_camera_eye( optix::make_float3( camera_eye[0], camera_eye[1], camera_eye[2] ) ),
        m_save_camera_lookat( optix::make_float3( camera_lookat[0], camera_lookat[1], camera_lookat[2] ) ),
        m_camera_lookat( optix::make_float3( camera_lookat[0], camera_lookat[1], camera_lookat[2] ) ),
        m_camera_up( optix::make_float3( camera_up[0], camera_up[1], camera_up[2] ) ),
        m_camera_rotate( optix::Matrix4x4::identity() ),
        m_variable_eye( eye ),
        m_variable_u( u ),
        m_variable_v( v ),
        m_variable_w( w )
        {
            apply();
        }

    SUTILAPI void reset_lookat();

    SUTILAPI bool process_mouse( float x, float y, bool left_button_down, bool right_button_down, bool middle_button_down );

    SUTILAPI bool rotate( float dx, float dy );

    SUTILAPI bool resize( unsigned int w, unsigned int h) {
        if ( w == m_width && h == m_height) return false;
        m_width = w;
        m_height = h;
        apply();
        return true;
    }

    SUTILAPI unsigned int width() const  { return m_width; }
    SUTILAPI unsigned int height() const { return m_height; }


    private:

    // Compute derived uvw frame and write to OptiX context
    void apply();

    unsigned int m_width;
    unsigned int m_height;

    optix::float3    m_camera_eye;
    optix::float3    m_camera_lookat;
    const optix::float3 m_save_camera_lookat;
    optix::float3    m_camera_up;
    optix::float3    m_camera_u; // derived
    optix::float3    m_camera_v; // derived
    optix::Matrix4x4 m_camera_rotate;

    // Handles for setting derived values for OptiX context
    optix::Variable  m_variable_eye;
    optix::Variable  m_variable_u;
    optix::Variable  m_variable_v;
    optix::Variable  m_variable_w;

};

} // namespace sutil

