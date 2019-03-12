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

//-----------------------------------------------------------------------------
//
// optixParticleVolumes: 
// RBF volume renderer for particle data. 
// Also an example of how to compute and integrate "deep images" using OptiX.
//
//-----------------------------------------------------------------------------

#ifndef __APPLE__
#  include <GL/glew.h>
#  if defined( _WIN32 )
#    include <GL/wglew.h>
#  endif
#endif

#include <GLFW/glfw3.h>

#include <imgui/imgui.h>
#include <imgui/imgui_impl_glfw_gl2.h>

#include <optixu/optixpp_namespace.h>
#include <optixu/optixu_math_stream_namespace.h>

#include <sutil.h>
#include <Camera.h>
#include "commonStructs.h"
#include <Arcball.h>

#include <cstring>
#include <iomanip>
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <stdint.h>
#include <map>

using namespace optix;

struct ParticleFrameData {
  std::vector<float4> positions;
  std::vector<float3> velocities;
  std::vector<float3> colors;
  std::vector<float>  radii;
  float3 bbox_min, bbox_max;
};

std::map<int, ParticleFrameData> dataCache;

const char* const SAMPLE_NAME = "optixParticleVolumes";
const unsigned int WIDTH  = 1024u;
const unsigned int HEIGHT = 768u;

struct ParticlesBuffer
{
    Buffer      positions;
    Buffer      velocities;
    Buffer      colors;
    Buffer      radii;
};

//------------------------------------------------------------------------------
//
// Globals
//
//------------------------------------------------------------------------------

Context         context;
uint32_t        width  = 1024u;
uint32_t        height = 768u;
bool            use_pbo = true;
bool            rbfs = true;
bool            particles_file_colors = false;
bool            particles_file_radius = false;
bool            particles_file_velocities = false;
bool            camera_slow_rotate = true;
size_t          max_particles = 0;
float           fixed_radius = 100.f;
float           particlesPerSlab = 16.f;
float           wScale = 3.5f;
float           opacity = .5f;
int             tf_type = 2;
bool            play = false;
unsigned int    iterations_per_animation_frame = 1;
optix::Aabb     aabb;

// Camera state
float3          camera_up;
float3          camera_lookat;
float3          camera_eye;
Matrix4x4       camera_rotate;
sutil::Arcball  arcball;

// Mouse state
int2            mouse_prev_pos;
int             mouse_button;

// Particles frame state
std::string     particles_file;
std::string     particles_file_extension;
std::string     particles_file_base;
int             current_particle_frame = 1;
int             max_particle_frames = 25;

// Accumulation frame
unsigned int    accumulation_frame = 0;

// Buffers, Geometry and GeometryGroup for the particle set
ParticlesBuffer buffers;
Geometry        geometry;
GeometryGroup   geometry_group;


//------------------------------------------------------------------------------
//
// Forward decls
//
//------------------------------------------------------------------------------

struct UsageReportLogger;

Buffer getOutputBuffer();
void destroyContext();
void registerExitHandler();
void createContext( int usage_report_level, UsageReportLogger* logger );
void loadMesh( const std::string& filename );
void setupCamera();
void setupLights();
void updateCamera();


//------------------------------------------------------------------------------
//
//  Helper functions
//
//------------------------------------------------------------------------------

static std::string ptxPath( const std::string& cuda_file )
{
  return
	std::string(sutil::samplesPTXDir()) +
	"/" + std::string(SAMPLE_NAME) + "_generated_" +
	cuda_file +
	".ptx";
}


Buffer getOutputBuffer()
{
    return context[ "output_buffer" ]->getBuffer();
}


void destroyContext()
{
    if( context )
    {
        context->destroy();
        context = 0;
    }
}

// State for animating buffers
struct RenderBuffers
{
    Buffer ht;       // Frequency domain heights
    Buffer heights;
    Buffer normals;
    int optix_device_ordinal;
};


struct UsageReportLogger
{
  void log( int lvl, const char* tag, const char* msg )
  {
    std::cout << "[" << lvl << "][" << std::left << std::setw( 12 ) << tag << "] " << msg;
  }
};


// Static callback
void usageReportCallback( int lvl, const char* tag, const char* msg, void* cbdata )
{
    // Route messages to a C++ object (the "logger"), as a real app might do.
    // We could have printed them directly in this simple case.

    UsageReportLogger* logger = reinterpret_cast<UsageReportLogger*>( cbdata );
    logger->log( lvl, tag, msg );
}


