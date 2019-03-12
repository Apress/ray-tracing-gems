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

#include <sutilapi.h>

#include <cstring>
#include <iostream>
#include <stdint.h>
#include <string>
#include <memory>


//------------------------------------------------------------------------------
//
// Struct for describing material parameters
//
//------------------------------------------------------------------------------
struct MaterialParams
{
  std::string         name;

  std::string         Kd_map;
  float               Kd[3];
  float               Ks[3];
  float               Kr[3];
  float               Ka[3];
  float               exp;

};


//------------------------------------------------------------------------------
//
// Mesh data structure
//
//------------------------------------------------------------------------------
struct Mesh
{
  int32_t             num_vertices;   // Number of triangle vertices
  float*              positions;      // Triangle vertex positions (len num_vertices)

  bool                has_normals;    //
  float*              normals;        // Triangle normals (len 0 or num_vertices)

  bool                has_texcoords;  //
  float*              texcoords;      // Triangle UVs (len 0 or num_vertices)


  int32_t             num_triangles;  // Number of triangles
  int32_t*            tri_indices;    // Indices into positions, normals, texcoords
  int32_t*            mat_indices;    // Indices into mat_params (len num_triangles)

  float               bbox_min[3];    // Scene BBox
  float               bbox_max[3];    //

  int32_t             num_materials;
  MaterialParams*     mat_params;     // Material params
};

//------------------------------------------------------------------------------
//
// Mesh convenience functions
//
//------------------------------------------------------------------------------

// Allocates memory for mesh using std lib new.
// Assumes num_vertices, has_normals, has_texcoords, num_triangles initialized.
SUTILAPI void allocMesh( Mesh& mesh );

// Calls std lib delete on non-null arrays in mesh
SUTILAPI void freeMesh( Mesh& mesh );

SUTILAPI void printMaterialInfo( const MaterialParams& mat, std::ostream& out = std::cout );
SUTILAPI void printMeshInfo    ( const Mesh& mesh,          std::ostream& out = std::cout );


//------------------------------------------------------------------------------
//
// Mesh Loader
//
//------------------------------------------------------------------------------
class MeshLoader
{
public:
  SUTILAPI MeshLoader( const std::string& filename );
  SUTILAPI ~MeshLoader();
  SUTILAPI void scanMesh( Mesh& mesh );
  SUTILAPI void loadMesh( Mesh& mesh, const float* load_xform=0 );

private:
  class Impl;
  Impl* p_impl;
};


//------------------------------------------------------------------------------
//
// Mesh Loader convenience  functions
//
//------------------------------------------------------------------------------


// Load mesh using std lib new for allocations
SUTILAPI void loadMesh( const std::string& filename, Mesh& mesh, const float* load_xform=0 );



//------------------------------------------------------------------------------
//
// Host mesh with RAII resource control of vertex/index arrays
//
//------------------------------------------------------------------------------

class HostMesh : public Mesh
{
public:
  HostMesh( const std::string& filename, const float* xform=0 )
  { 
    loadMesh( filename, *this, xform ); 
  }

  ~HostMesh()
  {
    freeMesh( *this );
  }
};
