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


// Note: wglew.h has to be included before sutil.h on Windows
#ifndef  __APPLE__
#  include <GL/glew.h>
#  if defined(_WIN32)
#    include <GL/wglew.h>
#  endif
#endif

#include <GLFW/glfw3.h>

#include <sutil/sutil.h>
#include <sutil/HDRLoader.h>
#include <sutil/PPMLoader.h>
#include <sampleConfig.h>
#include <sutil/stb/stb_image_write.h>

#include <imgui/imgui.h>
#include <imgui/imgui_impl_glfw_gl2.h>

#include <optixu/optixu_math_namespace.h>

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <fstream>
#include <stdint.h>

#if defined(_WIN32)
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN 1
#  endif
#  include<windows.h>
#  include<mmsystem.h>
#else // Apple and Linux both use this 
#  include<sys/time.h>
#  include <unistd.h>
#  include <dirent.h>
#endif


using namespace optix;


namespace
{

// Global variables for GLUT display functions
RTcontext   g_context           = 0;
RTbuffer    g_image_buffer      = 0;
GLFWwindow* g_window            = 0; 
bool        g_glfw_initialized  = false;


void errorCallback(int error, const char* description)                   
{                                                                                
    std::cerr << "GLFW Error " << error << ": " << description << std::endl;           
}


void keyCallback( GLFWwindow* /*window*/, int key, int /*scancode*/, int action, int /*mods*/ )
{
    if( action == GLFW_PRESS )
    {
        switch( key )
        {
            case GLFW_KEY_Q: // esc
            case GLFW_KEY_ESCAPE:
                if( g_context )
                    rtContextDestroy( g_context );

                ImGui_ImplGlfwGL2_Shutdown();
                ImGui::DestroyContext();
                if( g_window )
                    glfwDestroyWindow( g_window );
                glfwTerminate();

                exit(EXIT_SUCCESS);
        }
    }
}


void display()
{
    RTsize buffer_width, buffer_height;
    RT_CHECK_ERROR( rtBufferGetSize2D( g_image_buffer, &buffer_width, &buffer_height) );
    const GLsizei width  = static_cast<GLsizei>(buffer_width);
    const GLsizei height = static_cast<GLsizei>(buffer_height);

    RTformat buffer_format;
    RT_CHECK_ERROR( rtBufferGetFormat( g_image_buffer, &buffer_format ) );

    GLenum gl_data_type;
    GLenum gl_format;

    switch (buffer_format) {
        case RT_FORMAT_UNSIGNED_BYTE4:
            gl_data_type = GL_UNSIGNED_BYTE;
            gl_format    = GL_BGRA;
            break;

        case RT_FORMAT_FLOAT:
            gl_data_type = GL_FLOAT;
            gl_format    = GL_LUMINANCE;
            break;

        case RT_FORMAT_FLOAT3:
            gl_data_type = GL_FLOAT;
            gl_format    = GL_RGB;
            break;

        case RT_FORMAT_FLOAT4:
            gl_data_type = GL_FLOAT;
            gl_format    = GL_RGBA;
            break;

        default:
            fprintf(stderr, "Unrecognized buffer data type or format.\n");
            exit(2);
            break;
    }

    GLvoid* imageData = 0;
    RT_CHECK_ERROR( rtBufferMap( g_image_buffer, &imageData ) );

    glDrawPixels(width, height, gl_format, gl_data_type, imageData);  // Using default glPixelStore unpack alignment of 4.

    // Now unmap the buffer
    RT_CHECK_ERROR( rtBufferUnmap( g_image_buffer ) );

    glfwSwapBuffers( g_window );
}


void checkBuffer( RTbuffer buffer )
{
    // Check to see if the buffer is two dimensional
    unsigned int dimensionality;
    RT_CHECK_ERROR( rtBufferGetDimensionality(buffer, &dimensionality) );
    if (2 != dimensionality)
        throw Exception( "Attempting to display non-2D buffer" );

    // Check to see if the buffer is of type float{1,3,4} or uchar4
    RTformat format;
    RT_CHECK_ERROR( rtBufferGetFormat(buffer, &format) );
    if( RT_FORMAT_FLOAT  != format && 
            RT_FORMAT_FLOAT4 != format &&
            RT_FORMAT_FLOAT3 != format &&
            RT_FORMAT_UNSIGNED_BYTE4 != format )
        throw Exception( "Attempting to diaplay buffer with format not float, float3, float4, or uchar4");
}


void SavePPM(const unsigned char *Pix, const char *fname, int wid, int hgt, int chan)
{
    if( Pix==NULL || wid < 1 || hgt < 1 )
        throw Exception( "Image is ill-formed. Not saving" );

    if( chan != 1 && chan != 3 && chan != 4 )
        throw Exception( "Attempting to save image with channel count != 1, 3, or 4.");

    std::ofstream OutFile(fname, std::ios::out | std::ios::binary);
    if(!OutFile.is_open())
        throw Exception( "Could not open file for SavePPM" );

    bool is_float = false;
    OutFile << 'P';
    OutFile << ((chan==1 ? (is_float?'Z':'5') : (chan==3 ? (is_float?'7':'6') : '8'))) << std::endl;
    OutFile << wid << " " << hgt << std::endl << 255 << std::endl;

    OutFile.write(reinterpret_cast<char*>(const_cast<unsigned char*>( Pix )), wid * hgt * chan * (is_float ? 4 : 1));

    OutFile.close();
}


bool dirExists( const char* path )
{
#if defined(_WIN32)
    DWORD attrib = GetFileAttributes( path );
    return (attrib != INVALID_FILE_ATTRIBUTES) && (attrib & FILE_ATTRIBUTE_DIRECTORY);
#else
    DIR* dir = opendir( path );
    if( dir == NULL )
        return false;
    
    closedir(dir);
    return true;
#endif
}

} // end anonymous namespace


