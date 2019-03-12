#
# Copyright (c) 2016, NVIDIA CORPORATION. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#  * Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
#  * Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#  * Neither the name of NVIDIA CORPORATION nor the names of its
#    contributors may be used to endorse or promote products derived
#    from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
# OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#

# Welcome to the optix_advanced_samples build.  We have chosen CMake, because it can generate multiple
# build systems for multiple architectures from a single script.  There are many resources
# for CMake on-line at http://www.cmake.org and their wiki page,
# http://www.cmake.org/Wiki/CMake, in addition to the documentation that comes with the
# distribution.  There is also a book available if you wish to delve more deeply into
# various topics.

# If you wish to create your own project and use the files here as a template there are a number
# of things you should do.
# 
# 1. You can remove any sample's directory you don't wish to build.  Be careful about
#    the following directories.
#
#    a. CMake - contains helper scripts that make this all work.  You should keep this.
#
#    b. sutil and support
#              - Almost all of the samples make use of this shared code one way or another, so
#               you should probably keep it until you have your own frameowrk for your
#               code.
#
#    d. data  - This directory contains meshes, textures, etc, used as input for the samples.
#
#    e. device_include - This directory contains common device or device/host functions used by .cu files
#                 in some samples.
#
# 2. You should update the list of sub directories that CMake needs to process below (look
#    for the comment "List of samples found in subdirectories.")
#

# The basic flow of execution of this file is to do the following.
#
# 1. Setup the project and other global settings.  This involves processing some helper
#    scripts.
# 
# 2. Look for external dependencies, such as glut, DirectX, CUDA, and OptiX.
#
# 3. Process all the subdirectories' CMakeLists.txt files.  These files create all the
#    executable and library targets that are used to build the samples.
#
# 4. As a convenience on Windows, copy the OptiX dlls into the build directories, so OptiX
#    doesn't have to be in the path to run the samples.
#
# 5. Set a CMake variable that indicates we have configured for the first time.  This
#    allows us to override and set varibles' defaults while allowing them to be modified
#    later.

# If you have any questions, don't feel shy about posting to the OptiX forums:
# http://developer.nvidia.com/forums/index.php?showforum=43


# This sets up the name of our project.  For our purposes the main thing this controls is
# the name of the VS solution file.
project(OptiX-Advanced-Samples)

# This enforces a particular version of CMake that we require to process the script files
# properly.
cmake_minimum_required(VERSION 2.8.8 FATAL_ERROR)

# As of CMake 2.6 policies were introduced in order to provide a mechanism for
# adding backwards compatibility one feature at a time.  We will just specify
# that all policies will use version 2.6 symantics.
cmake_policy(VERSION 2.6)

# Add paths to our CMake code to the module path, so they can be found automatically by
# CMake.
set(CMAKE_MODULE_PATH
  "${CMAKE_SOURCE_DIR}/CMake"
  ${CMAKE_MODULE_PATH}
  )

# Set the default build to Release.  Note this doesn't do anything for the VS
# default build target which defaults to Debug when you first start it.
IF(NOT CMAKE_BUILD_TYPE)
  SET(CMAKE_BUILD_TYPE "Release" CACHE STRING
      "Choose the type of build, options are: Debug Release RelWithDebInfo MinSizeRel."
      FORCE)
ENDIF(NOT CMAKE_BUILD_TYPE)

# Tells CMake to build all the libraries as shared libraries by default.  This can be
# overrided by individual libraries later.
set(BUILD_SHARED_LIBS ON)

##########
# Process our custom setup scripts here.

# Include all CMake Macros.
include(Macros)
# Determine information about the compiler
include (CompilerInfo)
# Check for specific machine/compiler options.
include (ConfigCompilerFlags)

# Turn off the warning that NVCC issues when generating PTX from our CUDA samples.  This
# is a custom extension to the FindCUDA code distributed by CMake.
OPTION(CUDA_REMOVE_GLOBAL_MEMORY_SPACE_WARNING "Suppress the \"Advisory: Cannot tell what pointer points to, assuming global memory space\" warning nvcc makes." ON)

# For Xcode 5, gcc is actually clang, so we have to tell CUDA to treat the compiler as
# clang, so that it doesn't mistake it for something else.
if(USING_CLANG_C)
  set(CUDA_HOST_COMPILER "clang" CACHE FILEPATH "Host side compiler used by NVCC")
endif()

find_package(CUDA 7.0 REQUIRED)
find_package(OpenGL REQUIRED)

