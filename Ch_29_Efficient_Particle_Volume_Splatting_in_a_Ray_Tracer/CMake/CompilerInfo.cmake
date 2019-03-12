
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

# Sets some variables depending on which compiler you are using
#
# USING_GNU_C       : gcc is being used for C compiler
# USING_GNU_CXX     : g++ is being used for C++ compiler
# USING_CLANG_C     : gcc is being used for C compiler
# USING_CLANG_CXX   : g++ is being used for C++ compiler
# USING_ICC         : icc is being used for C compiler
# USING_ICPC        : icpc is being used for C++ compiler
# USING_WINDOWS_CL  : Visual Studio's compiler
# USING_WINDOWS_ICL : Intel's Windows compiler

set(USING_KNOWN_C_COMPILER TRUE)
if(CMAKE_COMPILER_IS_GNUCC)
  set(USING_GNU_C TRUE)
elseif( CMAKE_C_COMPILER_ID STREQUAL "Intel" )
  set(USING_ICC TRUE)
elseif( CMAKE_C_COMPILER_ID STREQUAL "Clang" )
  set(USING_CLANG_C TRUE)
elseif( MSVC OR "x${CMAKE_C_COMPILER_ID}" STREQUAL "xMSVC" )
  set(USING_WINDOWS_CL TRUE)
else()
  set(USING_KNOWN_C_COMPILER FALSE)
endif()


set(USING_KNOWN_CXX_COMPILER TRUE)
if(CMAKE_COMPILER_IS_GNUCXX)
  set(USING_GNU_CXX TRUE)
elseif( CMAKE_CXX_COMPILER_ID STREQUAL "Intel" )
  set(USING_ICPC TRUE)
elseif( CMAKE_CXX_COMPILER_ID STREQUAL "Clang" )
  set(USING_CLANG_CXX TRUE)
elseif( MSVC OR "x${CMAKE_C_COMPILER_ID}" STREQUAL "xMSVC" )
  if( NOT USING_WINDOWS_CL )
    message( WARNING "Mixing WinCL C++ compiler with non-matching C compiler" )
  endif()
else()
  set(USING_KNOWN_CXX_COMPILER FALSE)
endif()

if(USING_GNU_C)
  execute_process(COMMAND ${CMAKE_C_COMPILER} -dumpversion
    OUTPUT_VARIABLE GCC_VERSION)
endif()

# Using unknown compilers
if(NOT USING_KNOWN_C_COMPILER)
  FIRST_TIME_MESSAGE("Specified C compiler ${CMAKE_C_COMPILER} is not recognized (gcc, icc).  Using CMake defaults.")
endif()

if(NOT USING_KNOWN_CXX_COMPILER)
  FIRST_TIME_MESSAGE("Specified CXX compiler ${CMAKE_CXX_COMPILER} is not recognized (g++, icpc).  Using CMake defaults.")
endif()


if(USING_WINDOWS_CL)
  # We should set this macro as well to get our nice trig functions
  add_definitions(-D_USE_MATH_DEFINES)
  # Microsoft does some stupid things like #define min and max.
  add_definitions(-DNOMINMAX)
endif()
  

# Determine if the current compiler supports the C++11 'override' keyword
if( MSVC10 OR MSVC12 ) # TODO: make this more robust
  set( SUPPORTS_OVERRIDE TRUE )
else()
  set( SUPPORTS_OVERRIDE FALSE )
endif()