void sutil::reportErrorMessage( const char* message )
{
    std::cerr << "OptiX Error: '" << message << "'\n";
#if defined(_WIN32) && defined(RELEASE_PUBLIC)
    {
        char s[2048];
        sprintf( s, "OptiX Error: %s", message );
        MessageBox( 0, s, "OptiX Error", MB_OK|MB_ICONWARNING|MB_SYSTEMMODAL );
    }
#endif
}


void sutil::handleError( RTcontext context, RTresult code, const char* file,
        int line)
{
    const char* message;
    char s[2048];
    rtContextGetErrorString(context, code, &message);
    sprintf(s, "%s\n(%s:%d)", message, file, line);
    reportErrorMessage( s );
}


const char* sutil::samplesDir()
{
    static char s[512];

    // Allow for overrides.
    const char* dir = getenv( "OPTIX_SAMPLES_SDK_DIR" );
    if (dir) {
        strcpy(s, dir);
        return s;
    }

    // Return hardcoded path if it exists.
    if( dirExists( SAMPLES_DIR ) )
        return SAMPLES_DIR;

    // Last resort.
    return ".";
}


const char* sutil::samplesPTXDir()
{
    static char s[512];

    // Allow for overrides.
    const char* dir = getenv( "OPTIX_SAMPLES_SDK_PTX_DIR" );
    if (dir) {
        strcpy(s, dir);
        return s;
    }

    // Return hardcoded path if it exists.
    if( dirExists(SAMPLES_PTX_DIR) )
        return SAMPLES_PTX_DIR;

    // Last resort.
    return ".";
}


