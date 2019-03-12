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
#include <optixu/optixu_aabb_namespace.h>
#include <vector>

#include "sutilapi.h"


// Default catch block
#define SUTIL_CATCH( ctx ) catch( sutil::APIError& e ) {           \
    sutil::handleError( ctx, e.code, e.file.c_str(), e.line );     \
  }                                                                \
  catch( std::exception& e ) {                                     \
    sutil::reportErrorMessage( e.what() );                         \
    exit(1);                                                       \
  }

// Error check/report helper for users of the C API
#define RT_CHECK_ERROR( func )                                     \
  do {                                                             \
    RTresult code = func;                                          \
    if( code != RT_SUCCESS )                                       \
      throw sutil::APIError( code, __FILE__, __LINE__ );           \
  } while(0)


struct GLFWwindow;

namespace sutil
{

// Exeption to be thrown by RT_CHECK_ERROR macro
struct APIError
{   
    APIError( RTresult c, const std::string& f, int l )
        : code( c ), file( f ), line( l ) {}
    RTresult     code;
    std::string  file;
    int          line;
};

// Display error message
void SUTILAPI reportErrorMessage(
        const char* message);               // Error mssg to be displayed

// Queries provided RTcontext for last error message, displays it, then exits.
void SUTILAPI handleError(
        RTcontext context,                  // Context associated with the error
        RTresult code,                      // Code returned by OptiX API call
        const char* file,                   // Filename for error reporting
        int line);                          // File lineno for error reporting

// Query top level samples directory.
// The pointer returned may point to a static array.
SUTILAPI const char* samplesDir();

// Query directory containing PTX files for compiled sample CUDA files.
// The pointer returned may point to a static array.
SUTILAPI const char* samplesPTXDir();

// Create an output buffer with given specifications
optix::Buffer SUTILAPI createOutputBuffer(
        optix::Context context,             // optix context
        RTformat format,                    // Pixel format (must be ubyte4 for pbo)
        unsigned width,                     // Buffer width
        unsigned height,                    // Buffer height
        bool use_pbo );                     // Use GL interop PBO

// Resize a Buffer and its underlying GLBO if necessary
void SUTILAPI resizeBuffer(
        optix::Buffer buffer,               // Buffer to be modified
        unsigned width,                     // New buffer width
        unsigned height );                  // New buffer height
 
 // Create ground plane for scene with +Y as up direction
optix::GeometryInstance SUTILAPI createOptiXGroundPlane( optix::Context context,
                                                         const std::string& parallelogram_ptx, 
                                                         const optix::Aabb& scene_aabb,
                                                         optix::Material material,
                                                         float scale );

// Initialize GLFW.  Should be called before any GLFW display functions.
// Returns the root GLFWwindow.
SUTILAPI GLFWwindow* initGLFW();

// Create GLFW window and display contents of the buffer.
void SUTILAPI displayBufferGLFW(
        const char* window_title,           // Window title
        optix::Buffer buffer);              // Buffer to be displayed

// Create GLFW window and display contents of the buffer (C API version).
void SUTILAPI displayBufferGLFW(
        const char* window_title,           // Window title
        RTbuffer buffer);                   // Buffer to be displayed

// Write the contents of the Buffer to an image file with type based on extension
void SUTILAPI writeBufferToFile(
        const char* filename,               // Image file to be created
        optix::Buffer buffer);              // Buffer to be displayed

// Write the contents of the Buffer to an image file with type based on extension (C API)
void SUTILAPI writeBufferToFile(
        const char* filename,               // Image file to be created
        RTbuffer buffer);                   // Buffer to be displayed


// Display contents of buffer, where the OpenGL context is managed by caller.
void SUTILAPI displayBufferGL(
        optix::Buffer buffer ); // Buffer to be displayed
        
// Display frames per second, where the OpenGL context
// is managed by the caller.
void SUTILAPI displayFps(
        unsigned total_frame_count );    // total frame count

// Create on OptiX TextureSampler for the given image file.  If the filename is
// empty or if loading the file fails, return 1x1 texture with default color.
optix::TextureSampler SUTILAPI loadTexture(
        optix::Context context,             // Context used for object creation 
        const std::string& filename,        // File to load
        optix::float3 default_color);       // Default color in case of file failure


// Creates a Buffer object for the given cubemap files.
optix::Buffer SUTILAPI loadCubeBuffer(
        optix::Context context,             // Context used for object creation
        const std::vector<std::string>& filenames ); // Files to be loaded 


// Calculate appropriate U,V,W for pinhole_camera shader.
void SUTILAPI calculateCameraVariables(
        optix::float3 eye,                  // Camera eye position
        optix::float3 lookat,               // Point in scene camera looks at
        optix::float3 up,                   // Up direction
        float  fov,                         // Horizontal or vertical field of view (assumed horizontal, see boolean below)
        float  aspect_ratio,                // Pixel aspect ratio (width/height)
        optix::float3& U,                   // [out] U coord for camera program
        optix::float3& V,                   // [out] V coord for camera program
        optix::float3& W,                   // [out] W coord for camera program
		bool fov_is_vertical = false );

// Blocking sleep call
void SUTILAPI sleep(
        int seconds );                      // Number of seconds to sleep

// Parse the string of the form <width>x<height> and return numeric values.
void SUTILAPI parseDimensions(
        const char* arg,                    // String of form <width>x<height>
        int& width,                         // [out] width
        int& height );                      // [in]  height

// Get current time in seconds for benchmarking/timing purposes.
double SUTILAPI currentTime();

} // end namespace sutil

