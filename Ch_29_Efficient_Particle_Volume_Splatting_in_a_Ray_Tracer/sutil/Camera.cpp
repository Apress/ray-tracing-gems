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

#include "Camera.h"
#include "Arcball.h"
#include "sutil.h"

using namespace optix;

//------------------------------------------------------------------------------
//
// Camera implementation
//
//------------------------------------------------------------------------------
//
// Compute derived uvw frame and write to OptiX context
void sutil::Camera::apply( )
{
    const float vfov  = 45.0f;
    const float aspect_ratio = static_cast<float>(m_width) /
        static_cast<float>(m_height);

    float3 camera_w;
    sutil::calculateCameraVariables(
            m_camera_eye, m_camera_lookat, m_camera_up, vfov, aspect_ratio,
            m_camera_u, m_camera_v, camera_w, /*fov_is_vertical*/ true );

    const Matrix4x4 frame = Matrix4x4::fromBasis(
            normalize( m_camera_u ),
            normalize( m_camera_v ),
            normalize( -camera_w ),
            m_camera_lookat);
    const Matrix4x4 frame_inv = frame.inverse();
    // Apply camera rotation twice to match old SDK behavior
    const Matrix4x4 trans   = frame*m_camera_rotate*m_camera_rotate*frame_inv;

    m_camera_eye    = make_float3( trans*make_float4( m_camera_eye,    1.0f ) );
    m_camera_lookat = make_float3( trans*make_float4( m_camera_lookat, 1.0f ) );
    m_camera_up     = make_float3( trans*make_float4( m_camera_up,     0.0f ) );

    sutil::calculateCameraVariables(
            m_camera_eye, m_camera_lookat, m_camera_up, vfov, aspect_ratio,
            m_camera_u, m_camera_v, camera_w, true );

    m_camera_rotate = Matrix4x4::identity();

    // Write variables to OptiX context
    m_variable_eye->setFloat( m_camera_eye );
    m_variable_u->setFloat( m_camera_u );
    m_variable_v->setFloat( m_camera_v );
    m_variable_w->setFloat( camera_w );
}

void sutil::Camera::reset_lookat()
{
    const float3 translation = m_save_camera_lookat - m_camera_lookat;
    m_camera_eye += translation;
    m_camera_lookat = m_save_camera_lookat;
    apply();
}

bool sutil::Camera::process_mouse( float x, float y, bool left_button_down, bool right_button_down, bool middle_button_down )
{
    static sutil::Arcball arcball;
    static float2 mouse_prev_pos = make_float2( 0.0f, 0.0f );
    static bool   have_mouse_prev_pos = false;

    // No action if mouse did not move
    if (( mouse_prev_pos.x == x && mouse_prev_pos.y == y) ) return false;

    bool dirty = false;

    if ( left_button_down || right_button_down || middle_button_down ) {
        if ( have_mouse_prev_pos) {
            if ( left_button_down ) {

                const float2 from = { mouse_prev_pos.x, mouse_prev_pos.y };
                const float2 to   = { x, y };

                const float2 a = { from.x / m_width, from.y / m_height };
                const float2 b = { to.x   / m_width, to.y   / m_height };

                m_camera_rotate = arcball.rotate( b, a );

            } else if ( right_button_down ) {
                const float dx = ( x - mouse_prev_pos.x ) / m_width;
                const float dy = ( y - mouse_prev_pos.y ) / m_height;
                const float dmax = fabsf( dx ) > fabs( dy ) ? dx : dy;
                const float scale = fminf( dmax, 0.9f );

                m_camera_eye = m_camera_eye + (m_camera_lookat - m_camera_eye)*scale;

            } else if ( middle_button_down ) {
                const float dx = ( x - mouse_prev_pos.x ) / m_width;
                const float dy = ( y - mouse_prev_pos.y ) / m_height;

                float3 translation = -dx*m_camera_u + dy*m_camera_v;
                m_camera_eye    = m_camera_eye + translation;
                m_camera_lookat = m_camera_lookat + translation;
            }

            apply();
            dirty = true;

        }

        have_mouse_prev_pos = true;
        mouse_prev_pos.x = x;
        mouse_prev_pos.y = y;

    } else {
        have_mouse_prev_pos = false;
    }

    return dirty;
}

bool sutil::Camera::rotate( float dx, float dy )
{
    static sutil::Arcball arcball;

    const float2 from = make_float2(m_width * .5f, m_height * .5f);
    const float2 to   = make_float2(from.x + dx, from.y + dy);

    const float2 a = { from.x / m_width, from.y / m_height };
    const float2 b = { to.x   / m_width, to.y   / m_height };

    m_camera_rotate = arcball.rotate( b, a );
    apply();

    return true;
}