# Optional: When IL_FOUND is false after this call, the OptiX introduction samples optixIntro_07 and higher will not be built.
find_package(DevIL)

if ( OPENGL_FOUND AND OPENGL_INCLUDE_DIR )
  include_directories( ${OPENGL_INCLUDE_DIR} )
endif()

# Present the CUDA_64_BIT_DEVICE_CODE on the default set of options.
mark_as_advanced(CLEAR CUDA_64_BIT_DEVICE_CODE)

# Add some useful default arguments to the nvcc flags.  This is an example of how we use
# PASSED_FIRST_CONFIGURE.  Once you have configured, this variable is TRUE and following
# block of code will not be executed leaving you free to edit the values as much as you
# wish from the GUI or from ccmake.
if(NOT PASSED_FIRST_CONFIGURE)

  list(APPEND default_nvcc_flags "--use_fast_math" "-arch sm_30" "-lineinfo")

  if (CUDA_VERSION VERSION_LESS "3.0")
    list(APPEND default_nvcc_flags "--keep")
  endif()

  if( APPLE )
    # Undef'ing __BLOCKS__ for OSX builds.  This is due to a name clash between OSX 10.6
    # C headers and CUDA headers
    list(APPEND default_nvcc_flags "-U__BLOCKS__")
  endif()

  # Append to nvcc flags
  foreach(flag IN LISTS default_nvcc_flags)
    list(FIND CUDA_NVCC_FLAGS ${flag} index)
    if(index EQUAL -1)
      list(APPEND CUDA_NVCC_FLAGS ${flag})
      set(CUDA_NVCC_FLAGS ${CUDA_NVCC_FLAGS} CACHE LIST "Semi-colon delimit multiple arguments." FORCE)
    endif()
  endforeach(flag)

endif(NOT PASSED_FIRST_CONFIGURE)

# Add required nvcc flag for callable programs under CUDA 8
if (CUDA_VERSION VERSION_GREATER "7.5")
  set(flag "--keep-device-functions")
  list(FIND CUDA_NVCC_FLAGS ${flag} index)
  if(index EQUAL -1)
    list(APPEND CUDA_NVCC_FLAGS ${flag})
    set(CUDA_NVCC_FLAGS ${CUDA_NVCC_FLAGS} CACHE LIST "Semi-colon delimit multiple arguments." FORCE)
  endif()
endif()

# This passes a preprocessor definition to cl.exe when processing CUDA code.
if(USING_WINDOWS_CL)
  list(APPEND CUDA_NVCC_FLAGS --compiler-options /D_USE_MATH_DEFINES)
endif()

# Put all the runtime stuff in the same directory.  By default, CMake puts each targets'
# output into their own directory.  We want all the targets to be put in the same
# directory, and we can do this by setting these variables.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

# Create a flag for mac which will allow apps to add the local cuda toolkit
# install path to the app's rpath.
if( APPLE )
  set( CUDA_TOOLKIT_RPATH_FLAG "-Wl,-rpath,${CUDA_TOOLKIT_ROOT_DIR}/lib" )
endif()


# Search for the OptiX libraries and include files.
find_package(OptiX REQUIRED)

# Add the path to the OptiX headers to our include paths.
include_directories(
  "${OptiX_INCLUDE}"
  )

##################################################################
# SUtil compilation

set(SAMPLES_PTX_DIR "${CMAKE_BINARY_DIR}/lib/ptx")
set(SAMPLES_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

set(CUDA_GENERATED_OUTPUT_DIR ${SAMPLES_PTX_DIR})

if (WIN32)
  string(REPLACE "/" "\\\\" SAMPLES_PTX_DIR ${SAMPLES_PTX_DIR})
else (WIN32)
  if ( USING_GNU_C AND NOT APPLE)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DM_PI=3.14159265358979323846" )
  endif()
endif (WIN32)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/sampleConfig.h.in ${CMAKE_CURRENT_BINARY_DIR}/sampleConfig.h @ONLY)

# Path to sutil.h that all the samples need
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/sutil
                     ${CMAKE_CURRENT_SOURCE_DIR}
                     ${OptiX_INCLUDE}/optixu
                     ${CMAKE_CURRENT_BINARY_DIR}
                     ${CUDA_INCLUDE_DIRS} )


set(SAMPLES_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/device_include)

##################################################################
# IMGUI compilation
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/support )
add_subdirectory( support/imgui )