void setParticlesBaseName( const std::string &particles_file )
{
    // gets the base name by stripping the suffix and the number, if there is no number
    // then there will be no sequence
    std::string::size_type first_dot_pos  = particles_file.find( "." );
    std::string::size_type second_dot_pos = particles_file.rfind( "." );

    particles_file_extension = particles_file.substr( second_dot_pos+1 );

    if ( ( first_dot_pos == std::string::npos ) || ( first_dot_pos == second_dot_pos ) ) {
        // either there are no dots in the name or there is just one, it won't be a sequence
        current_particle_frame = 0;
        particles_file_base = particles_file;
        return;
    }

    std::string frame_str = particles_file.substr( first_dot_pos+1, second_dot_pos-first_dot_pos-1 );

    current_particle_frame = atoi( frame_str.c_str() );
    particles_file_base = particles_file.substr( 0, first_dot_pos );
}


void createContext( int usage_report_level, UsageReportLogger* logger )
{
    // Set up context
    context = Context::create();
    context->setRayTypeCount( 2 );
    context->setEntryPointCount( 1 );
    if( usage_report_level > 0 )
    {
        context->setUsageReportCallback( usageReportCallback, usage_report_level, logger );
    }
    context["radiance_ray_type"]->setUint( 0u );
    context["shadow_ray_type"  ]->setUint( 1u );
    context["scene_epsilon"    ]->setFloat( 1.e-4f );

    Buffer output_buffer = sutil::createOutputBuffer( context, RT_FORMAT_UNSIGNED_BYTE4, width, height, use_pbo );
    context["output_buffer"]->set( output_buffer );

    Buffer accum_buffer = context->createBuffer( RT_BUFFER_INPUT_OUTPUT | RT_BUFFER_GPU_LOCAL,
        RT_FORMAT_FLOAT4, width, height );
    context["accum_buffer"]->set( accum_buffer );

    // Ray generation program
    std::string ptx;
    ptx = ptxPath( "raygen.cu" );
    Program ray_gen_program = context->createProgramFromPTXFile( ptxPath("raygen.cu"), "raygen_program" );

    context->setRayGenerationProgram( 0, ray_gen_program );

    // Exception program
    Program exception_program = context->createProgramFromPTXFile( ptxPath("raygen.cu"), "exception" );
    context->setExceptionProgram( 0, exception_program );
    context["bad_color"]->setFloat( 1.0f, 0.0f, 1.0f );

    // Miss program
    context->setMissProgram( 0, context->createProgramFromPTXFile( ptxPath("constantbg.cu"), "miss" ) );

    context["bg_color"]->setFloat( 0.07f, 0.11f, 0.17f );

}


static inline float parseFloat( const char *&token )
{
  token += strspn( token, " \t" );
  float f = (float) atof( token );
  token += strcspn( token, " \t\r" );
  return f;
}


static inline float3 get_min(
    const float3 &v1,
    const float3 &v2)
{
    float3 result;
    result.x = std::min<float>( v1.x, v2.x );
    result.y = std::min<float>( v1.y, v2.y );
    result.z = std::min<float>( v1.z, v2.z );
    return result;
}


static inline float3 get_max(
    const float3 &v1,
    const float3 &v2)
{
    float3 result;
    result.x = std::max<float>( v1.x, v2.x );
    result.y = std::max<float>( v1.y, v2.y );
    result.z = std::max<float>( v1.z, v2.z );
    return result;
}


static void fillBuffers(
    const std::vector<float4> &positions,
    const std::vector<float3> &velocities,
    const std::vector<float3> &colors,
    const std::vector<float>  &radii)
{
    buffers.positions->setSize( positions.size() );
    float *pos = reinterpret_cast<float*> ( buffers.positions->map() );
    for ( int i=0, index = 0; i<static_cast<int>(positions.size()); ++i ) {
        float4 p = positions[i];

        pos[index++] = p.x;
        pos[index++] = p.y;
        pos[index++] = p.z;
        pos[index++] = p.w;
    }
    buffers.positions->unmap();

    buffers.velocities->setSize( velocities.size() );
    float *vel = reinterpret_cast<float*> ( buffers.velocities->map() );
    for ( int i=0, index = 0; i<static_cast<int>(velocities.size()); ++i ) {
        float3 v = velocities[i];

        vel[index++] = v.x;
        vel[index++] = v.y;
        vel[index++] = v.z;
    }
    buffers.velocities->unmap();

    buffers.colors->setSize( colors.size() );
    float *col = reinterpret_cast<float*> ( buffers.colors->map() );
    for ( int i=0, index = 0; i<static_cast<int>(colors.size()); ++i ) {
        float3 c = colors[i];

        col[index++] = c.x;
        col[index++] = c.y;
        col[index++] = c.z;
    }
    buffers.colors->unmap();

    buffers.radii->setSize( radii.size() );
    float *rad = reinterpret_cast<float*> ( buffers.radii->map() );
    for ( int i=0; i<static_cast<int>(radii.size()); ++i ) {
        rad[i] = radii[i];
    }
    buffers.radii->unmap();
}


