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


#include <optixu/optixu_matrix.h>
#include <optixu/optixu_math_stream_namespace.h>

#include "Mesh.h" 
#include "rply-1.01/rply.h"
#include "tinyobjloader/tiny_obj_loader.h"
#include <algorithm>
#include <iostream>
#include <locale>
#include <stdexcept>
#include <stdint.h>
#include <vector>

//------------------------------------------------------------------------------
//
// Helpers 
//
//------------------------------------------------------------------------------

namespace
{

void clearMesh( Mesh& mesh )
{
  memset( &mesh, 0, sizeof( mesh ) );
}


bool checkValid( const Mesh& mesh )
{
  if( mesh.num_vertices  == 0 )
  {
    std::cerr << "Mesh not valid: num_vertices = 0" << std::endl;
    return false;
  }
  if( mesh.positions == 0 )
  {
    std::cerr << "Mesh not valid: positions = NULL" << std::endl;
    return false;
  }
  if( mesh.num_triangles == 0 )
  {
    std::cerr << "Mesh not valid: num_triangles = 0" << std::endl;
    return false;
  }
  if( mesh.tri_indices == 0 )
  {
    std::cerr << "Mesh not valid: tri_indices = NULL" << std::endl;
    return false;
  }
  if( mesh.mat_indices == 0 )
  {
    std::cerr << "Mesh not valid: mat_indices = NULL" << std::endl;
    return false;
  }
  if( mesh.has_normals && !mesh.normals )
  {
    std::cerr << "Mesh has normals, but normals is NULL" << std::endl;
    return false;
  }
  if( mesh.has_texcoords && !mesh.texcoords )
  {
    std::cerr << "Mesh has texcoords, but texcoords is NULL" << std::endl;
    return false;
  }
  if ( mesh.num_materials == 0 )
  {
    std::cerr << "Mesh not valid: num_materials = 0" << std::endl;
    return false;
  }
  if ( mesh.mat_params == 0 )
  {
    std::cerr << "Mesh not valid: mat_params = 0" << std::endl;
    return false;
  }

  return true;
}


std::string directoryOfFilePath( const std::string& filepath )                 
{                                                                              
  size_t slash_pos, backslash_pos;                                             
  slash_pos     = filepath.find_last_of( '/' );                                
  backslash_pos = filepath.find_last_of( '\\' );                               

  size_t break_pos;                                                            
  if( slash_pos == std::string::npos && backslash_pos == std::string::npos ) { 
    return std::string();                                                      
  } else if ( slash_pos == std::string::npos ) {                               
    break_pos = backslash_pos;                                                 
  } else if ( backslash_pos == std::string::npos ) {                           
    break_pos = slash_pos;                                                     
  } else {                                                                     
    break_pos = std::max(slash_pos, backslash_pos);                            
  }                                                                            

  // Include the final slash                                                   
  return filepath.substr(0, break_pos + 1);                                    
}


std::string getExtension( const std::string& filename )                        
{                                                                              
  // Get the filename extension                                                
  std::string::size_type extension_index = filename.find_last_of( "." );       
  std::string ext =  extension_index != std::string::npos ?                                
                     filename.substr( extension_index+1 ) :                                
                     std::string();                                                        
  std::locale loc;
  for ( std::string::size_type i=0; i < ext.length(); ++i )
    ext[i] = std::tolower( ext[i], loc );

  return ext;
}                                                                              


bool fileIsOBJ( const std::string& filename )                                  
{                                                                              
  return getExtension( filename ) == "obj";                                    
}                                                                              


bool fileIsPLY( const std::string& filename )                                  
{                                                                              
  return getExtension( filename ) == "ply";                                    
}
  

struct PlyData
{
  Mesh* mesh;
  int32_t cur_vertex;
  int32_t cur_index;
};


int plyLoadVertex( p_ply_argument argument ) 
{
  int coord_index;
  PlyData* data;
  ply_get_argument_user_data( argument, reinterpret_cast<void**>( &data ), &coord_index );

  float value = static_cast<float>( ply_get_argument_value( argument ) );

  switch( coord_index )
  {
    // Vertex property
    case 0: 
      data->mesh->positions[3*data->cur_vertex+0] = value;
      data->mesh->bbox_min[0] = std::min( data->mesh->bbox_min[0], value );
      data->mesh->bbox_max[0] = std::max( data->mesh->bbox_max[0], value );
      break;
    case 1: 
      data->mesh->positions[3*data->cur_vertex+1] = value;
      data->mesh->bbox_min[1] = std::min( data->mesh->bbox_min[1], value );
      data->mesh->bbox_max[1] = std::max( data->mesh->bbox_max[1], value );
      break;
    case 2:
      data->mesh->positions[3*data->cur_vertex+2] = value;
      data->mesh->bbox_min[2] = std::min( data->mesh->bbox_min[2], value );
      data->mesh->bbox_max[2] = std::max( data->mesh->bbox_max[2], value );
      if( !data->mesh->has_normals )
        ++data->cur_vertex;
      break;

    // Normal property
    case 3: 
      data->mesh->normals[3*data->cur_vertex+0] = value;
      break;
    case 4:
      data->mesh->normals[3*data->cur_vertex+1] = value;
      break;
    case 5: 
      data->mesh->normals[3*data->cur_vertex+2] = value;
      ++data->cur_vertex;
      break;

      // Silently ignore other coord_index values
  }
  return 1;
}
  

int plyLoadFace( p_ply_argument argument )
{
  PlyData* data;
  ply_get_argument_user_data( argument, reinterpret_cast<void**>( &data ), 0 );

  int num_verts, which_vertex;
  ply_get_argument_property( argument, NULL, &num_verts, &which_vertex );

  int32_t value = static_cast<int32_t>( ply_get_argument_value(argument) );

  // num_verts is disregarded; we assume only triangles are given

  switch( which_vertex ) {
    case 0:
      data->mesh->tri_indices[3*data->cur_index + 0] = value;
      break;
    case 1:
      data->mesh->tri_indices[3*data->cur_index + 1] = value;
      break;
    case 2:
      data->mesh->tri_indices[3*data->cur_index + 2] = value;
      ++data->cur_index;
      break;

    // Silently ignore other values of which_vertex
  }

  return 1;
}

  
void applyLoadXForm( Mesh& mesh, const float* load_xform )
{
  if( !load_xform )
      return;

  bool have_matrix = false;
  for( int32_t i = 0; i < 16; ++i )
    if( load_xform[i] != 0.0f )
      have_matrix = true;

  if( have_matrix )
  {
    mesh.bbox_min[0] = mesh.bbox_min[1] = mesh.bbox_min[2] =  1e16f;
    mesh.bbox_max[0] = mesh.bbox_max[1] = mesh.bbox_max[2] = -1e16f;

    optix::Matrix4x4 mat( load_xform );

    float3* positions = reinterpret_cast<float3*>( mesh.positions );
    for( int32_t i = 0; i < mesh.num_vertices; ++i )
    {
      const float3 v = make_float3( mat*make_float4( positions[i], 1.0f ) );
      positions[i] = v; 
      mesh.bbox_min[0] = std::min<float>( mesh.bbox_min[0], v.x );
      mesh.bbox_min[1] = std::min<float>( mesh.bbox_min[1], v.y );
      mesh.bbox_min[2] = std::min<float>( mesh.bbox_min[2], v.z );
      mesh.bbox_max[0] = std::max<float>( mesh.bbox_max[0], v.x );
      mesh.bbox_max[1] = std::max<float>( mesh.bbox_max[1], v.y );
      mesh.bbox_max[2] = std::max<float>( mesh.bbox_max[2], v.z );
    }

    if( mesh.has_normals )
    {
      mat = mat.inverse().transpose();
      float3* normals = reinterpret_cast<float3*>( mesh.normals );
      for( int32_t i = 0; i < mesh.num_vertices; ++i )
        normals[i] = make_float3( mat*make_float4( normals[i], 1.0f ) );
    }
  }
}

} 

