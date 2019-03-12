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

#include <PPMLoader.h>
#include <optixu/optixu_math_namespace.h>
#include <fstream>
#include <iostream>
#include <sstream>

using namespace optix;

//-----------------------------------------------------------------------------
//  
//  PPMLoader class definition
//
//-----------------------------------------------------------------------------

PPMLoader::PPMLoader( const std::string& filename, const bool vflip )
  : m_nx( 0u ), m_ny( 0u ), m_max_val( 0u ), m_raster( 0 ), m_is_ascii(false)
{
  if ( filename.empty() ) return;
  
  size_t pos;
  std::string extension;
  if( (pos = filename.find_last_of( '.' )) != std::string::npos )
    extension = filename.substr( pos );
  if( extension != ".ppm" ) {
    std::cerr << "PPMLoader( '" << filename << "' ) non-ppm file extension given '" << extension << "'" << std::endl;
    return;
  }

  // Open file
  try {
    std::ifstream file_in( filename.c_str(), std::ifstream::in | std::ifstream::binary );
    if ( !file_in ) {
      std::cerr << "PPMLoader( '" << filename << "' ) failed to open file."
                << std::endl;
      return;
    }

    // Check magic number to make sure we have an ascii or binary PPM
    std::string line, magic_number;
    getLine( file_in, line );
    std::istringstream iss1( line );
    iss1 >> magic_number;
    if ( magic_number != "P6" && magic_number != "P3") {
      std::cerr << "PPMLoader( '" << filename << "' ) unknown magic number: "
                << magic_number << ".  Only P3 and P6 supported." << std::endl;
      return;
    }
    if ( magic_number == "P3" ) {
      m_is_ascii = true;
    }

    // width, height
    getLine( file_in, line );
    std::istringstream iss2( line );
    iss2 >> m_nx >> m_ny;

    // max channel value
    getLine( file_in, line );
    std::istringstream iss3( line );
    iss3 >> m_max_val;

    m_raster = new(std::nothrow)  unsigned char[ m_nx*m_ny*3 ];
    if(m_is_ascii) {
      unsigned int num_elements = m_nx*m_ny*3;
      unsigned int count = 0;

      while(count < num_elements) {
        getLine( file_in, line );
        std::istringstream iss(line);

        while(iss.good()) {
          unsigned int c;
          iss >> c;
          m_raster[count++] = static_cast<unsigned char>(c);
        }
      }

    } else {
      file_in.read( reinterpret_cast<char*>( m_raster ), m_nx*m_ny*3 );
    }

    if(vflip) {
      unsigned char *m_raster2 = new(std::nothrow)  unsigned char[ m_nx*m_ny*3 ];
      for(unsigned int y2=m_ny-1, y=0; y<m_ny; y2--, y++) {
        for(unsigned int x=0; x<m_nx*3; x++)
          m_raster2[y2*m_nx*3+x] = m_raster[y*m_nx*3+x];
      }

      delete [] m_raster;
      m_raster = m_raster2;
    }
  } catch ( ... ) {
    std::cerr << "PPMLoader( '" << filename << "' ) failed to load" << std::endl;
    m_raster = 0;
  }
}


PPMLoader::~PPMLoader()
{
  if ( m_raster ) delete[] m_raster;
}


bool PPMLoader::failed() const
{
  return m_raster == 0;
}


unsigned int PPMLoader::width() const
{
  return m_nx;
}


unsigned int PPMLoader::height() const
{
  return m_ny;
}


unsigned char* PPMLoader::raster() const
{
  return m_raster;
}


void PPMLoader::getLine( std::ifstream& file_in, std::string& s )
{
  for (;;) {
    if ( !std::getline( file_in, s ) )
      return;
    std::string::size_type index = s.find_first_not_of( "\n\r\t " );
    if ( index != std::string::npos && s[index] != '#' )
      break;
  }
}

  
//-----------------------------------------------------------------------------
//  
//  Utility functions 
//
//-----------------------------------------------------------------------------