void createMaterialPrograms(
    Context context,
    Program &closest_hit,
    Program &any_hit)
{
    if( !any_hit )
      any_hit     = context->createProgramFromPTXFile( ptxPath("material.cu"), "any_hit" );
}


Material createOptiXMaterial(
    Context context,
    Program closest_hit,
    Program any_hit)
{
    Material mat = context->createMaterial();
    mat->setAnyHitProgram( 0u, any_hit ) ;

    return mat;
}


Program createBoundingBoxProgram( Context context )
{
  return context->createProgramFromPTXFile( ptxPath("geometry.cu"), "particle_bounds" );
}


Program createIntersectionProgram( Context context )
{
  return context->createProgramFromPTXFile( ptxPath("geometry.cu"), "particle_intersect" );
}

void readFile( std::vector<float4>& positions, 
               std::vector<float3>& velocities, 
               std::vector<float3>& colors, 
               std::vector<float>& radii, 
               float3& bbox_min, 
               float3& bbox_max )
{
	//read raw data file.
    if (particles_file_extension == "raw")
    {
        std::cout << "Reading raw file" << particles_file << std::endl;

        FILE* fp = fopen(particles_file.c_str(), "r");
        fseek(fp, 0L, SEEK_END);
        size_t sz = ftell(fp);
        rewind(fp);

        size_t numParticles = sz / 16;
        std::cout << "# particles = " << numParticles << std::endl;

        if (max_particles > 0 && numParticles > max_particles)
        {
          std::cout << "only reading " << max_particles << " particles." << std::endl;
          numParticles = max_particles;
        }

        positions.resize(numParticles);
        size_t b = fread(&positions[0], sizeof(float), numParticles, fp);
        fclose(fp);

        float4 pmin, pmax;

        pmin.x = pmin.y = pmin.z = pmin.w = 1e16f;
        pmax.x = pmax.y = pmax.z = pmax.w = -1e16f;

        const float rd = fixed_radius;

        #pragma omp parallel for
        for(size_t i=0; i<numParticles; i++)
        {
            const float4& p = positions[i];

            pmin.x = fminf(pmin.x, p.x);
            pmin.y = fminf(pmin.y, p.y);
            pmin.z = fminf(pmin.z, p.z);
            pmin.w = fminf(pmin.w, p.w);

            pmax.x = fmaxf(pmax.x, p.x);
            pmax.y = fmaxf(pmax.y, p.y);
            pmax.z = fmaxf(pmax.z, p.z);
            pmax.w = fmaxf(pmax.w, p.w);
        }

        std::cout << "Particle pmin = " << pmin << std::endl;
        std::cout << "Particle pmax = " << pmax << std::endl;

        bbox_min = make_float3(pmin.x - rd, pmin.y - rd, pmin.z - rd);
        bbox_max = make_float3(pmax.x + rd, pmax.y + rd, pmax.z + rd);

        if (fixed_radius == 0.f)
          fixed_radius = length(bbox_max - bbox_min) / powf(float(numParticles), 0.33333f);  

        std::cout << "Particle fixed_radius = " << fixed_radius << std::endl;

        float wRange, wOff;
        if (pmin.w < 0.f)
        {
          if (-pmin.w > pmax.w)
            wRange = float(0.5f / -pmin.w);
          else
            wRange = float(0.5f / pmax.w);

          tf_type = 3;
          wOff = .5f;
        }
        else
        {
          wRange = float( 1.0 / double(pmax.w - pmin.w) );
          wOff = 0.f;
        }

        std::cout << "Transfer function tf_type = " << tf_type << std::endl;

        #pragma omp parallel for
        for(size_t i=0; i<numParticles; i++)
            positions[i].w = positions[i].w * wRange + wOff;

        float wmin = 1e16f;
        float wmax = -1e16f;
        for(size_t i=0; i<numParticles; i++){
            wmin = fminf(wmin, positions[i].w);
            wmax = fmaxf(wmax, positions[i].w);
        }
        std::cout << "Attribute range wmin = " << wmin << ", wmax = " << wmax << std::endl;
    }
     
    //read txt data file
    else
    {
        std::cout << "Reading txt file" << particles_file << std::endl;

        std::string filename = particles_file_base;

        if ( current_particle_frame > 0 ) {
            std::ostringstream s;
            s << current_particle_frame;

            if ( current_particle_frame < 10 )
                filename += ".000" + s.str() + ".txt";
            else
                filename += ".00" + s.str() + ".txt";
        }
        else
            filename = particles_file_base;


        std::ifstream ifs( filename.c_str() );

        bbox_min.x = bbox_min.y = bbox_min.z = 1e16f;
        bbox_max.x = bbox_max.y = bbox_max.z = -1e16f;

        int maxchars = 8192;
        std::vector<char> buf(static_cast<size_t>(maxchars)); // Alloc enough size

        float wmin = 1e16f;
        float wmax = -1e16f;

        while ( ifs.peek() != -1 ) {
            ifs.getline( &buf[0], maxchars );

            std::string linebuf(&buf[0]);

            // Trim newline '\r\n' or '\n'
            if ( linebuf.size() > 0 ) {
                if ( linebuf[linebuf.size() - 1] == '\n' )
                    linebuf.erase(linebuf.size() - 1);
            }

            if ( linebuf.size() > 0 ) {
                if ( linebuf[linebuf.size() - 1] == '\r' )
                    linebuf.erase( linebuf.size() - 1 );
            }

            // Skip if empty line.
            if ( linebuf.empty() ) {
                continue;
            }

            // Skip leading space.
            const char *token = linebuf.c_str();
            token += strspn( token, " \t" );

            assert( token );
            if ( token[0] == '\0' )
                continue; // empty line

            if ( token[0] == '#' )
                continue; // comment line

            // meaningful line here. The expected format is: position, velocity, color and radius

            // position
            float x  = parseFloat( token );
            float y  = parseFloat( token );
            float z  = parseFloat( token );

            // velocity
            float vx = parseFloat( token );
            float vy = parseFloat( token );
            float vz = parseFloat( token );

            float3 vel = make_float3(vx,vy,vz);
            float vel_magnitude = length(vel);

            wmin = fminf(wmin, vel_magnitude);
            wmax = fmaxf(wmax, vel_magnitude);

            float r,g,b;
            r=g=b=.9f;

            if (particles_file_colors)
            {
              // color
              r  = parseFloat( token );
              g  = parseFloat( token );
              b  = parseFloat( token );
            }
            
            float rd = fixed_radius;
            if (particles_file_radius)
            {
              // radius
              rd = parseFloat( token );
            }

            positions.push_back( make_float4( x, y, z, vel_magnitude ) );
            velocities.push_back( make_float3( vx, vy, vz ) );
            colors.push_back( make_float3( r, g, b ) );
            radii.push_back( rd );
        }

        const size_t numParticles = positions.size();
        std::cout << "# particles = " << numParticles << std::endl;

        float4 pmin, pmax;
        pmin.x = pmin.y = pmin.z = pmin.w = 1e16f;
        pmax.x = pmax.y = pmax.z = pmax.w = -1e16f;

        for(size_t i=0; i<numParticles; i++)
        {
            const float4& p = positions[i];

            pmin.x = fminf(pmin.x, p.x);
            pmin.y = fminf(pmin.y, p.y);
            pmin.z = fminf(pmin.z, p.z);
            pmin.w = fminf(pmin.w, p.w);

            pmax.x = fmaxf(pmax.x, p.x);
            pmax.y = fmaxf(pmax.y, p.y);
            pmax.z = fmaxf(pmax.z, p.z);
            pmax.w = fmaxf(pmax.w, p.w);
        }

        std::cout << "Particle pmin = " << pmin << std::endl;
        std::cout << "Particle pmax = " << pmax << std::endl;

        bbox_min = make_float3(pmin.x, pmin.y, pmin.z);
        bbox_max = make_float3(pmax.x, pmax.y, pmax.z);

        if (fixed_radius == 0.f)
          fixed_radius = length(bbox_max - bbox_min) / powf(float(numParticles), 0.333333f);  

        std::cout << "Using fixed_radius = " << fixed_radius << std::endl;

        bbox_min -= make_float3(fixed_radius);
        bbox_max += make_float3(fixed_radius);

        std::cout << "Attribute range wmin = " << wmin << ", wmax = " << wmax << std::endl;

        float wRange = float( 1.0 / double(wmax - wmin) );

        //#pragma omp parallel for
        for(size_t i=0; i<positions.size(); i++)
            positions[i].w = positions[i].w * wRange;
    }

}


