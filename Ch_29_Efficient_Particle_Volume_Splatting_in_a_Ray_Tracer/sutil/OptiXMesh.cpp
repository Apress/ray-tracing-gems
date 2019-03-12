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

#include <optixu/optixu_math_namespace.h>

#include "Mesh.h"
#include "OptiXMesh.h"
#include "sutil.h"
#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace optix {
  float3 make_float3( const float* a )
  {
    return make_float3( a[0], a[1], a[2] );
  }
}


namespace
{

struct MeshBuffers
{
  optix::Buffer tri_indices;
  optix::Buffer mat_indices;
  optix::Buffer positions;
  optix::Buffer normals;
  optix::Buffer texcoords;
};


void setupMeshLoaderInputs(
    optix::Context            context, 
    MeshBuffers&              buffers,
    Mesh&                     mesh
    )
{
  buffers.tri_indices = context->createBuffer( RT_BUFFER_INPUT, RT_FORMAT_INT3,   mesh.num_triangles );
  buffers.mat_indices = context->createBuffer( RT_BUFFER_INPUT, RT_FORMAT_INT,    mesh.num_triangles );
  buffers.positions   = context->createBuffer( RT_BUFFER_INPUT, RT_FORMAT_FLOAT3, mesh.num_vertices );
  buffers.normals     = context->createBuffer( RT_BUFFER_INPUT, RT_FORMAT_FLOAT3,
                                               mesh.has_normals ? mesh.num_vertices : 0);
  buffers.texcoords   = context->createBuffer( RT_BUFFER_INPUT, RT_FORMAT_FLOAT2,
                                               mesh.has_texcoords ? mesh.num_vertices : 0);

  mesh.tri_indices = reinterpret_cast<int32_t*>( buffers.tri_indices->map() );
  mesh.mat_indices = reinterpret_cast<int32_t*>( buffers.mat_indices->map() );
  mesh.positions   = reinterpret_cast<float*>  ( buffers.positions->map() );
  mesh.normals     = reinterpret_cast<float*>  ( mesh.has_normals   ? buffers.normals->map()   : 0 );
  mesh.texcoords   = reinterpret_cast<float*>  ( mesh.has_texcoords ? buffers.texcoords->map() : 0 );

  mesh.mat_params = new MaterialParams[ mesh.num_materials ];
}


void unmap( MeshBuffers& buffers, Mesh& mesh )
{
  buffers.tri_indices->unmap();
  buffers.mat_indices->unmap();
  buffers.positions->unmap();
  if( mesh.has_normals )
    buffers.normals->unmap();
  if( mesh.has_texcoords)
    buffers.texcoords->unmap();

  mesh.tri_indices = 0; 
  mesh.mat_indices = 0;
  mesh.positions   = 0;
  mesh.normals     = 0;
  mesh.texcoords   = 0;

  delete [] mesh.mat_params;
  mesh.mat_params = 0;
}


void createMaterialPrograms(
    optix::Context         context,
    bool                   use_textures,
    optix::Program&        closest_hit,
    optix::Program&        any_hit
    )
{
  const std::string path = std::string( sutil::samplesPTXDir() ) + 
                          "/cuda_compile_ptx_generated_phong.cu.ptx";
  const std::string closest_name = use_textures ?
                                   "closest_hit_radiance_textured" :
                                   "closest_hit_radiance";

  if( !closest_hit )
      closest_hit = context->createProgramFromPTXFile( path, closest_name );
  if( !any_hit )
      any_hit     = context->createProgramFromPTXFile( path, "any_hit_shadow" );
}


optix::Material createOptiXMaterial(
    optix::Context         context,
    optix::Program         closest_hit,
    optix::Program         any_hit,
    const MaterialParams&  mat_params,
    bool                   use_textures
    )
{
  optix::Material mat = context->createMaterial();
  mat->setClosestHitProgram( 0u, closest_hit );             
  mat->setAnyHitProgram( 1u, any_hit ) ;    

  if( use_textures )
    mat[ "Kd_map"]->setTextureSampler( sutil::loadTexture( context, mat_params.Kd_map, optix::make_float3(mat_params.Kd) ) );
  else
    mat[ "Kd_map"]->setTextureSampler( sutil::loadTexture( context, "", optix::make_float3(mat_params.Kd) ) );

  mat[ "Kd_mapped" ]->setInt( use_textures  );
  mat[ "Kd"        ]->set3fv( mat_params.Kd );
  mat[ "Ks"        ]->set3fv( mat_params.Ks );
  mat[ "Kr"        ]->set3fv( mat_params.Kr );
  mat[ "Ka"        ]->set3fv( mat_params.Ka );
  mat[ "phong_exp" ]->setFloat( mat_params.exp );

  return mat;
}


optix::Program createBoundingBoxProgram( optix::Context context )
{
  std::string path = std::string( sutil::samplesPTXDir() ) +
                     "/cuda_compile_ptx_generated_triangle_mesh.cu.ptx";
  return context->createProgramFromPTXFile( path, "mesh_bounds" );
}


optix::Program createIntersectionProgram( optix::Context context )
{
  std::string path = std::string( sutil::samplesPTXDir() ) +
                     "/cuda_compile_ptx_generated_triangle_mesh.cu.ptx";
  return context->createProgramFromPTXFile( path, "mesh_intersect" );
}


void translateMeshToOptiX(
    const Mesh&        mesh,
    const MeshBuffers& buffers,
    OptiXMesh&         optix_mesh
    )
{
  optix::Context ctx       = optix_mesh.context;
  optix_mesh.bbox_min      = optix::make_float3( mesh.bbox_min );
  optix_mesh.bbox_max      = optix::make_float3( mesh.bbox_max );
  optix_mesh.num_triangles = mesh.num_triangles;

  std::vector<optix::Material> optix_materials;
  if( optix_mesh.material )
  {
    // Rewrite all mat_indices to point to single override material
    memset( mesh.mat_indices, 0, mesh.num_triangles*sizeof(int32_t) );

    optix_materials.push_back( optix_mesh.material ); 
  }
  else 
  {
    bool have_textures = false;
    for( int32_t i = 0; i < mesh.num_materials; ++i )
      if( !mesh.mat_params[i].Kd_map.empty() ) 
        have_textures = true;

    optix::Program closest_hit = optix_mesh.closest_hit;
    optix::Program any_hit     = optix_mesh.any_hit;
    createMaterialPrograms( ctx, have_textures, closest_hit, any_hit );

    for( int32_t i = 0; i < mesh.num_materials; ++i )
      optix_materials.push_back( createOptiXMaterial(
            ctx,
            closest_hit,
            any_hit,
            mesh.mat_params[i],
            have_textures ) );
  }

  optix::Geometry geometry = ctx->createGeometry();  
  geometry[ "vertex_buffer"   ]->setBuffer( buffers.positions ); 
  geometry[ "normal_buffer"   ]->setBuffer( buffers.normals); 
  geometry[ "texcoord_buffer" ]->setBuffer( buffers.texcoords ); 
  geometry[ "material_buffer" ]->setBuffer( buffers.mat_indices); 
  geometry[ "index_buffer"    ]->setBuffer( buffers.tri_indices); 
  geometry->setPrimitiveCount     ( mesh.num_triangles );
  geometry->setBoundingBoxProgram ( optix_mesh.bounds ?
                                    optix_mesh.bounds :
                                    createBoundingBoxProgram( ctx ) );
  geometry->setIntersectionProgram( optix_mesh.intersection ?
                                    optix_mesh.intersection :
                                    createIntersectionProgram( ctx ) );

  optix_mesh.geom_instance = ctx->createGeometryInstance(
                                 geometry,
                                 optix_materials.begin(),
                                 optix_materials.end()
                                 );
}


} // namespace end


void loadMesh(
    const std::string&          filename,
    OptiXMesh&                  optix_mesh, 
    const optix::Matrix4x4&     load_xform
    )
{
  if( !optix_mesh.context )
  {
    throw std::runtime_error( "OptiXMesh: loadMesh() requires valid OptiX context" );
  }

  optix::Context context = optix_mesh.context;

  Mesh mesh;
  MeshLoader loader( filename );
  loader.scanMesh( mesh );

  MeshBuffers buffers;
  setupMeshLoaderInputs( context, buffers, mesh );

  loader.loadMesh( mesh, load_xform.getData() );

  translateMeshToOptiX( mesh, buffers, optix_mesh );

  unmap( buffers, mesh );
}
