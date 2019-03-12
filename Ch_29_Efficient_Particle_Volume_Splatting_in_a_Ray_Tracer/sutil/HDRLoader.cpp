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

#include "HDRLoader.h"

#include <math.h>
#include <fstream>
#include <iostream>
#include <string>
#include <cstdlib>

//-----------------------------------------------------------------------------
//  
//  HDRLoader class definition
//
//-----------------------------------------------------------------------------

void HDRLoader::getLine( std::ifstream& file_in, std::string& s )
{
  for (;;) {
    if ( !std::getline( file_in, s ) )
      return;
    if(s.empty()) return;
    std::string::size_type index = s.find_first_not_of( "\n\r\t " );
    if ( index != std::string::npos && s[index] != '#' )
      break;
  }
}

namespace {

  // The error class to throw
  struct HDRError {
    std::string Er;
    HDRError(const std::string &st = "HDRLoader error") : Er(st) {}
  };

  union RGBe {
    struct {
      unsigned char r, g, b, e;
    };
    unsigned char v[4];
  };

  inline void RGBEtoFloats(const RGBe &RV, float *FV, float inv_img_exposure)
  {
    if(RV.e == 0)
      FV[0] = FV[1] = FV[2] = 0.0f;
    else {
      const int HDR_EXPON_BIAS = 128;
      float s = (float)ldexp(1.0, (int(RV.e)-(HDR_EXPON_BIAS+8)));
      s *= inv_img_exposure;
      FV[0] = (RV.r + 0.5f)*s;
      FV[1] = (RV.g + 0.5f)*s;
      FV[2] = (RV.b + 0.5f)*s;
    }
  }


  void ReadScanlineNoRLE(std::ifstream &inf, RGBe *RGBEline, const size_t wid)
  {
    inf.read(reinterpret_cast<char *>(RGBEline), wid * sizeof(RGBe));
    if(inf.eof()) throw HDRError("Premature file end in ReadScanlineNoRLE");
  }

  void ReadScanline(std::ifstream &inf, RGBe *RGBEline, const size_t wid)
  {
    const size_t MinLen = 8, MaxLen = 0x7fff;
    if(wid<MinLen || wid>MaxLen) return ReadScanlineNoRLE(inf, RGBEline, wid);
    char c0, c1, c2, c3;
    inf.get(c0);
    inf.get(c1);
    inf.get(c2);
    inf.get(c3);
    if(inf.eof()) throw HDRError("Premature file end in ReadScanline 1");
    if(c0 != 2 || c1 != 2 || (c2&0x80)) {
      inf.putback(c3);
      inf.putback(c2);
      inf.putback(c1);
      inf.putback(c0);
      return ReadScanlineNoRLE(inf, RGBEline, wid); // Found an old-format scanline
    }

    if(size_t(size_t(c2)<<8 | size_t(c3)) != wid) throw HDRError("Scanline width inconsistent");

    // This scanline is RLE.
    for(unsigned int ch=0; ch<4; ch++) {
      for(unsigned int x=0; x<wid; ) {
        unsigned char code;
        inf.get(reinterpret_cast<char &>(code));
        if(inf.eof()) throw HDRError("Premature file end in ReadScanline 2");
        if(code > 0x80) { // RLE span
          char pix;
          inf.get(pix);
          if(inf.eof()) throw HDRError("Premature file end in ReadScanline 3");
          code = code & 0x7f;
          while(code--)
            RGBEline[x++].v[ch] = pix;
        } else { // Arbitrary span
          while(code--) {
            inf.get(reinterpret_cast<char &>(RGBEline[x++].v[ch]));
            if(inf.eof()) throw HDRError("Premature file end in ReadScanline 4");
          }
        }
      }
    }
  }
};