// loads up the particles file corresponding to the current frame (if it is a sequence)
void loadParticles()
{
    float3 bbox_min, bbox_max;

    std::map<int, ParticleFrameData>::iterator cacheIt = dataCache.find(current_particle_frame);
    if(cacheIt == dataCache.end())
    {
        ParticleFrameData newCacheEntry;

        std::vector<float4>& positions = newCacheEntry.positions;
        std::vector<float3>& velocities = newCacheEntry.velocities;
        std::vector<float3>& colors = newCacheEntry.colors;
        std::vector<float>&  radii = newCacheEntry.radii;

	    readFile(positions, velocities, colors, radii, bbox_min, bbox_max);

        context[ "fixed_radius"     ]->setFloat(fixed_radius);
        context[ "particlesPerSlab"     ]->setFloat(particlesPerSlab);
        context[ "wScale" ] ->setFloat(wScale);
        context[ "opacity" ] ->setFloat(opacity);
        context[ "tf_type" ]->setInt(tf_type);

        context[ "bbox_min"     ]->setFloat(bbox_min);
        context[ "bbox_max"     ]->setFloat(bbox_max);

        newCacheEntry.bbox_min = bbox_min;
        newCacheEntry.bbox_max = bbox_max;
        cacheIt = dataCache.insert(std::make_pair(current_particle_frame, newCacheEntry)).first;
    }

    ParticleFrameData& cacheEntry = cacheIt->second;

    // all vectors have the same size
    geometry->setPrimitiveCount( (int) cacheEntry.positions.size() );

    // fills up the buffers
    fillBuffers( cacheEntry.positions, cacheEntry.velocities, cacheEntry.colors, cacheEntry.radii );

    // the bounding box will actually be used only for the first frame
    aabb.set( cacheEntry.bbox_min, cacheEntry.bbox_max );

    // builds the BVH (or re-builds it if already existing)
    Acceleration accel = geometry_group->getAcceleration();
    accel->markDirty();
}


