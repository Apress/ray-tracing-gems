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

#include <sutil.h>
#include <optixpp_namespace.h>
#include <optixu/optixu_math_namespace.h>


//------------------------------------------------------------------------------
//
// Implements the Preetham analytic sun/sky model ( Preetham, SIGGRAPH 99 )
//
//------------------------------------------------------------------------------

namespace sutil {


class PreethamSunSky
{
protected:
  typedef optix::float3 float3;
public:
  SUTILAPI PreethamSunSky();

  SUTILAPI void setSunTheta( float sun_theta )            { m_sun_theta = sun_theta; m_dirty = true; }
  SUTILAPI void setSunPhi( float sun_phi)                 { m_sun_phi = sun_phi;     m_dirty = true; }
  SUTILAPI void setTurbidity( float turbidity )           { m_turbidity = turbidity; m_dirty = true; }

  SUTILAPI void setUpDir( const float3& up )       { m_up = up; m_dirty = true; }
  SUTILAPI void setOvercast( float overcast )             { m_overcast = overcast;    }
  
  SUTILAPI float  getSunTheta()                           { return m_sun_theta; }
  SUTILAPI float  getSunPhi()                             { return m_sun_phi;   }
  SUTILAPI float  getTurbidity()                          { return m_turbidity; }

  SUTILAPI float         getOvercast()                    { return m_overcast;              }
  SUTILAPI float3 getUpDir()                       { return m_up;                    }
  SUTILAPI float3 getSunDir()                      { preprocess(); return m_sun_dir; }


  // Query the sun color at current sun position and air turbidity ( kilo-cd / m^2 )
  SUTILAPI float3  sunColor();

  // Query the sky color in a given direction ( kilo-cd / m^2 )
  SUTILAPI float3  skyColor( const float3 & direction, bool CEL = false );
  
  // Sample the solid angle subtended by the sun at its current position
  SUTILAPI float3 sampleSun()const;

  // Set precomputed Preetham model variables on the given context:
  //   c[0-4]          : 
  //   inv_divisor_Yxy :
  //   sun_dir         :
  //   sun_color       :
  //   overcast        :
  //   up              :
  SUTILAPI void setVariables( optix::Context context );


private:
  void          preprocess();
  float3 calculateSunColor();


  // Represents one entry from table 2 in the paper
  struct Datum  
  {
    float wavelength;
    float sun_spectral_radiance;
    float k_o;
    float k_wa;
  };
  
  static const float cie_table[38][4];          // CIE spectral sensitivy curves
  static const Datum data[38];                  // Table2


  // Calculate absorption for a given wavelength of direct sunlight
  static float calculateAbsorption( float sun_theta, // Sun angle from zenith
                                    float m,         // Optical mass of atmosphere
                                    float lambda,    // light wavelength
                                    float turbidity, // atmospheric turbidity
                                    float k_o,       // atten coeff for ozone
                                    float k_wa );    // atten coeff for h2o vapor
  
  // Unit conversion helpers
  static float3 XYZ2rgb( const float3& xyz );
  static float3 Yxy2XYZ( const float3& Yxy );
  static float  rad2deg( float rads );

  // Input parameters
  float  m_sun_theta;
  float  m_sun_phi;
  float  m_turbidity;
  float  m_overcast;
  float3 m_up;
  float3 m_sun_color;
  float3 m_sun_dir;

  // Precomputation results
  bool   m_dirty;
  float3 m_c0;
  float3 m_c1;
  float3 m_c2;
  float3 m_c3;
  float3 m_c4;
  float3 m_inv_divisor_Yxy;
};



  
inline float PreethamSunSky::rad2deg( float rads )
{
  return rads * 180.0f / static_cast<float>( M_PI );
}


inline optix::float3 PreethamSunSky::Yxy2XYZ( const optix::float3& Yxy )
{
  // Avoid division by zero in the transformation.
  if( Yxy.z < 1e-4 )
    return optix::make_float3( 0.0f, 0.0f, 0.0f );

  return optix::make_float3(  Yxy.y * ( Yxy.x / Yxy.z ),
                              Yxy.x,
                              ( 1.0f - Yxy.y - Yxy.z ) * ( Yxy.x / Yxy.z ) );
}


inline optix::float3 PreethamSunSky::XYZ2rgb( const optix::float3& xyz)
{
  const float R = optix::dot( xyz, optix::make_float3(  3.2410f, -1.5374f, -0.4986f ) );
  const float G = optix::dot( xyz, optix::make_float3( -0.9692f,  1.8760f,  0.0416f ) );
  const float B = optix::dot( xyz, optix::make_float3(  0.0556f, -0.2040f,  1.0570f ) );
  return optix::make_float3( R, G, B );
}

} // namespace