optix::Buffer sutil::createOutputBuffer(
        optix::Context context,
        RTformat format,
        unsigned width,
        unsigned height,
        bool use_pbo )
{
    
    optix::Buffer buffer;
    if( use_pbo )
    {
        // First allocate the memory for the GL buffer, then attach it to OptiX.

        // Assume ubyte4 or float4 for now
        unsigned int elmt_size = format == RT_FORMAT_UNSIGNED_BYTE4 ?  4 : 16;

        GLuint vbo = 0;
        glGenBuffers( 1, &vbo );
        glBindBuffer( GL_ARRAY_BUFFER, vbo );
        glBufferData( GL_ARRAY_BUFFER, elmt_size * width * height, 0, GL_STREAM_DRAW);
        glBindBuffer( GL_ARRAY_BUFFER, 0 );

        buffer = context->createBufferFromGLBO(RT_BUFFER_OUTPUT, vbo);
        buffer->setFormat( format );
        buffer->setSize( width, height );
    }
    else
    {
        buffer = context->createBuffer( RT_BUFFER_OUTPUT, format, width, height );
    }

    return buffer;
}


void sutil::resizeBuffer( optix::Buffer buffer, unsigned width, unsigned height )
{
    buffer->setSize( width, height );

    // Check if we have a GL interop display buffer
    const unsigned pboId = buffer->getGLBOId();
    if( pboId )
    {
        buffer->unregisterGLBuffer();
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pboId );
        glBufferData(GL_PIXEL_UNPACK_BUFFER, buffer->getElementSize() * width * height, 0, GL_STREAM_DRAW);
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
        buffer->registerGLBuffer();
    }
}


optix::GeometryInstance sutil::createOptiXGroundPlane( optix::Context context,
                                               const std::string& parallelogram_ptx, 
                                               const optix::Aabb& aabb,
                                               optix::Material material,
                                               float scale )
{
    optix::Geometry parallelogram = context->createGeometry();
    parallelogram->setPrimitiveCount( 1u );
    parallelogram->setBoundingBoxProgram( context->createProgramFromPTXFile( parallelogram_ptx, "bounds" ) );
    parallelogram->setIntersectionProgram( context->createProgramFromPTXFile( parallelogram_ptx, "intersect" ) );
    const float extent = scale*fmaxf( aabb.extent( 0 ), aabb.extent( 2 ) );
    const float3 anchor = make_float3( aabb.center(0) - 0.5f*extent, aabb.m_min.y - 0.001f*aabb.extent( 1 ), aabb.center(2) - 0.5f*extent );
    float3 v1 = make_float3( 0.0f, 0.0f, extent );
    float3 v2 = make_float3( extent, 0.0f, 0.0f );
    const float3 normal = normalize( cross( v1, v2 ) );
    float d = dot( normal, anchor );
    v1 *= 1.0f / dot( v1, v1 );
    v2 *= 1.0f / dot( v2, v2 );
    float4 plane = make_float4( normal, d );
    parallelogram["plane"]->setFloat( plane );
    parallelogram["v1"]->setFloat( v1 );
    parallelogram["v2"]->setFloat( v2 );
    parallelogram["anchor"]->setFloat( anchor );

    optix::GeometryInstance instance = context->createGeometryInstance( parallelogram, &material, &material + 1 );
    return instance;
}


GLFWwindow* sutil::initGLFW()
{
    // Initialize GLFW
    if( !glfwInit() )
        throw Exception( "glfwInit failed.");

    glfwSetErrorCallback( errorCallback );

    g_window = glfwCreateWindow(100, 100, "", NULL, NULL);
    if (!g_window)
        throw Exception( "GLFW window or GL context creation failed.");
    glfwMakeContextCurrent( g_window );
    glfwSetWindowPos( g_window, 100, 100 );

    glfwSetKeyCallback( g_window, keyCallback );

    g_glfw_initialized = true;
    
    ImGui::CreateContext();
    ImGui_ImplGlfwGL2_Init( g_window, /*install callbacks*/ true );

    return g_window;
}


void sutil::displayBufferGLFW( const char* window_title, Buffer buffer )
{
    displayBufferGLFW(window_title, buffer->get() );
}