##################################################################
# GLFW compilation
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory( support/glfw )


#########################################################
# OPTIX_add_sample_executable
#
# Convience function for adding samples to the code.  You can copy the contents of this
# funtion into your individual project if you wish to customize the behavior.  Note that
# in CMake, functions have their own scope, whereas macros use the scope of the caller.
function(OPTIX_add_sample_executable target_name)

  # These calls will group PTX and CUDA files into their own directories in the Visual
  # Studio projects.
  source_group("PTX Files"  REGULAR_EXPRESSION ".+\\.ptx$")
  source_group("CUDA Files" REGULAR_EXPRESSION ".+\\.cu$")

  # Separate the sources from the CMake and CUDA options fed to the macro.  This code
  # comes from the CUDA_COMPILE_PTX macro found in FindCUDA.cmake.  We are copying the
  # code here, so that we can use our own name for the target.  target_name is used in the
  # creation of the output file names, and we want this to be unique for each target in
  # the SDK.
  CUDA_GET_SOURCES_AND_OPTIONS(source_files cmake_options options ${ARGN})

  # Create the rules to build the PTX from the CUDA files.
  CUDA_WRAP_SRCS( ${target_name} PTX generated_files ${source_files} ${cmake_options}
    OPTIONS ${options} )

  # Here is where we create the rule to make the executable.  We define a target name and
  # list all the source files used to create the target.  In addition we also pass along
  # the cmake_options parsed out of the arguments.
  add_executable(${target_name}
    ${source_files}
    ${generated_files}
    ${cmake_options}
    )

  # Most of the samples link against sutil, optix, and glfw.  Here is the
  # rule that specifies this linkage.
  target_link_libraries( ${target_name}
    sutil_sdk
    optix
    glfw
    imgui
    ${OPENGL_gl_LIBRARY}
    ${optix_rpath}
    )
  if(USING_GNU_CXX)
    target_link_libraries( ${target_name} m ) # Explicitly link against math library (C samples don't do that by default)
  endif()
endfunction()

#########################################################
#  List of samples found in subdirectories.
#
# If you wish to start your own sample, you can copy one of the sample's directories.
# Just make sure you rename all the occurances of the sample's name in the C code as well
# and the CMakeLists.txt file.

add_subdirectory(optixParticleVolumes)


# Our sutil library.  The rules to build it are found in the subdirectory.
add_subdirectory(sutil)

# This copies out dlls into the build directories, so that users no longer need to copy
# them over in order to run the samples.  This depends on the optixHello sample being compiled.
# If you remove this sample from the list of compiled samples, then you should change
# "optixHello" found below to the name of one of your other samples.
if(WIN32)
  if(CMAKE_SIZEOF_VOID_P EQUAL 8 AND NOT APPLE)
    set(bit_dest "64")
  else()
    set(bit_dest "")
  endif()
  foreach(config ${CMAKE_CONFIGURATION_TYPES})
    cmake_policy(SET CMP0026 OLD)  # disable warning about LOCATION property
    get_target_property(loc optixHello ${config}_LOCATION)
    if(loc)
      # A little helper function
      function(copy_dll lib)
        get_filename_component(path ${loc} PATH)
        get_filename_component(name ${lib} NAME)
        #message("${CMAKE_COMMAND} -E copy_if_different ${lib} ${path}/${name}")
        execute_process(COMMAND ${CMAKE_COMMAND} -E copy_if_different ${lib} ${path}/${name})
      endfunction()
      
      # Copy OptiX and optional DevIL dlls from their binary directories into the build
      get_filename_component( path_to_optix_dll ${optix_DLL} PATH )
      file(GLOB dlls "${path_to_optix_dll}/*.dll")
      if (IL_FOUND)
        get_filename_component( path_to_devil_dll ${IL_LIBRARIES} PATH )
        file(GLOB devil_dlls "${path_to_devil_dll}/*.dll")
        list(APPEND dlls ${devil_dlls})
      endif()
      foreach(file ${dlls})
        copy_dll("${file}")
      endforeach()
    else()
      message(WARNING "Unable to find location to copy DLLs into the build")
    endif()
  endforeach()
endif(WIN32)

#################################################################

# Now that everything is done, indicate that we have finished configuring at least once.
# We use this variable to set certain defaults only on the first pass, so that we don't
# continually set them over and over again.
set(PASSED_FIRST_CONFIGURE ON CACHE INTERNAL "Already Configured once?")