//------------------------------------------------------------------------------
//
// MeshLoader implementation class
//
//------------------------------------------------------------------------------

class MeshLoader::Impl
{
public:
  Impl( const std::string& filename );
  
  void scanMesh( Mesh& mesh );
  void loadMesh( Mesh& mesh, const float* load_xform );

  void scanMeshOBJ( Mesh& mesh );
  void scanMeshPLY( Mesh& mesh );

  void loadMeshOBJ( Mesh& mesh );
  void loadMeshPLY( Mesh& mesh );
private:
  enum FileType
  {
    OBJ = 0,
    PLY,
    UNKNOWN
  };
  std::string                         m_filename;
  FileType                            m_filetype;
  
  std::vector<tinyobj::shape_t>       m_shapes;
  std::vector<tinyobj::material_t>    m_materials;
};


MeshLoader::Impl::Impl( const std::string& filename )
  : m_filename( filename )
{
   if( fileIsOBJ( m_filename ) )
     m_filetype = OBJ;
   else if( fileIsPLY( m_filename ) )
     m_filetype = PLY;
   else 
     m_filetype = UNKNOWN;
}


void MeshLoader::Impl::scanMesh( Mesh& mesh )
{
  clearMesh( mesh );

  if( m_filetype == OBJ )
    scanMeshOBJ( mesh );
  else if( m_filetype == PLY )
    scanMeshPLY( mesh );
  else
    throw std::runtime_error( "MeshLoader: Unsupported file type for '" + m_filename + "'" );
}