void setupParticles()
{
    // the buffers will be set to the right size at a later stage
    buffers.positions  = context->createBuffer( RT_BUFFER_INPUT, RT_FORMAT_FLOAT4, 0 );
    buffers.velocities = context->createBuffer( RT_BUFFER_INPUT, RT_FORMAT_FLOAT3, 0 );
    buffers.colors     = context->createBuffer( RT_BUFFER_INPUT, RT_FORMAT_FLOAT3, 0 );
    buffers.radii      = context->createBuffer( RT_BUFFER_INPUT, RT_FORMAT_FLOAT,  0 );

    context[ "positions_buffer"  ]->setBuffer( buffers.positions );

    geometry = context->createGeometry();
    geometry[ "positions_buffer"  ]->setBuffer( buffers.positions );

    geometry[ "fixed_radius"      ]->setFloat(fixed_radius);
    geometry->setBoundingBoxProgram  ( createBoundingBoxProgram( context ) );
    geometry->setIntersectionProgram ( createIntersectionProgram( context ) );

    Program closest_hit, any_hit;
    createMaterialPrograms( context, closest_hit, any_hit );

    std::vector<Material> optix_materials;
    optix_materials.push_back( createOptiXMaterial(
        context,
        closest_hit,
        any_hit ) );

    GeometryInstance geom_instance = context->createGeometryInstance(
        geometry,
        optix_materials.begin(),
        optix_materials.end() );

    geometry_group = context->createGeometryGroup();

    geometry_group->addChild( geom_instance );
    
    Acceleration accel = context->createAcceleration( "Bvh8" );

    //We only need to refit the BVH when the radius changes. However, in some versions of OptiX a full rebuild may be required.
    //accel->setProperty( "refit", "1" );
    geometry_group->setAcceleration( accel );

    context[ "top_object"   ]->set( geometry_group );
    context[ "top_shadower" ]->set( geometry_group );
}

void setupCamera()
{
    const float max_dim = fmaxf( aabb.extent( 0 ), aabb.extent( 1 ) ); // max of x, y components

    camera_eye    = aabb.center() + make_float3( 0.0f, 0.0f, max_dim*1.1f );
    camera_lookat = aabb.center();
    camera_up     = make_float3( 0.0f, 1.0f, 0.0f );

    camera_rotate  = Matrix4x4::identity();
}


void setupLights()
{
    const float max_dim = fmaxf( aabb.extent( 0 ), aabb.extent (1 ) ); // max of x, y components

    BasicLight lights[] = {
        { make_float3( -0.5f,  0.25f, -1.0f ), make_float3( 0.2f, 0.2f, 0.25f ), 0, 0 },
        { make_float3( -0.5f,  0.0f ,  1.0f ), make_float3( 0.1f, 0.1f, 0.10f ), 0, 0 },
        { make_float3(  0.5f,  0.5f ,  0.5f ), make_float3( 0.7f, 0.7f, 0.65f ), 1, 0 }
    };
    lights[0].pos *= max_dim * 10.0f;
    lights[1].pos *= max_dim * 10.0f;
    lights[2].pos *= max_dim * 10.0f;

    Buffer light_buffer = context->createBuffer( RT_BUFFER_INPUT );
    light_buffer->setFormat( RT_FORMAT_USER );
    light_buffer->setElementSize( sizeof( BasicLight ) );
    light_buffer->setSize( sizeof( lights )/sizeof( lights[0] ) );
    memcpy( light_buffer->map(), lights, sizeof( lights ) );
    light_buffer->unmap();

    context[ "lights" ]->set( light_buffer );
}