void sutil::displayBufferGLFW( const char* window_title, RTbuffer buffer )
{
    if( !g_glfw_initialized )
        throw Exception( "displayGLFWWindow called before initGLFW.");


    checkBuffer(buffer);
    g_image_buffer = buffer;

    RTsize buffer_width, buffer_height;
    RT_CHECK_ERROR( rtBufferGetSize2D(buffer, &buffer_width, &buffer_height ) );

    GLsizei width  = static_cast<int>( buffer_width );
    GLsizei height = static_cast<int>( buffer_height );
    
    glfwSetWindowTitle( g_window, window_title );
    glfwSetWindowSize( g_window, width, height );

    // Init state
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(0, width, 0, height);
    
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    
    while( !glfwWindowShouldClose( g_window ) )
    {
        glfwPollEvents();                                                        
        display();
    }

    if( g_context )
        rtContextDestroy( g_context );
    
    ImGui_ImplGlfwGL2_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow( g_window );
    glfwTerminate();
}


void sutil::writeBufferToFile( const char* filename, Buffer buffer)
{
    writeBufferToFile( filename, buffer->get() );
}


void sutil::writeBufferToFile( const char* filename, RTbuffer buffer)
{
    GLsizei width, height;
    RTsize buffer_width, buffer_height;

    GLvoid* imageData;
    RT_CHECK_ERROR( rtBufferMap( buffer, &imageData) );

    RT_CHECK_ERROR( rtBufferGetSize2D(buffer, &buffer_width, &buffer_height) );
    width  = static_cast<GLsizei>(buffer_width);
    height = static_cast<GLsizei>(buffer_height);

    std::vector<unsigned char> pix(width * height * 3);

    RTformat buffer_format;
    RT_CHECK_ERROR( rtBufferGetFormat(buffer, &buffer_format) );

    switch(buffer_format) {
        case RT_FORMAT_UNSIGNED_BYTE4:
            // Data is BGRA and upside down, so we need to swizzle to RGB
            for(int j = height-1; j >= 0; --j) {
                unsigned char *dst = &pix[0] + (3*width*(height-1-j));
                unsigned char *src = ((unsigned char*)imageData) + (4*width*j);
                for(int i = 0; i < width; i++) {
                    *dst++ = *(src + 2);
                    *dst++ = *(src + 1);
                    *dst++ = *(src + 0);
                    src += 4;
                }
            }
            break;

        case RT_FORMAT_FLOAT:
            // This buffer is upside down
            for(int j = height-1; j >= 0; --j) {
                unsigned char *dst = &pix[0] + width*(height-1-j);
                float* src = ((float*)imageData) + (3*width*j);
                for(int i = 0; i < width; i++) {
                    int P = static_cast<int>((*src++) * 255.0f);
                    unsigned int Clamped = P < 0 ? 0 : P > 0xff ? 0xff : P;

                    // write the pixel to all 3 channels
                    *dst++ = static_cast<unsigned char>(Clamped);
                    *dst++ = static_cast<unsigned char>(Clamped);
                    *dst++ = static_cast<unsigned char>(Clamped);
                }
            }
            break;

        case RT_FORMAT_FLOAT3:
            // This buffer is upside down
            for(int j = height-1; j >= 0; --j) {
                unsigned char *dst = &pix[0] + (3*width*(height-1-j));
                float* src = ((float*)imageData) + (3*width*j);
                for(int i = 0; i < width; i++) {
                    for(int elem = 0; elem < 3; ++elem) {
                        int P = static_cast<int>((*src++) * 255.0f);
                        unsigned int Clamped = P < 0 ? 0 : P > 0xff ? 0xff : P;
                        *dst++ = static_cast<unsigned char>(Clamped);
                    }
                }
            }
            break;

        case RT_FORMAT_FLOAT4:
            // This buffer is upside down
            for(int j = height-1; j >= 0; --j) {
                unsigned char *dst = &pix[0] + (3*width*(height-1-j));
                float* src = ((float*)imageData) + (4*width*j);
                for(int i = 0; i < width; i++) {
                    for(int elem = 0; elem < 3; ++elem) {
                        int P = static_cast<int>((*src++) * 255.0f);
                        unsigned int Clamped = P < 0 ? 0 : P > 0xff ? 0xff : P;
                        *dst++ = static_cast<unsigned char>(Clamped);
                    }

                    // skip alpha
                    src++;
                }
            }
            break;

        default:
            fprintf(stderr, "Unrecognized buffer data type or format.\n");
            exit(2);
            break;
    }

    std::string suffix;
    std::string fn( filename );
    if ( fn.length() > 4 ) {
        suffix = fn.substr( fn.length() - 4 );
    }

    if ( suffix == ".ppm" ) {
        SavePPM(&pix[0], filename, width, height, 3);
    } else if ( suffix == ".png" ) {
        if ( !stbi_write_png( filename, (int)width, (int)height, 3, &pix[0], /*row stride in bytes*/ width*3*sizeof(unsigned char)) ) {
            throw Exception( std::string("Failed to write image: ") + filename );
        }
    } else {
        throw Exception( std::string("Unrecognized output image file extension: ") + filename );
    }

    // Now unmap the buffer
    RT_CHECK_ERROR( rtBufferUnmap(buffer) );
}