HDRLoader::HDRLoader( const std::string& filename )
: m_nx( 0u ), m_ny( 0u ), m_raster( 0 )
{
  if ( filename.empty() ) return;

  // Open file
  try {
    std::ifstream inf(filename.c_str(), std::ios::binary);

    if(!inf.is_open()) throw HDRError("Couldn't open file " + filename);

    std::string magic, comment;
    float exposure = 1.0f;

    std::getline(inf, magic);
    if(magic != "#?RADIANCE") throw HDRError("File isn't Radiance.");
    for (;;) {
      getLine(inf, comment);

      // VS2010 doesn't let you look at the 0th element of a 0 length string, so this was tripping
      // debug asserts
      if (comment.empty()) break;
      if(comment[0] == '#') continue;

      if(comment.find("FORMAT") != std::string::npos) {
        if(comment != "FORMAT=32-bit_rle_rgbe") throw HDRError("Can only handle RGBe, not XYZe.");
        continue;
      }

      size_t ofs = comment.find("EXPOSURE=");
      if(ofs != std::string::npos) {
        exposure = (float)atof(comment.c_str()+ofs+9);
      }
    }
    
    std::string major, minor;
    inf >> minor >> m_ny >> major >> m_nx;
    if(minor != "-Y" || major != "+X") throw "Can only handle -Y +X ordering";
    if(m_nx <= 0 || m_ny <= 0) throw "Invalid image dimensions";
    getLine(inf, comment); // Read the last newline of the header

    RGBe *RGBERaster = new RGBe[m_nx * m_ny];

    for(unsigned int y=0; y<m_ny; y++) {
      ReadScanline(inf, RGBERaster + m_nx*y, m_nx);
    }

    m_raster = new float[m_nx * m_ny * 4];

    float inv_img_exposure = 1.0f / exposure;
    for(unsigned int i=0; i<m_nx*m_ny; i++) {
      RGBEtoFloats(RGBERaster[i], m_raster + i*4, inv_img_exposure);
    }
    delete[] RGBERaster;
  } catch ( const HDRError& err  ) {
    std::cerr << "HDRLoader( '" << filename << "' ) failed to load file: " << err.Er << '\n';
    delete [] m_raster;
    m_raster = 0;
  }
}


HDRLoader::~HDRLoader()
{
  delete [] m_raster;
}


bool HDRLoader::failed()const
{
  return m_raster == 0;
}


unsigned int HDRLoader::width()const
{
  return m_nx;
}


unsigned int HDRLoader::height()const
{
  return m_ny;
}


float* HDRLoader::raster()const
{
  return m_raster;
}


//-----------------------------------------------------------------------------
//  
//  Utility functions 
//
//-----------------------------------------------------------------------------

optix::TextureSampler loadHDRTexture( optix::Context context,
                                      const std::string& filename,
                                      const optix::float3& default_color )
{
  // Create tex sampler and populate with default values
  optix::TextureSampler sampler = context->createTextureSampler();
  sampler->setWrapMode( 0, RT_WRAP_REPEAT );
  sampler->setWrapMode( 1, RT_WRAP_REPEAT );
  sampler->setWrapMode( 2, RT_WRAP_REPEAT );
  sampler->setIndexingMode( RT_TEXTURE_INDEX_NORMALIZED_COORDINATES );
  sampler->setReadMode( RT_TEXTURE_READ_NORMALIZED_FLOAT );
  sampler->setMaxAnisotropy( 1.0f );
  sampler->setMipLevelCount( 1u );
  sampler->setArraySize( 1u );

  // Read in HDR, set texture buffer to empty buffer if fails
  HDRLoader hdr( filename );
  if ( hdr.failed() ) {

    // Create buffer with single texel set to default_color
    optix::Buffer buffer = context->createBuffer( RT_BUFFER_INPUT, RT_FORMAT_FLOAT4, 1u, 1u );
    float* buffer_data = static_cast<float*>( buffer->map() );
    buffer_data[0] = default_color.x;
    buffer_data[1] = default_color.y;
    buffer_data[2] = default_color.z;
    buffer_data[3] = 1.0f;
    buffer->unmap();

    sampler->setBuffer( 0u, 0u, buffer );
    // Although it would be possible to use nearest filtering here, we chose linear
    // to be consistent with the textures that have been loaded from a file. This
    // allows OptiX to perform some optimizations.
    sampler->setFilteringModes( RT_FILTER_LINEAR, RT_FILTER_LINEAR, RT_FILTER_NONE );

    return sampler;
  }

  const unsigned int nx = hdr.width();
  const unsigned int ny = hdr.height();

  // Create buffer and populate with HDR data
  optix::Buffer buffer = context->createBuffer( RT_BUFFER_INPUT, RT_FORMAT_FLOAT4, nx, ny );
  float* buffer_data = static_cast<float*>( buffer->map() );

  for ( unsigned int i = 0; i < nx; ++i ) {
    for ( unsigned int j = 0; j < ny; ++j ) {

      unsigned int hdr_index = ( (ny-j-1)*nx + i )*4;
      unsigned int buf_index = ( (j     )*nx + i )*4;

      buffer_data[ buf_index + 0 ] = hdr.raster()[ hdr_index + 0 ];
      buffer_data[ buf_index + 1 ] = hdr.raster()[ hdr_index + 1 ];
      buffer_data[ buf_index + 2 ] = hdr.raster()[ hdr_index + 2 ];
      buffer_data[ buf_index + 3 ] = hdr.raster()[ hdr_index + 3 ];
    }
  }

  buffer->unmap();

  sampler->setBuffer( 0u, 0u, buffer );
  sampler->setFilteringModes( RT_FILTER_LINEAR, RT_FILTER_LINEAR, RT_FILTER_NONE );

  return sampler;
}