void MeshLoader::Impl::loadMesh( Mesh& mesh, const float* load_xform )
{
  if( !checkValid( mesh ) )
  {
    std::cerr << "MeshLoader - ERROR: Attempted to load mesh '" << m_filename
              << "' into invalid mesh struct:" << std::endl;
    printMeshInfo( mesh, std::cerr );
    return;
  }
  
  mesh.bbox_min[0] = mesh.bbox_min[1] = mesh.bbox_min[2] =  1e16f;
  mesh.bbox_max[0] = mesh.bbox_max[1] = mesh.bbox_max[2] = -1e16f;

  if( m_filetype == OBJ )
    loadMeshOBJ( mesh );
  else if( m_filetype == PLY )
    loadMeshPLY( mesh );
  else
    throw std::runtime_error( "MeshLoader: Unsupported file type for '" + m_filename + "'" );

  applyLoadXForm( mesh, load_xform );
}


void MeshLoader::Impl::scanMeshPLY( Mesh& mesh )
{
  p_ply ply = ply_open( m_filename.c_str(), 0 );                       

  if( !ply )
    throw std::runtime_error( "MeshLoader: Unable to open '" + m_filename + "'" );

  if( !ply_read_header( ply ) )
    throw std::runtime_error( "MeshLoader: Unable to read PLY header '" + m_filename + "'" );

  // Setting callbacks reports the number of corresponding property elements
  mesh.num_vertices  = ply_set_read_cb( ply, "vertex", "x",              NULL, NULL, 0 );
  mesh.has_normals   = ply_set_read_cb( ply, "vertex", "nx",             NULL, NULL, 3 ) != 0;
  mesh.num_triangles = ply_set_read_cb( ply, "face",   "vertex_indices", NULL, NULL, 0 );
  
  mesh.has_texcoords = false;
  
  mesh.num_materials = 1; // default material

  ply_close( ply );
} 