void sutil::displayBufferGL( optix::Buffer buffer )
{
    // Query buffer information
    RTsize buffer_width_rts, buffer_height_rts;
    buffer->getSize( buffer_width_rts, buffer_height_rts );
    uint32_t width  = static_cast<int>(buffer_width_rts);
    uint32_t height = static_cast<int>(buffer_height_rts);
    RTformat buffer_format = buffer->getFormat();
    
    GLboolean use_SRGB = GL_FALSE;
    if( buffer_format == RT_FORMAT_FLOAT4 || buffer_format == RT_FORMAT_FLOAT3 )
    {
        glGetBooleanv( GL_FRAMEBUFFER_SRGB_CAPABLE_EXT, &use_SRGB );
        if( use_SRGB )
            glEnable(GL_FRAMEBUFFER_SRGB_EXT);
    }

    // Check if we have a GL interop display buffer
    const unsigned pboId = buffer->getGLBOId();
    if( pboId )
    {
        static unsigned int gl_tex_id = 0;
        if( !gl_tex_id )
        {
            glGenTextures( 1, &gl_tex_id );
            glBindTexture( GL_TEXTURE_2D, gl_tex_id );

            // Change these to GL_LINEAR for super- or sub-sampling
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

            // GL_CLAMP_TO_EDGE for linear filtering, not relevant for nearest.
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        }

        glBindTexture( GL_TEXTURE_2D, gl_tex_id );

        // send PBO to texture
        glBindBuffer( GL_PIXEL_UNPACK_BUFFER, pboId );

        RTsize elmt_size = buffer->getElementSize();
        if      ( elmt_size % 8 == 0) glPixelStorei(GL_UNPACK_ALIGNMENT, 8);
        else if ( elmt_size % 4 == 0) glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
        else if ( elmt_size % 2 == 0) glPixelStorei(GL_UNPACK_ALIGNMENT, 2);
        else                          glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

        if( buffer_format == RT_FORMAT_UNSIGNED_BYTE4)
            glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_BGRA, GL_UNSIGNED_BYTE, 0);
        else if(buffer_format == RT_FORMAT_FLOAT4)
            glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA32F_ARB, width, height, 0, GL_RGBA, GL_FLOAT, 0);
        else if(buffer_format == RT_FORMAT_FLOAT3)
            glTexImage2D( GL_TEXTURE_2D, 0, GL_RGB32F_ARB, width, height, 0, GL_RGB, GL_FLOAT, 0);
        else if(buffer_format == RT_FORMAT_FLOAT)
            glTexImage2D( GL_TEXTURE_2D, 0, GL_LUMINANCE32F_ARB, width, height, 0, GL_LUMINANCE, GL_FLOAT, 0);
        else
          throw Exception( "Unknown buffer format" );

        glBindBuffer( GL_PIXEL_UNPACK_BUFFER, 0 );

        // 1:1 texel to pixel mapping with glOrtho(0, 1, 0, 1, -1, 1) setup:
        // The quad coordinates go from lower left corner of the lower left pixel 
        // to the upper right corner of the upper right pixel. 
        // Same for the texel coordinates.

        glEnable(GL_TEXTURE_2D);

        glBegin(GL_QUADS);
        glTexCoord2f( 0.0f, 0.0f );
        glVertex2f( 0.0f, 0.0f );

        glTexCoord2f( 1.0f, 0.0f );
        glVertex2f( 1.0f, 0.0f);

        glTexCoord2f( 1.0f, 1.0f );
        glVertex2f( 1.0f, 1.0f );

        glTexCoord2f(0.0f, 1.0f );
        glVertex2f( 0.0f, 1.0f );
        glEnd();

        glDisable(GL_TEXTURE_2D);
    }
    else
    {
        GLvoid* imageData = buffer->map( 0, RT_BUFFER_MAP_READ );
        GLenum gl_data_type = GL_FALSE;
        GLenum gl_format = GL_FALSE;

        switch (buffer_format)
        {
            case RT_FORMAT_UNSIGNED_BYTE4:
                gl_data_type = GL_UNSIGNED_BYTE;
                gl_format    = GL_BGRA;
                break;

            case RT_FORMAT_FLOAT:
                gl_data_type = GL_FLOAT;
                gl_format    = GL_LUMINANCE;
                break;

            case RT_FORMAT_FLOAT3:
                gl_data_type = GL_FLOAT;
                gl_format    = GL_RGB;
                break;

            case RT_FORMAT_FLOAT4:
                gl_data_type = GL_FLOAT;
                gl_format    = GL_RGBA;
                break;

            default:
                fprintf(stderr, "Unrecognized buffer data type or format.\n");
                exit(2);
                break;
        }

        RTsize elmt_size = buffer->getElementSize();
        int align = 1;
        if      ((elmt_size % 8) == 0) align = 8; 
        else if ((elmt_size % 4) == 0) align = 4;
        else if ((elmt_size % 2) == 0) align = 2;
        glPixelStorei(GL_UNPACK_ALIGNMENT, align);

        glDrawPixels(
                static_cast<GLsizei>( width ),
                static_cast<GLsizei>( height ),
                gl_format,
                gl_data_type,
                imageData
                );
        buffer->unmap();
    }

    if ( use_SRGB )
        glDisable(GL_FRAMEBUFFER_SRGB_EXT);
}