void updateCamera()
{
    const float vfov = 35.0f;
    const float aspect_ratio = static_cast<float>(width) /
                               static_cast<float>(height);

    float3 camera_u, camera_v, camera_w;
    sutil::calculateCameraVariables(
            camera_eye, camera_lookat, camera_up, vfov, aspect_ratio,
            camera_u, camera_v, camera_w, true );

    const Matrix4x4 frame = Matrix4x4::fromBasis(
            normalize( camera_u ),
            normalize( camera_v ),
            normalize( -camera_w ),
            camera_lookat);
    const Matrix4x4 frame_inv = frame.inverse();
    // Apply camera rotation twice to match old SDK behavior
    const Matrix4x4 trans     = frame*camera_rotate*camera_rotate*frame_inv;

    camera_eye    = make_float3( trans*make_float4( camera_eye,    1.0f ) );
    camera_lookat = make_float3( trans*make_float4( camera_lookat, 1.0f ) );
    camera_up     = make_float3( trans*make_float4( camera_up,     0.0f ) );

    sutil::calculateCameraVariables(
            camera_eye, camera_lookat, camera_up, vfov, aspect_ratio,
            camera_u, camera_v, camera_w, true );

    camera_rotate = Matrix4x4::identity();

    context[ "eye" ]->setFloat( camera_eye );
    context[ "U"   ]->setFloat( camera_u );
    context[ "V"   ]->setFloat( camera_v );
    context[ "W"   ]->setFloat( camera_w );

}


//------------------------------------------------------------------------------
//
//  GLFW callbacks
//
//------------------------------------------------------------------------------


struct CallbackData
{
    sutil::Camera& camera;
    unsigned int& accumulation_frame;
};

void keyCallback( GLFWwindow* window, int key, int scancode, int action, int mods )
{
    bool handled = false;

    if( action == GLFW_PRESS )
    {
        switch( key )
        {
            case GLFW_KEY_Q:
            case GLFW_KEY_ESCAPE:
                if( context )
                    context->destroy();
                if( window )
                    glfwDestroyWindow( window );
                glfwTerminate();
                exit(EXIT_SUCCESS);

            case( GLFW_KEY_S ):
            {
                const std::string outputImage = std::string(SAMPLE_NAME) + ".png";
                std::cout << "Saving current frame to '" << outputImage << "'\n";
                sutil::writeBufferToFile( outputImage.c_str(), getOutputBuffer() );
                handled = true;
                break;
            }
            case( GLFW_KEY_F ):
            {
               CallbackData* cb = static_cast<CallbackData*>( glfwGetWindowUserPointer( window ) );
               cb->camera.reset_lookat();
               cb->accumulation_frame = 0;
               handled = true;
               break;
            }
            case( GLFW_KEY_SPACE ):
            {
	       camera_slow_rotate = !camera_slow_rotate;
               handled = true;
               break;
            }

        }
    }

    if (!handled) {
        // forward key event to imgui
        ImGui_ImplGlfw_KeyCallback( window, key, scancode, action, mods );
    }
}

void windowSizeCallback( GLFWwindow* window, int w, int h )
{
    if (w < 0 || h < 0) return;

    const unsigned width = (unsigned)w;
    const unsigned height = (unsigned)h;

    CallbackData* cb = static_cast<CallbackData*>( glfwGetWindowUserPointer( window ) );
    if ( cb->camera.resize( width, height ) ) {
        cb->accumulation_frame = 0;
    }

    sutil::resizeBuffer( getOutputBuffer(), width, height );
    sutil::resizeBuffer( context[ "accum_buffer" ]->getBuffer(), width, height );

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, 1, 0, 1, -1, 1);
    glViewport(0, 0, width, height);
}


//------------------------------------------------------------------------------
//
// GLFW setup and run 
//
//------------------------------------------------------------------------------

GLFWwindow* glfwInitialize( )
{
    GLFWwindow* window = sutil::initGLFW();

    // Note: this overrides imgui key callback with our own.  We'll chain this.
    glfwSetKeyCallback( window, keyCallback );

    glfwSetWindowSize( window, (int)WIDTH, (int)HEIGHT );
    glfwSetWindowSizeCallback( window, windowSizeCallback );

    return window;
}