void MeshLoader::Impl::loadMeshPLY( Mesh& mesh )
{
  p_ply ply = ply_open( m_filename.c_str(), 0 );                       

  if( !ply )
    throw std::runtime_error( "MeshLoader: Unable to open '" + m_filename + "'" );

  if( !ply_read_header( ply ) )
    throw std::runtime_error( "MeshLoader: Unable to read PLY header '" + m_filename + "'" );
  
  PlyData ply_data = {0};
  ply_data.mesh = &mesh;

  ply_set_read_cb( ply, "vertex", "x",  plyLoadVertex, &ply_data, 0 );
  ply_set_read_cb( ply, "vertex", "y",  plyLoadVertex, &ply_data, 1 );
  ply_set_read_cb( ply, "vertex", "z",  plyLoadVertex, &ply_data, 2 );
  ply_set_read_cb( ply, "vertex", "nx", plyLoadVertex, &ply_data, 3 );
  ply_set_read_cb( ply, "vertex", "ny", plyLoadVertex, &ply_data, 4 );
  ply_set_read_cb( ply, "vertex", "nz", plyLoadVertex, &ply_data, 5 );
  ply_set_read_cb( ply, "face", "vertex_indices", plyLoadFace, &ply_data, 0);

  if( !ply_read( ply ) ) 
    throw std::runtime_error( "MeshLoader: Error parsing ply file (" + m_filename + ")" );
  ply_close( ply );


  // Fill in default white matte material

  MaterialParams mat; 
  mat.Kd[0] = mat.Kd[1] = mat.Kd[2] = 0.7f;
  mat.Ks[0] = mat.Ks[1] = mat.Ks[2] = 0.0f;
  mat.Kr[0] = mat.Kr[1] = mat.Kr[2] = 0.0f;
  mat.Ka[0] = mat.Ka[1] = mat.Ka[2] = 0.0f;
  mat.exp   = 0.0f;
  mesh.mat_params[0] = mat;

  // And assign this material to all triangles
  for( int32_t i = 0; i < mesh.num_triangles; ++i )
    mesh.mat_indices[i] = 0;
}


void MeshLoader::Impl::scanMeshOBJ( Mesh& mesh )
{
  if( m_shapes.empty() )
  {
    std::string err;
    bool ret = tinyobj::LoadObj( 
        m_shapes,
        m_materials,
        err, 
        m_filename.c_str(),
        directoryOfFilePath( m_filename ).c_str()
        );

    if( !err.empty() )
      std::cerr << err << std::endl;

    if( !ret )
      throw std::runtime_error( "MeshLoader: " + err );
  }

  //
  // Iterate over all shapes and sum up number of vertices and triangles
  //
  uint64_t num_groups_with_normals   = 0;
  uint64_t num_groups_with_texcoords = 0;
  for( std::vector<tinyobj::shape_t>::const_iterator it = m_shapes.begin();
       it < m_shapes.end();
       ++it )
  {
    const tinyobj::shape_t & shape = *it;

    mesh.num_triangles += static_cast<int32_t>(shape.mesh.indices.size()) / 3;
    mesh.num_vertices  += static_cast<int32_t>(shape.mesh.positions.size()) / 3;

    if( !shape.mesh.normals.empty() )
      ++num_groups_with_normals; 

    if( !shape.mesh.texcoords.empty() )
      ++num_groups_with_texcoords; 
  }

  //
  // We ignore normals and texcoords unless they are present for all shapes
  //

  if( num_groups_with_normals != 0 )
  {
    if( num_groups_with_normals != m_shapes.size() )
      std::cerr << "MeshLoader - WARNING: mesh '" << m_filename 
                << "' has normals for some groups but not all.  "
                << "Ignoring all normals." << std::endl;
    else
      mesh.has_normals = true;

  }
  
  if( num_groups_with_texcoords != 0 )
  {
    if( num_groups_with_texcoords != m_shapes.size() )
      std::cerr << "MeshLoader - WARNING: mesh '" << m_filename 
                << "' has texcoords for some groups but not all.  "
                << "Ignoring all texcoords." << std::endl;
    else
      mesh.has_texcoords = true;
  }

  mesh.num_materials = (int32_t) m_materials.size();
}


