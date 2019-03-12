
#
#  Copyright (c) 2008 - 2009 NVIDIA Corporation.  All rights reserved.
#
#  NVIDIA Corporation and its licensors retain all intellectual property and proprietary
#  rights in and to this software, related documentation and any modifications thereto.
#  Any use, reproduction, disclosure or distribution of this software and related
#  documentation without an express license agreement from NVIDIA Corporation is strictly
#  prohibited.
#
#  TO THE MAXIMUM EXTENT PERMITTED BY APPLICABLE LAW, THIS SOFTWARE IS PROVIDED *AS IS*
#  AND NVIDIA AND ITS SUPPLIERS DISCLAIM ALL WARRANTIES, EITHER EXPRESS OR IMPLIED,
#  INCLUDING, BUT NOT LIMITED TO, IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
#  PARTICULAR PURPOSE.  IN NO EVENT SHALL NVIDIA OR ITS SUPPLIERS BE LIABLE FOR ANY
#  SPECIAL, INCIDENTAL, INDIRECT, OR CONSEQUENTIAL DAMAGES WHATSOEVER (INCLUDING, WITHOUT
#  LIMITATION, DAMAGES FOR LOSS OF BUSINESS PROFITS, BUSINESS INTERRUPTION, LOSS OF
#  BUSINESS INFORMATION, OR ANY OTHER PECUNIARY LOSS) ARISING OUT OF THE USE OF OR
#  INABILITY TO USE THIS SOFTWARE, EVEN IF NVIDIA HAS BEEN ADVISED OF THE POSSIBILITY OF
#  SUCH DAMAGES
#

# This script copies one of two supplied dlls into the build directory based on the build configuration.

# build_configuration - Should be passed in via:

#   if(CMAKE_GENERATOR MATCHES "Visual Studio")
#     set( build_configuration "$(ConfigurationName)" )
#   else()
#     set( build_configuration "${CMAKE_BUILD_TYPE}")
#   endif()
#
#  -D build_configuration:STRING=${build_configuration}

# output_directory - should be passed in via the following.  If not supplied the current output directory is used.
#
#  -D "output_directory:PATH=${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${CMAKE_CFG_INTDIR}"

# source_dll - should be the release version or the single version if you don't have a debug version
#
#  -D "source_dll:FILE=${path_to_source_dll}"

# source_debug_dll - should be the debug version of the dll (optional)
#
#  -D "source_debug_dll:FILE=${path_to_source_debug_dll}"

if(NOT DEFINED build_configuration)
  message(FATAL_ERROR "build_configuration not specified")
endif()

if(NOT DEFINED output_directory)
  set(output_directory ".")
endif()

if(NOT DEFINED source_dll)
  message(FATAL_ERROR "source_dll not specified")
endif()

if(NOT DEFINED source_debug_dll)
  set(source_debug_dll "${source_dll}")
endif()

# Compute the file name
if(build_configuration STREQUAL Debug)
  set(source "${source_debug_dll}")
else()
  set(source "${source_dll}")
endif()

get_filename_component(filename "${source}" NAME)
set(dest "${output_directory}/${filename}")
message(STATUS "Copying if different ${source} to ${dest}")
execute_process(COMMAND ${CMAKE_COMMAND} -E copy_if_different "${source}" "${dest}"
  RESULT_VARIABLE result
  )
if(result)
  message(FATAL_ERROR "Error copying dll")
endif()