void glfwRun( GLFWwindow* window, sutil::Camera& camera, RenderBuffers& buffers )
{
    // Initialize GL state
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, 1, 0, 1, -1, 1 );
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glViewport(0, 0, WIDTH, HEIGHT );

    unsigned int frame_count = 0;
    unsigned int accumulation_frame = 0;
    bool do_animate = true;

    double previous_time = sutil::currentTime();
    double anim_time = 0.0f;

    // Expose user data for access in GLFW callback functions when the window is resized, etc.
    // This avoids having to make it global.
    CallbackData cb = { camera, accumulation_frame };
    glfwSetWindowUserPointer( window, &cb );

    const float initial_fixed_radius = fixed_radius;
    const float fixed_radius_min = fixed_radius * .25f;
    const float fixed_radius_max = fixed_radius * 2.f;

    while( !glfwWindowShouldClose( window ) )
    {

        glfwPollEvents();                                                        

        ImGui_ImplGlfwGL2_NewFrame();

        ImGuiIO& io = ImGui::GetIO();

        static double mouseX, mouseY;
        glfwGetCursorPos( window, &mouseX, &mouseY );
       
        // Let imgui process the mouse first
        if (!io.WantCaptureMouse) 
        {
            glfwGetCursorPos( window, &mouseX, &mouseY );

            if ( camera.process_mouse( (float)mouseX, (float)mouseY, ImGui::IsMouseDown(0), ImGui::IsMouseDown(1), ImGui::IsMouseDown(2) ) ) {
                accumulation_frame = 0;
            }
        }

        if (camera_slow_rotate )
            camera.rotate(1.f, 0.f);

        // imgui pushes
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,   ImVec2(0,0) );
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha,          0.6f        );
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 2.0f        );

        sutil::displayFps( frame_count++ );
        
        {
            static const ImGuiWindowFlags window_flags = 
                    ImGuiWindowFlags_NoTitleBar |
                    ImGuiWindowFlags_AlwaysAutoResize |
                    ImGuiWindowFlags_NoMove |
                    ImGuiWindowFlags_NoScrollbar;
            ImGui::SetNextWindowPos( ImVec2( 2.0f, 40.0f ) );
            ImGui::Begin("controls", 0, window_flags );

            if (ImGui::SliderFloat( "radius", &fixed_radius, fixed_radius_min, fixed_radius_max ) ) {
              context[ "fixed_radius"     ]->setFloat(fixed_radius);
              geometry[ "fixed_radius"      ]->setFloat(fixed_radius);
              Acceleration accel = geometry_group->getAcceleration();
              accel->setProperty( "refit", "1" );
              accel->markDirty();
            }
        
            if (ImGui::SliderFloat( "attribute scale", &wScale, .1f, 10.f ) ) {
              context[ "wScale" ] ->setFloat(wScale);
            }

            if (ImGui::SliderFloat( "sample opacity", &opacity, 0.f, 1.f ) ) {
              context[ "opacity"     ]->setFloat(opacity);
            }

            if (ImGui::SliderInt( "transfer function preset", &tf_type, 1, 3 ) ) {
              context[ "tf_type" ] ->setInt(tf_type);
            }

            static float redshift = 1.f;
            if (ImGui::SliderFloat( "redshift scale", &redshift, 0.f, 10.f ) ) {
              context[ "redshift" ] ->setFloat(redshift);
            }

            if ( ImGui::Checkbox( "camera rotate", &camera_slow_rotate ) ) {
            }

            ImGui::End();
        }

        // imgui pops
        ImGui::PopStyleVar( 3 );

        if ( do_animate ) {

            // update animation time
            const double current_time = sutil::currentTime();
            anim_time += previous_time - current_time;
            previous_time = current_time;

            //updateHeightfield( static_cast<float>( anim_time ), buffers );
            accumulation_frame = 0;
        }

        // Render main window
        context["frame"]->setUint( accumulation_frame++ );
        context->launch( 0, camera.width(), camera.height() );

        // Tonemap
        //context->launch( 3, camera.width(), camera.height() );
        sutil::displayBufferGL( getOutputBuffer() );

        // Render gui over it
        ImGui::Render();
        ImGui_ImplGlfwGL2_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers( window );
    }
    
    destroyContext();
    glfwDestroyWindow( window );
    glfwTerminate();
}



//------------------------------------------------------------------------------
//
// Main
//
//------------------------------------------------------------------------------


void printUsageAndExit( const std::string& argv0 )
{
    std::cout << "\nUsage: " << argv0 << " [options]\n";
    std::cout <<
        "App Options:\n"
        "  -h | --help                         Print this usage message and exit.\n"
        "  -f | --file                         Save single frame to file and exit.\n"
        "  -n | --nopbo                        Disable GL interop for display buffer.\n"
        "  -p | --particles <particles_file>   Specify path to particles file to be loaded.\n"
        "  -r | --report <LEVEL>               Enable usage reporting and report level [1-3].\n"
        "  --no-rotate                         Disable camera rotation (default on).\n"
        "  --wScale <float>                    Rescale particle attribute range by a fixed multiple.\n"
        "  --opacity <float>                   Opacity scale (alpha) for each particle.\n"
        "  --fixed_radius <float>              Specify default (world space) radius of a particle.\n"
        "  --max_particles <int M>             Only read the first M particles of the dataset.\n"
        "  --tf_type <int>                     Use preset transfer function (0,1,2 = unsigned data, 3 = signed data).\n"
        "App Keystrokes:\n"
        "  q  Quit\n"
        << std::endl;

    exit(1);
}