void MeshLoader::Impl::loadMeshOBJ( Mesh& mesh )
{
  uint32_t vrt_offset = 0;
  uint32_t tri_offset = 0;
  for( std::vector<tinyobj::shape_t>::const_iterator it = m_shapes.begin();
       it < m_shapes.end();
       ++it )
  {
    const tinyobj::shape_t & shape = *it;
    for( uint64_t i = 0; i < shape.mesh.positions.size() / 3; ++i )
    {
      const float x = shape.mesh.positions[i*3+0];
      const float y = shape.mesh.positions[i*3+1];
      const float z = shape.mesh.positions[i*3+2];

      mesh.bbox_min[0] = std::min<float>( mesh.bbox_min[0], x );
      mesh.bbox_min[1] = std::min<float>( mesh.bbox_min[1], y );
      mesh.bbox_min[2] = std::min<float>( mesh.bbox_min[2], z );

      mesh.bbox_max[0] = std::max<float>( mesh.bbox_max[0], x );
      mesh.bbox_max[1] = std::max<float>( mesh.bbox_max[1], y );
      mesh.bbox_max[2] = std::max<float>( mesh.bbox_max[2], z );

      mesh.positions[ vrt_offset*3 + i*3+0 ] = x; 
      mesh.positions[ vrt_offset*3 + i*3+1 ] = y; 
      mesh.positions[ vrt_offset*3 + i*3+2 ] = z; 
    }

    if( mesh.has_normals )
      for( uint64_t i = 0; i < shape.mesh.normals.size(); ++i )
        mesh.normals[ vrt_offset*3 + i ] = shape.mesh.normals[i];
    
    if( mesh.has_texcoords )
      for( uint64_t i = 0; i < shape.mesh.texcoords.size(); ++i )
        mesh.texcoords[ vrt_offset*2 + i ] = shape.mesh.texcoords[i];
    
    for( uint64_t i = 0; i < shape.mesh.indices.size() / 3; ++i )
    {
      mesh.tri_indices[ tri_offset*3 + i*3+0 ] = shape.mesh.indices[i*3+0] + vrt_offset;
      mesh.tri_indices[ tri_offset*3 + i*3+1 ] = shape.mesh.indices[i*3+1] + vrt_offset;
      mesh.tri_indices[ tri_offset*3 + i*3+2 ] = shape.mesh.indices[i*3+2] + vrt_offset;
      mesh.mat_indices[ tri_offset + i ] = shape.mesh.material_ids[i] >= 0 ? 
                                           shape.mesh.material_ids[i]      :
                                           0; 
    }

    vrt_offset += static_cast<uint32_t>(shape.mesh.positions.size()) / 3;
    tri_offset += static_cast<uint32_t>(shape.mesh.indices.size()) / 3;
  }

  for( uint64_t i = 0; i < m_materials.size(); ++i )
  {
    MaterialParams mat_params;

    mat_params.name   = m_materials[i].name;

    mat_params.Kd_map = m_materials[i].diffuse_texname.empty() ? "" :
                        directoryOfFilePath( m_filename ) + m_materials[i].diffuse_texname;

    mat_params.Kd[0]  = m_materials[i].diffuse[0];
    mat_params.Kd[1]  = m_materials[i].diffuse[1];
    mat_params.Kd[2]  = m_materials[i].diffuse[2];
    
    mat_params.Ks[0]  = m_materials[i].specular[0];
    mat_params.Ks[1]  = m_materials[i].specular[1];
    mat_params.Ks[2]  = m_materials[i].specular[2];

    mat_params.Ka[0]  = m_materials[i].ambient[0];
    mat_params.Ka[1]  = m_materials[i].ambient[1];
    mat_params.Ka[2]  = m_materials[i].ambient[2];

    mat_params.Kr[0]  = m_materials[i].specular[0];
    mat_params.Kr[1]  = m_materials[i].specular[1];
    mat_params.Kr[2]  = m_materials[i].specular[2];

    mat_params.exp    = m_materials[i].shininess;

    mesh.mat_params[i] = mat_params;
  }
}


//------------------------------------------------------------------------------
//
//  Mesh API free functions
//
//------------------------------------------------------------------------------

void printMaterialInfo( const MaterialParams& mat, std::ostream& out )
{
  out << "MaterialParams[ " << mat.name << " ]:" << std::endl
      << "\tmat.Kd map: '" << mat.Kd_map << "'" << std::endl 
      << "\tmat.Kd val: ( " << mat.Kd[0] << ", " << mat.Kd[1] << ", " << mat.Kd[2] << " )" << std::endl
      << "\tmat.Ks val: ( " << mat.Ks[0] << ", " << mat.Ks[1] << ", " << mat.Ks[2] << " )" << std::endl
      << "\tmat.Kr val: ( " << mat.Kr[0] << ", " << mat.Kr[1] << ", " << mat.Kr[2] << " )" << std::endl
      << "\tmat.Ka val: ( " << mat.Ka[0] << ", " << mat.Ka[1] << ", " << mat.Ka[2] << " )" << std::endl
      << "\tExp   : " << mat.exp << std::endl;
}