namespace
{
    const float FPS_UPDATE_INTERVAL = 0.5;  //seconds
} // namespace


void sutil::displayFps( unsigned int frame_count )
{
    static double fps = -1.0;
    static unsigned last_frame_count = 0;
    static double last_update_time = sutil::currentTime();
    static double current_time = 0.0;
    current_time = sutil::currentTime();
    if ( current_time - last_update_time > FPS_UPDATE_INTERVAL ) {
        fps = ( frame_count - last_frame_count ) / ( current_time - last_update_time );
        last_frame_count = frame_count;
        last_update_time = current_time;
    }
    if ( frame_count > 0 && fps >= 0.0 ) {

        ImGui::SetNextWindowPos( ImVec2( 2.0f, 2.0f ) );
        ImGui::Begin("fps", 0,
                ImGuiWindowFlags_NoTitleBar |
                ImGuiWindowFlags_AlwaysAutoResize |
                ImGuiWindowFlags_NoMove |
                ImGuiWindowFlags_NoScrollbar |
                ImGuiWindowFlags_NoInputs
                );
        ImGui::Text( "fps: %7.2f", fps );
        ImGui::End();
    }
}


optix::TextureSampler sutil::loadTexture( optix::Context context,
        const std::string& filename, optix::float3 default_color )
{
    bool isHDR = false;
    size_t len = filename.length();
    if(len >= 3) {
      isHDR = (filename[len-3] == 'H' || filename[len-3] == 'h') &&
              (filename[len-2] == 'D' || filename[len-2] == 'd') &&
              (filename[len-1] == 'R' || filename[len-1] == 'r');
    }
    if ( isHDR ) {
        return loadHDRTexture(context, filename, default_color);
    } else {
        return loadPPMTexture(context, filename, default_color);
    }
}