int main( int argc, char** argv )
 {
    std::string out_file;
    particles_file = std::string( sutil::samplesDir() ) + "/data/darksky_1M.xyz";
    int usage_report_level = 0;
    for( int i=1; i<argc; ++i )
    {
        const std::string arg( argv[i] );

        if( arg == "-h" || arg == "--help" )
        {
            printUsageAndExit( argv[0] );
        }
        else if( arg == "-f" || arg == "--file"  )
        {
            if( i == argc-1 )
            {
                std::cout << "Option '" << arg << "' requires additional argument.\n";
                printUsageAndExit( argv[0] );
            }
            out_file = argv[++i];
        }
        else if( arg == "--no_colors"  )
        {
            //Note: this is left as an undocumented feature, in case users wish to modify the sample to support per-particle colors
            // (the original optixParticles sample does)
            particles_file_colors = false;
        }
        else if( arg == "--max_particles"  )
        {
            if( i == argc-1 )
            {
                std::cout << "Option '" << argv[i] << "' requires additional argument.\n";
                printUsageAndExit( argv[0] );
            }
            max_particles = atoi(argv[++i]);
        }
        else if( arg == "--tf_type"  )
        {
            if( i == argc-1 )
            {
                std::cout << "Option '" << argv[i] << "' requires additional argument.\n";
                printUsageAndExit( argv[0] );
            }
            tf_type = atoi(argv[++i]);
        }
        else if( arg == "--no_radius"  )
        {
            particles_file_radius = false;
        }
        else if( arg == "--no-rotate"  )
        {
            camera_slow_rotate = false;
        }
        else if( arg == "--fixed_radius"  )
        {
            if( i == argc-1 )
            {
                std::cout << "Option '" << argv[i] << "' requires additional argument.\n";
                printUsageAndExit( argv[0] );
            }
            fixed_radius = (float) atof( argv[++i] );
        }

        else if( arg == "--particlesPerSlab"  )
        {
            if( i == argc-1 )
            {
                std::cout << "Option '" << argv[i] << "' requires additional argument.\n";
                printUsageAndExit( argv[0] );
            }
            particlesPerSlab = (float) atof( argv[++i] );
        }
        else if( arg == "--wScale"  )
        {
            if( i == argc-1 )
            {
                std::cout << "Option '" << argv[i] << "' requires additional argument.\n";
                printUsageAndExit( argv[0] );
            }
            wScale = (float) atof( argv[++i] );
        }
        else if( arg == "--opacity"  )
        {
            if( i == argc-1 )
            {
                std::cout << "Option '" << argv[i] << "' requires additional argument.\n";
                printUsageAndExit( argv[0] );
            }
            opacity = (float) atof( argv[++i] );
        }
        else if( arg == "-n" || arg == "--nopbo"  )
        {
            use_pbo = false;
        }
        else if( arg == "-p" || arg == "--particles" )
        {
            if( i == argc-1 )
            {
                std::cout << "Option '" << argv[i] << "' requires additional argument.\n";
                printUsageAndExit( argv[0] );
            }
            particles_file = argv[++i];
        }
        else if( arg == "-r" || arg == "--report" )
        {
            if( i == argc-1 )
            {
                std::cout << "Option '" << argv[i] << "' requires additional argument.\n";
                printUsageAndExit( argv[0] );
            }
            usage_report_level = atoi( argv[++i] );
        }
        else
        {
            std::cout << "Unknown option '" << arg << "'\n";
            printUsageAndExit( argv[0] );
        }
    }

    try
    {

        GLFWwindow* window = glfwInitialize();

#ifndef __APPLE__
        GLenum err = glewInit();
        if (err != GLEW_OK)
        {
            std::cout << "GLEW init failed: " << glewGetErrorString( err ) << std::endl;
            exit(EXIT_FAILURE);
        }
#endif

        UsageReportLogger logger;
        RenderBuffers render_buffers;

        createContext( usage_report_level, &logger );
        setupParticles();
        setParticlesBaseName( particles_file );
        loadParticles();
        setupCamera();
        setupLights();

        sutil::Camera camera( WIDTH, HEIGHT, 
                &camera_eye.x, &camera_lookat.x, &camera_up.x,
                context["eye"], context["U"], context["V"], context["W"] );

        context->validate();

        if ( out_file.empty() )
        {
            glfwRun( window, camera, render_buffers );
        }
        else
        {
            updateCamera();
            context->launch( 0, width, height );
            sutil::writeBufferToFile( out_file.c_str(), getOutputBuffer() );
            std::cout << "Wrote " << out_file << std::endl;
            destroyContext();

        }

    }
    SUTIL_CATCH( context->get() )
}