void printMeshInfo( const Mesh& mesh, std::ostream& out )
{
  out << "Mesh:" << std::endl
      << "\tnum vertices : " << mesh.num_vertices  << std::endl
      << "\thas normals  : " << mesh.has_normals   << std::endl
      << "\thas texcoords: " << mesh.has_texcoords << std::endl
      << "\tnum triangles: " << mesh.num_triangles << std::endl
      << "\tnum materials: " << mesh.num_materials << std::endl
      << "\tbbox min     : ( " << mesh.bbox_min[0] << ", "
                               << mesh.bbox_min[1] << ", "
                               << mesh.bbox_min[2] << " )"
                               << std::endl 
      << "\tbbox max     : ( " << mesh.bbox_max[0] << ", "
                               << mesh.bbox_max[1] << ", "
                               << mesh.bbox_max[2] << " )"
                               << std::endl;
  /*
  if( mesh.positions )
  {
    out << "\tpositions:" << std::endl;
    for( uint64_t i = 0; i < mesh.num_vertices; ++i )
      out << "\t\t( " << mesh.positions[i*3+0] << ", "
                      << mesh.positions[i*3+1] << ", "
                      << mesh.positions[i*3+2] << " )"
                      << std::endl;
  }
  if( mesh.tri_indices )
  {
    out << "\ttri indices:" << std::endl;
    for( uint64_t i = 0; i < mesh.num_triangles; ++i )
      out << "\t\t( " << mesh.tri_indices[i*3+0] << ", "
                      << mesh.tri_indices[i*3+1] << ", "
                      << mesh.tri_indices[i*3+2] << " )"
                      << std::endl;
  }
  if( mesh.mat_indices )
  {
    out << "\tmat indices:" << std::endl;
    for( uint64_t i = 0; i < mesh.num_triangles; ++i )
      out << "\t\t" << mesh.mat_indices[i] << std::endl; 
  }
  */
}


SUTILAPI void allocMesh( Mesh& mesh )
{

  if( mesh.num_vertices == 0 || mesh.num_triangles == 0 )
  {
    clearMesh( mesh );
    return;
  }

  mesh.positions   = new float[ 3*mesh.num_vertices ];
  mesh.normals     = mesh.has_normals   ? new float[ 3*mesh.num_vertices ]   : 0;
  mesh.texcoords   = mesh.has_texcoords ? new float[ 2*mesh.num_vertices ]   : 0;
  mesh.tri_indices = new int32_t[ 3*mesh.num_triangles ]; 
  mesh.mat_indices = new int32_t[ 1*mesh.num_triangles ]; 

  mesh.mat_params  = new MaterialParams[ mesh.num_materials ];
}


SUTILAPI void freeMesh( Mesh& mesh )
{
  delete [] mesh.positions;
  delete [] mesh.normals;
  delete [] mesh.texcoords;
  delete [] mesh.tri_indices;
  delete [] mesh.mat_indices;
  delete [] mesh.mat_params;

  clearMesh( mesh );
}


//------------------------------------------------------------------------------
//
//  Mesh API MeshLoader class 
//
//------------------------------------------------------------------------------

MeshLoader::MeshLoader( const std::string& filename )
  : p_impl( new Impl( filename ) )
{
}


MeshLoader::~MeshLoader()
{
  delete p_impl;
}


void MeshLoader::scanMesh( Mesh& mesh )
{
  p_impl->scanMesh( mesh );
}


void MeshLoader::loadMesh( Mesh& mesh, const float* load_xform )
{
  p_impl->loadMesh( mesh, load_xform );
}

//------------------------------------------------------------------------------
//
// Mesh Loader convenience  functions
//
//------------------------------------------------------------------------------


void loadMesh( const std::string& filename, Mesh& mesh, const float* xform )
{
    MeshLoader loader( filename );
    loader.scanMesh( mesh );
    allocMesh( mesh );
    loader.loadMesh( mesh, xform );
}