optix::Buffer sutil::loadCubeBuffer( optix::Context context,
        const std::vector<std::string>& filenames )
{
    return loadPPMCubeBuffer( context, filenames );
}


void sutil::calculateCameraVariables( float3 eye, float3 lookat, float3 up,
        float  fov, float  aspect_ratio,
        float3& U, float3& V, float3& W, bool fov_is_vertical )
{
    float ulen, vlen, wlen;
    W = lookat - eye; // Do not normalize W -- it implies focal length

    wlen = length( W ); 
    U = normalize( cross( W, up ) );
    V = normalize( cross( U, W  ) );

	if ( fov_is_vertical ) {
		vlen = wlen * tanf( 0.5f * fov * M_PIf / 180.0f );
		V *= vlen;
		ulen = vlen * aspect_ratio;
		U *= ulen;
	}
	else {
		ulen = wlen * tanf( 0.5f * fov * M_PIf / 180.0f );
		U *= ulen;
		vlen = ulen / aspect_ratio;
		V *= vlen;
	}
}


void sutil::parseDimensions( const char* arg, int& width, int& height )
{

    // look for an 'x': <width>x<height>
    size_t width_end = strchr( arg, 'x' ) - arg;
    size_t height_begin = width_end + 1;

    if ( height_begin < strlen( arg ) )
    {
        // find the beginning of the height string/
        const char *height_arg = &arg[height_begin];

        // copy width to null-terminated string
        char width_arg[32];
        strncpy( width_arg, arg, width_end );
        width_arg[width_end] = '\0';

        // terminate the width string
        width_arg[width_end] = '\0';

        width  = atoi( width_arg );
        height = atoi( height_arg );
        return;
    }

    throw Exception(
            "Failed to parse width, heigh from string '" +
            std::string( arg ) +
            "'" );
}


double sutil::currentTime()
{
#if defined(_WIN32)

    // inv_freq is 1 over the number of ticks per second.
    static double inv_freq;
    static bool freq_initialized = 0;
    static bool use_high_res_timer = 0;

    if(!freq_initialized)
    {
        LARGE_INTEGER freq;
        use_high_res_timer = ( QueryPerformanceFrequency( &freq ) != 0 );
        inv_freq = 1.0/freq.QuadPart;
        freq_initialized = 1;
    }

    if (use_high_res_timer)
    {
        LARGE_INTEGER c_time;
        if( QueryPerformanceCounter( &c_time ) )
            return c_time.QuadPart*inv_freq;
        else
            throw Exception( "sutil::currentTime: QueryPerformanceCounter failed" );
    }

    return static_cast<double>( timeGetTime() ) * 1.0e-3;

#else

    struct timeval tv;
    if( gettimeofday( &tv, 0 ) )
        throw Exception( "sutil::urrentTime(): gettimeofday failed!\n" );

    return  tv.tv_sec+ tv.tv_usec * 1.0e-6;

#endif 
}


void sutil::sleep( int seconds )
{
#if defined(_WIN32)
    Sleep( seconds * 1000 );
#else
    ::sleep( seconds );
#endif
}