optix::TextureSampler PPMLoader::loadTexture( optix::Context context,
                                              const float3& default_color,
                                              bool linearize_gamma)
{
  // lookup table for sRGB gamma linearization
  static unsigned char srgb2linear[256];
  // filling in a static lookup table for sRGB gamma linearization, standard formula for sRGB
  static bool srgb2linear_init = false;
  if (!srgb2linear_init) {
    srgb2linear_init = true;
    for (int i = 0; i < 256; i++) {
      float cs = i / 255.0f;
      if (cs <= 0.04045f)
        srgb2linear[i] = (unsigned char)(255.0f * cs / 12.92f + 0.5f);
      else
        srgb2linear[i] = (unsigned char)(255.0f * powf((cs + 0.055f)/1.055f, 2.4f) + 0.5f);
    }
  }

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

  if (failed() ) {

    // Create buffer with single texel set to default_color
    optix::Buffer buffer = context->createBuffer( RT_BUFFER_INPUT, RT_FORMAT_UNSIGNED_BYTE4, 1u, 1u );
    unsigned char* buffer_data = static_cast<unsigned char*>( buffer->map() );
    buffer_data[0] = (unsigned char)clamp((int)(default_color.x * 255.0f), 0, 255);
    buffer_data[1] = (unsigned char)clamp((int)(default_color.y * 255.0f), 0, 255);
    buffer_data[2] = (unsigned char)clamp((int)(default_color.z * 255.0f), 0, 255);
    buffer_data[3] = 255;
    buffer->unmap();

    sampler->setBuffer( 0u, 0u, buffer );
    // Although it would be possible to use nearest filtering here, we chose linear
    // to be consistent with the textures that have been loaded from a file. This
    // allows OptiX to perform some optimizations.
    sampler->setFilteringModes( RT_FILTER_LINEAR, RT_FILTER_LINEAR, RT_FILTER_NONE );

    return sampler;
  }

  const unsigned int nx = width();
  const unsigned int ny = height();

  // Create buffer and populate with PPM data
  optix::Buffer buffer = context->createBuffer( RT_BUFFER_INPUT, RT_FORMAT_UNSIGNED_BYTE4, nx, ny );
  unsigned char* buffer_data = static_cast<unsigned char*>( buffer->map() );

  for ( unsigned int i = 0; i < nx; ++i ) {
    for ( unsigned int j = 0; j < ny; ++j ) {

      unsigned int ppm_index = ( (ny-j-1)*nx + i )*3;
      unsigned int buf_index = ( (j     )*nx + i )*4;

      buffer_data[ buf_index + 0 ] = raster()[ ppm_index + 0 ];
      buffer_data[ buf_index + 1 ] = raster()[ ppm_index + 1 ];
      buffer_data[ buf_index + 2 ] = raster()[ ppm_index + 2 ];
      if (linearize_gamma) {
        buffer_data[ buf_index + 0 ] = srgb2linear[buffer_data[ buf_index + 0 ]];
        buffer_data[ buf_index + 1 ] = srgb2linear[buffer_data[ buf_index + 1 ]];
        buffer_data[ buf_index + 2 ] = srgb2linear[buffer_data[ buf_index + 2 ]];
      }
      buffer_data[ buf_index + 3 ] = 255;
    }
  }

  buffer->unmap();

  sampler->setBuffer( 0u, 0u, buffer );
  sampler->setFilteringModes( RT_FILTER_LINEAR, RT_FILTER_LINEAR, RT_FILTER_NONE );

  return sampler;
}

  
//-----------------------------------------------------------------------------
//  
//  Utility functions 
//
//-----------------------------------------------------------------------------

optix::TextureSampler loadPPMTexture( optix::Context context,
                                      const std::string& filename,
                                      const optix::float3& default_color )
{
  PPMLoader ppm( filename );
  return ppm.loadTexture(context, default_color );
}


optix::Buffer loadPPMCubeBuffer( optix::Context context,
                                 const std::vector<std::string>& filenames )
{
  optix::Buffer buffer = context->createBuffer( RT_BUFFER_INPUT | RT_BUFFER_CUBEMAP, RT_FORMAT_UNSIGNED_BYTE4 );
  char* buffer_data = 0;
  for(size_t face = 0; face < filenames.size(); ++face) {
    // Read in HDR, set texture buffer to empty buffer if fails
    PPMLoader ppm( filenames[face] );
    if ( ppm.failed() ) {
      return buffer;
    }

    const unsigned int nx = ppm.width();
    const unsigned int ny = ppm.height();
        
    if(face == 0) {      
      buffer->setSize(nx, ny, filenames.size());
      buffer_data = static_cast<char*>( buffer->map() );
    } else {
      buffer_data += nx * ny * sizeof(char) * 4;
    }

    for ( unsigned int i = 0; i < nx; ++i ) {
      for ( unsigned int j = 0; j < ny; ++j ) {

        //unsigned int ppm_index = ( (ny-j-1)*nx + i )*3;
        unsigned int ppm_index = ( (j     )*nx + i )*3;
        unsigned int buf_index = ( (j     )*nx + i )*4;

        buffer_data[ buf_index + 0 ] = ppm.raster()[ ppm_index + 0 ];
        buffer_data[ buf_index + 1 ] = ppm.raster()[ ppm_index + 1 ];
        buffer_data[ buf_index + 2 ] = ppm.raster()[ ppm_index + 2 ];
        buffer_data[ buf_index + 3 ] = ppm.raster()[ ppm_index + 3 ];
      }
    }

  }
  buffer->unmap();

  return buffer;
}
