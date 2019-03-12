
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

# Appends VAL to the string contained in STR
MACRO(APPEND_TO_STRING STR VAL)
  if (NOT "${ARGN}" STREQUAL "")
    message(SEND_ERROR "APPEND_TO_STRING takes only a single argument to append.  Offending args: ${ARGN}")
  endif()
  # You need to double ${} STR to get the value.  The first one gets
  # the variable, the second one gets the value.
  if (${STR})
    set(${STR} "${${STR}} ${VAL}")
  else()
    set(${STR} "${VAL}")
  endif()
ENDMACRO(APPEND_TO_STRING)

# Prepends VAL to the string contained in STR
MACRO(PREPEND_TO_STRING STR VAL)
  if (NOT "${ARGN}" STREQUAL "")
    message(SEND_ERROR "PREPEND_TO_STRING takes only a single argument to append.  Offending args: ${ARGN}")
  endif()
  # You need to double ${} STR to get the value.  The first one gets
  # the variable, the second one gets the value.
  if (${STR})
    set(${STR} "${VAL} ${${STR}}")
  else()
    set(${STR} "${VAL}")
  endif()
ENDMACRO(PREPEND_TO_STRING)

# Prepends a prefix to items in a list and appends the result to list_out
macro( prepend list_out prefix )
  set( _results )
  foreach( str ${ARGN} )
    list( APPEND _results "${prefix}${str}" )
  endforeach()
  list( APPEND ${list_out} ${_results} )
endmacro()


#################################################################
#  FORCE_ADD_FLAGS(parameter flags)
#
# This will add arguments not found in ${parameter} to the end.  It
# does not attempt to remove duplicate arguments already existing in
# ${parameter}.
#################################################################
MACRO(FORCE_ADD_FLAGS parameter)
  # Create a separated list of the arguments to loop over
  SET(p_list ${${parameter}})
  SEPARATE_ARGUMENTS(p_list)
  # Make a copy of the current arguments in ${parameter}
  SET(new_parameter ${${parameter}})
  # Now loop over each required argument and see if it is in our
  # current list of arguments.
  FOREACH(required_arg ${ARGN})
    # This helps when we get arguments to the function that are
    # grouped as a string:
    #
    # ["-msse -msse2"]  instead of [-msse -msse2]
    SET(TMP ${required_arg}) #elsewise the Seperate command doesn't work)
    SEPARATE_ARGUMENTS(TMP)
    FOREACH(option ${TMP})
      # Look for the required argument in our list of existing arguments
      SET(found FALSE)
      FOREACH(p_arg ${p_list})
        IF (${p_arg} STREQUAL ${option})
          SET(found TRUE)
        ENDIF (${p_arg} STREQUAL ${option})
      ENDFOREACH(p_arg)
      IF(NOT found)
        # The required argument wasn't found, so we need to add it in.
        SET(new_parameter "${new_parameter} ${option}")
      ENDIF(NOT found)
    ENDFOREACH(option ${TMP})
  ENDFOREACH(required_arg ${ARGN})
  SET(${parameter} ${new_parameter} CACHE STRING "" FORCE)
ENDMACRO(FORCE_ADD_FLAGS)

# This MACRO is designed to set variables to default values only on
# the first configure.  Subsequent configures will produce no ops.
MACRO(FIRST_TIME_SET VARIABLE VALUE TYPE COMMENT)
  IF(NOT PASSED_FIRST_CONFIGURE)
    SET(${VARIABLE} ${VALUE} CACHE ${TYPE} ${COMMENT} FORCE)
  ENDIF(NOT PASSED_FIRST_CONFIGURE)
ENDMACRO(FIRST_TIME_SET)

MACRO(FIRST_TIME_MESSAGE)
  IF(NOT PASSED_FIRST_CONFIGURE)
    MESSAGE(${ARGV})
  ENDIF(NOT PASSED_FIRST_CONFIGURE)  
ENDMACRO(FIRST_TIME_MESSAGE)

MACRO(SUBDIRS_IF VARIABLE DESCRIPTION DIRS)
  # Add the cached variable.
  SET(${VARIABLE} 0 CACHE BOOL ${DESCRIPTION})
  IF(${VARIABLE})
    SUBDIRS(${DIRS})
  ENDIF(${VARIABLE})
ENDMACRO(SUBDIRS_IF)

# Computes the realtionship between two version strings.  A version
# string is a number delineated by '.'s such as 1.3.2 and 0.99.9.1.
# You can feed version strings with different number of dot versions,
# and the shorter version number will be padded with zeros: 9.2 <
# 9.2.1 will actually compare 9.2.0 < 9.2.1.
#
# Input: a_in - value, not variable
#        b_in - value, not variable
#        result_out - variable with value:
#                         -1 : a_in <  b_in
#                          0 : a_in == b_in
#                          1 : a_in >  b_in
#
# Written by James Bigler.
MACRO(COMPARE_VERSION_STRINGS a_in b_in result_out)
  # Since SEPARATE_ARGUMENTS using ' ' as the separation token,
  # replace '.' with ' ' to allow easy tokenization of the string.
  STRING(REPLACE "." " " a ${a_in})
  STRING(REPLACE "." " " b ${b_in})
  SEPARATE_ARGUMENTS(a)
  SEPARATE_ARGUMENTS(b)

  # Check the size of each list to see if they are equal.
  LIST(LENGTH a a_length)
  LIST(LENGTH b b_length)

  # Pad the shorter list with zeros.

  # Note that range needs to be one less than the length as the for
  # loop is inclusive (silly CMake).
  IF(a_length LESS b_length)
    # a is shorter
    SET(shorter a)
    MATH(EXPR range "${b_length} - 1")
    MATH(EXPR pad_range "${b_length} - ${a_length} - 1")
  ELSE(a_length LESS b_length)
    # b is shorter
    SET(shorter b)
    MATH(EXPR range "${a_length} - 1")
    MATH(EXPR pad_range "${a_length} - ${b_length} - 1")
  ENDIF(a_length LESS b_length)

  # PAD out if we need to
  IF(NOT pad_range LESS 0)
    FOREACH(pad RANGE ${pad_range})
      # Since shorter is an alias for b, we need to get to it by by dereferencing shorter.
      LIST(APPEND ${shorter} 0)
    ENDFOREACH(pad RANGE ${pad_range})
  ENDIF(NOT pad_range LESS 0)

  SET(result 0)
  FOREACH(index RANGE ${range})
    IF(result EQUAL 0)
      # Only continue to compare things as long as they are equal
      LIST(GET a ${index} a_version)
      LIST(GET b ${index} b_version)
      # LESS
      IF(a_version LESS b_version)
        SET(result -1)
      ENDIF(a_version LESS b_version)
      # GREATER
      IF(a_version GREATER b_version)
        SET(result 1)
      ENDIF(a_version GREATER b_version)
    ENDIF(result EQUAL 0)
  ENDFOREACH(index)

  # Copy out the return result
  SET(${result_out} ${result})
ENDMACRO(COMPARE_VERSION_STRINGS)

# Only adds a test if TEST_NUM_PROCS is >= NP.  !(x < NP) == (x >= NP)
# equavelance is used because CMake doesn't have >= test.
MACRO(ADD_NP_TEST NP)
  IF(NOT TEST_NUM_PROCS LESS ${NP})
    ADD_TEST(${ARGN})
  ENDIF(NOT TEST_NUM_PROCS LESS ${NP})
ENDMACRO(ADD_NP_TEST)

# Used to add Perforce stuff to the Visual Studio projects.  Note that we don't
# have to make a switch on the generator, because these target properties are
# only respected when generating VS projects.
macro(add_perforce_to_target _target)
  # Default this to off, until I can figure out why VS is rewritting all the project files.
  option(USE_PERFORCE "Turn on Perforce plugin in project files" OFF)
  if (USE_PERFORCE)
    set_target_properties(${_target} PROPERTIES
      VS_SCC_PROJECTNAME "Perforce Project"
      VS_SCC_LOCALPATH "${CMAKE_SOURCE_DIR}"
      VS_SCC_PROVIDER "MSSCCI:Perforce SCM"
      )
  endif()
endmacro()


# Used by ll_to_cpp and bc_to_cpp
find_file(bin2cpp_cmake bin2cpp.cmake ${CMAKE_MODULE_PATH} )
set(bin2cpp_cmake "${bin2cpp_cmake}" CACHE INTERNAL "Path to internal bin2cpp.cmake" FORCE)

# Converts input_ll file to llvm bytecode encoded as a string in the outputSource file
# defined with the export symbol provided.
function(ll_to_cpp input outputSource outputInclude exportSymbol)
  # message("input = ${input}")
  # message("outputSource = ${outputSource}")
  # message("outputInclude = ${outputInclude}")
  # message("exportSymbol = ${exportSymbol}")
  get_filename_component(outputABS  "${outputSource}" ABSOLUTE )
  get_filename_component(outputDir  "${outputSource}" PATH )
  get_filename_component(outputName "${outputSource}" NAME )
  file(RELATIVE_PATH outputRelPath  "${CMAKE_BINARY_DIR}" "${outputDir}")
  
  set(bc_filename "${outputName}.tmp.bc")

  # Generate header file (configure time)
  include(bin2cpp)
  bin2h(${outputInclude} ${exportSymbol} "${bc_filename}")
  
  # Convert ll to byte code
  add_custom_command(
    OUTPUT ${outputSource}

    # convert ll to bc
    COMMAND ${LLVM_llvm-as} "${input}" -o "${bc_filename}"
    # convert bc file to cpp
    COMMAND ${CMAKE_COMMAND} -DCUDA_BIN2C_EXECUTABLE:STRING="${CUDA_BIN2C_EXECUTABLE}"
                             -DCPP_FILE:STRING="${outputSource}"
                             -DCPP_SYMBOL:STRING="${exportSymbol}"
                             -DSOURCE_BASE:STRING="${outputDir}"
                             -DSOURCES:STRING="${bc_filename}"
                             -P "${bin2cpp_cmake}"
    # Remove temp bc file                         
    COMMAND ${CMAKE_COMMAND} -E remove -f "${bc_filename}"

    WORKING_DIRECTORY ${outputDir}
    MAIN_DEPENDENCY ${input}                             
    DEPENDS ${bin2cpp_cmake}
    COMMENT "Generating ${outputRelPath}/${outputName}"
    )
endfunction()

function(bc_to_cpp input outputSource outputInclude exportSymbol)
  # message("input = ${input}")
  # message("outputSource = ${outputSource}")
  # message("outputInclude = ${outputInclude}")
  # message("exportSymbol = ${exportSymbol}")
  get_filename_component(outputABS  "${outputSource}" ABSOLUTE )
  get_filename_component(outputDir  "${outputABS}" PATH )
  get_filename_component(outputName "${outputABS}" NAME )
  file(RELATIVE_PATH outputRelPath  "${EXTERNAL_BINARY_DIR}" "${outputDir}")
  get_filename_component(inputABS "${input}" ABSOLUTE)
  get_filename_component(inputDir "${inputABS}" PATH)
  get_filename_component(inputName "${inputABS}" NAME)
  
  set(bc_filename "${inputName}")
  
  # Generate header file (configure time)
  include(bin2cpp)
  bin2h(${outputInclude} ${exportSymbol} "${bc_filename}")
  
  # Convert ll to byte code
  add_custom_command(
    OUTPUT ${outputSource}

    # convert bc file to cpp
    COMMAND ${CMAKE_COMMAND} -DCUDA_BIN2C_EXECUTABLE:STRING="${CUDA_BIN2C_EXECUTABLE}"
                             -DCPP_FILE:STRING="${outputSource}"
                             -DCPP_SYMBOL:STRING="${exportSymbol}"
                             -DSOURCE_BASE:STRING="${inputDir}"
                             -DSOURCES:STRING="${bc_filename}"
                             -P "${bin2cpp_cmake}"

    WORKING_DIRECTORY ${outputDir}
    MAIN_DEPENDENCY ${input}
    DEPENDS ${bin2cpp_cmake}
    COMMENT "Generating ${outputRelPath}/${outputName}"
    )
endfunction()

################################################################################
# Copy ptx scripts into a string in a cpp header file.
#
# Usage: ptx_to_cpp( ptx_cpp_headers my_directory FILE1 FILE2 ... FILEN )
#   ptx_cpp_files  : [out] List of cpp files created (Note: new files are appended to this list)
#   directory      : [in]  Directory in which to place the resulting headers
#   FILE1 .. FILEN : [in]  ptx files to be cpp stringified
#
# FILE1 -> filename: ${FILE1}_ptx.cpp
#       -> string  : const char* nvrt::${FILE1}_ptx = "...";

macro( ptx_to_cpp ptx_cpp_files directory )
  foreach( file ${ARGN} )
    if( ${file} MATCHES ".*\\.ptx$" )

      #message( "file_name     : ${file}" )

      # Create the output cpp file name
      get_filename_component( base_name ${file} NAME_WE )
      set( cpp_filename ${directory}/${base_name}_ptx.cpp )
      set( variable_name ${base_name}_ptx )
      set( ptx2cpp ${CMAKE_SOURCE_DIR}/CMake/ptx2cpp.cmake )

      #message( "base_name     : ${base_name}" )
      #message( "cpp_file_name : ${cpp_filename}" )
      #message( "variable_name : ${variable_name}" )

      add_custom_command( OUTPUT ${cpp_filename}
        COMMAND ${CMAKE_COMMAND}
          -DCUDA_BIN2C_EXECUTABLE:STRING="${CUDA_BIN2C_EXECUTABLE}"
          -DCPP_FILE:STRING="${cpp_filename}"
          -DPTX_FILE:STRING="${file}"
          -DVARIABLE_NAME:STRING=${variable_name}
          -DNAMESPACE:STRING=optix
          -P ${ptx2cpp}
        DEPENDS ${file}
        DEPENDS ${ptx2cpp}
        COMMENT "${ptx2cpp}: converting ${file} to ${cpp_filename}"
        )

      list(APPEND ${ptx_cpp_files} ${cpp_filename} )
      #message( "ptx_cpp_files   : ${${ptx_cpp_files}}" )

    endif( ${file} MATCHES ".*\\.ptx$" )
  endforeach( file )
endmacro( ptx_to_cpp )


################################################################################
# Strip library of all local symbols 
#
# Usage: strip_symbols( target_name ) 
#   target_name : [out] target name for the library to be stripped 

function( strip_symbols target )
  if( NOT WIN32 )
    add_custom_command( TARGET ${target}
                        POST_BUILD
                        # using -x to strip all local symbols
                        COMMAND ${CMAKE_STRIP} -x $<TARGET_FILE:${target}>
                        COMMENT "Stripping symbols from ${target}"
                      )
  endif()
endfunction( strip_symbols )

################################################################################
# Only export the symbols that we need.
#
# Usage: optix_setup_exports( target_name export_file ) 
#   target_name : [in] target name for the library to be stripped 
#   export_file : [in] name of the file that contains the export symbol names
#
# Do not use this macro with WIN32 DLLs unless you are not using the dllexport
# macros. The DLL name will be set using the SOVERSION property of the target,
# so be sure to set that before calling this macro
#
function( optix_setup_exports target export_file )
  # Suck in the exported symbol lists
  include(${export_file})  
  
  # Collect the symbols we wish to export.  These variables were defined in the
  # export_file.
  
  # Public symbols
  set(exported_symbols ${${target}_public_symbols})
  
  # Developer symbols
  if( NOT RELEASE_PUBLIC )
    list( APPEND exported_symbols ${${target}_developer_symbols} ) 
  endif()
  
  # Win32 symbols
  if( WIN32 )
    list( APPEND exported_symbols ${${target}_win32_symbols} )
  endif()
  
  if( UNIX )
    if ( APPLE )
      # -exported_symbols_list lists the exact set of symbols to export.  You can call it
      # more than once if needed.
      set( export_arg -exported_symbols_list )
    else()
      # -Bsymbolic tells the linker to resolve any local symbols locally first.
      # --version-script allows us to be explicit about which symbols to export.
      set( export_arg -Bsymbolic,--version-script )
    endif()

    # Create the symbol export file.  Since Apple and Linux have different file formats
    # for doing this we will have to specify the information in the file differently.
    set(exported_symbol_file ${CMAKE_CURRENT_BINARY_DIR}/${target}_exported_symbols.txt)
    if(APPLE)
      # The base name of the symbols just has the name.  We need to prefix them with "_".
      set(modified_symbols)
      foreach(symbol ${exported_symbols})
        list(APPEND modified_symbols "_${symbol}")
      endforeach()
      # Just list the symbols.  One per line.  Since we are treating the list as a string
      # here we can replace the ';' character with a newline.
      string(REPLACE ";" "\n" exported_symbol_file_content "${modified_symbols}")
      file(WRITE ${exported_symbol_file} "${exported_symbol_file_content}\n")
    else()
      # Format is:
      #
      # {
      # global:
      # extern "C" {
      # symbol;
      # };
      # local: *;
      # };
      string(REPLACE ";" ";\n" exported_symbol_file_content "${exported_symbols}")
      file(WRITE ${exported_symbol_file} "{\nglobal:\nextern \"C\" {\n${exported_symbol_file_content};\n};\nlocal: *;\n};\n")
    endif()

    # Add the command to the LINK_FLAGS
    set_property( TARGET ${target}
      APPEND_STRING 
      PROPERTY LINK_FLAGS
      " -Wl,${export_arg},${exported_symbol_file}"
      )      
  elseif( WIN32 )
    set(exported_symbol_file ${CMAKE_CURRENT_BINARY_DIR}/${target}.def)
    set(name ${target} )
    get_property( abi_version TARGET ${target} PROPERTY SOVERSION )
    if( abi_version )
      set(name "${name}.${abi_version}")
    endif()
    # Format is:
    # 
    # NAME <dllname>
    # EXPORTS
    #  <names>
    #
    string(REPLACE ";" "\n" def_file_content "${exported_symbols}" )
    file(WRITE ${exported_symbol_file} "NAME ${name}.dll\nEXPORTS\n${def_file_content}")   
    
    # Add the command to the LINK_FLAGS
    set_property( TARGET ${target}
      APPEND_STRING 
      PROPERTY LINK_FLAGS
      " /DEF:${exported_symbol_file}"
      ) 
  endif()
  
  # Make sure that if the exported_symbol_file changes we relink the library.
  set_property( TARGET ${target}
    APPEND
    PROPERTY LINK_DEPENDS
    "${exported_symbol_file}"
    )
endfunction()

################################################################################
# Some helper functions for pushing and poping variable values
#
function(push_variable variable)
  #message("push before: ${variable} = ${${variable}}, ${variable}_STACK = ${${variable}_STACK}")
  #message("             ARGN = ${ARGN}")
  #message("             ARGC = ${ARGC}, ARGV = ${ARGV}")
  if(ARGC LESS 2)
    message(FATAL_ERROR "push_variable requires at least one value to push.")
  endif()
  # Because the old value may be a list, we need to indicate how many items
  # belong to this push.  We do this by marking the start of the new push.
  list(LENGTH ${variable}_STACK start_index)
  # If the value of variable is empty, then we need to leave a placeholder,
  # because CMake doesn't have an "empty" token.
  if (DEFINED ${variable} AND NOT ${variable} STREQUAL "")
    list(APPEND ${variable}_STACK ${${variable}} ${start_index})
  else()
    list(APPEND ${variable}_STACK ${variable}_EMPTY ${start_index})
  endif()
  # Make the stack visible outside of the function's scope.
  set(${variable}_STACK ${${variable}_STACK} PARENT_SCOPE)
  # Set the new value of the variable.
  set(${variable} ${ARGN} PARENT_SCOPE)
  #set(${variable} ${ARGN}) # use for the output message below
  #message("push after : ${variable} = ${${variable}}, ${variable}_STACK = ${${variable}_STACK}")
endfunction()

function(pop_variable variable)
  #message("pop  before: ${variable} = ${${variable}}, ${variable}_STACK = ${${variable}_STACK}")
  # Find the length of the stack to use as an index to the end of the list.
  list(LENGTH ${variable}_STACK stack_length)
  if(stack_length LESS 2)
    message(FATAL_ERROR "${variable}_STACK is empty.  Can't pop any more values.")
  endif()
  math(EXPR stack_end "${stack_length} - 1")
  # Get the start of where the old value begins in the stack.
  list(GET ${variable}_STACK ${stack_end} variable_start)
  math(EXPR variable_end "${stack_end} - 1")
  foreach(index RANGE ${variable_start} ${variable_end})
    list(APPEND list_indices ${index})
  endforeach()
  list(GET ${variable}_STACK ${list_indices} stack_popped)
  # If the first element is our special EMPTY token, then we should empty it out
  if(stack_popped STREQUAL "${variable}_EMPTY")
    set(stack_popped "")
  endif()
  # Remove all the items
  list(APPEND list_indices ${stack_end})
  list(REMOVE_AT ${variable}_STACK ${list_indices})
  # Make sthe stack visible outside of the function's scope.
  set(${variable}_STACK ${${variable}_STACK} PARENT_SCOPE)
  # Assign the old value to the variable
  set(${variable} ${stack_popped} PARENT_SCOPE)
  #set(${variable} ${stack_popped}) # use for the output message below
  #message("pop  after : ${variable} = ${${variable}}, ${variable}_STACK = ${${variable}_STACK}")
endfunction()


# Helper function to generate ptx for a particular sm versions.
#
# sm_versions[input]: a list of version, such as sm_13;sm_20.  These will be used to
#                     generate the names of the output files.
# generate_files[output]: list of generated source files
# ARGN[input]: list of input CUDA C files and other options to pass to nvcc.
#
function(compile_ptx sm_versions_in generated_files)
  # CUDA_GET_SOURCES_AND_OPTIONS is a FindCUDA internal command that we are going to
  # borrow.  There's no garantees on backward compatibility using this macro.
  CUDA_GET_SOURCES_AND_OPTIONS(_sources _cmake_options _options ${ARGN})
  # Check to see if they specified an sm version, and spit out an error.
  list(FIND _options -arch arch_index)
  if(arch_index GREATER -1)
    math(EXPR sm_index "${arch_index}+1")
    list(GET _options ${sm_index} sm_value)
    message(FATAL_ERROR "-arch ${sm_value} has been specified to compile_ptx.  Please remove that option and put it in the sm_versions argument.")
  endif()
  set(${generated_files})
  set(sm_versions ${sm_versions_in})
  if(NOT CUDA_SM_20)
    list(REMOVE_ITEM sm_versions sm_20)
  endif()
  push_variable(CUDA_64_BIT_DEVICE_CODE ON)
  foreach(source ${_sources})
    set(ptx_generated_files)
    #message("\n\nProcessing ${source}")
    foreach(sm ${sm_versions})

      # Generate the 32 bit ptx for 32 bit builds and when the CMAKE_OSX_ARCHITECTURES
      # specifies it.
      list(FIND CMAKE_OSX_ARCHITECTURES i386 osx_build_32_bit_ptx)
      if( CMAKE_SIZEOF_VOID_P EQUAL 4 OR NOT osx_build_32_bit_ptx LESS 0)
        set(CUDA_64_BIT_DEVICE_CODE OFF)
        CUDA_WRAP_SRCS( ptx_${sm}_32 PTX _generated_files ${source} ${_cmake_options}
          OPTIONS -arch ${sm} ${_options}
          )
        # Add these files onto the list of files.
        list(APPEND ptx_generated_files ${_generated_files})
      endif()

      # Generate the 64 bit ptx for 64 bit builds and when the CMAKE_OSX_ARCHITECTURES
      # specifies it.
      list(FIND CMAKE_OSX_ARCHITECTURES x86_64 osx_build_64_bit_ptx)
      if( CMAKE_SIZEOF_VOID_P EQUAL 8 OR NOT osx_build_64_bit_ptx LESS 0)
        set(CUDA_64_BIT_DEVICE_CODE ON)
        CUDA_WRAP_SRCS( ptx_${sm}_64 PTX _generated_files ${source} ${_cmake_options}
          OPTIONS -arch ${sm} ${_options}
          )
        # Add these files onto the list of files.
        list(APPEND ptx_generated_files ${_generated_files})
      endif()
    endforeach()

    get_filename_component(source_basename "${source}" NAME_WE)
    set(cpp_wrap "${CMAKE_CURRENT_BINARY_DIR}/${source_basename}_ptx.cpp")
    set(h_wrap   "${CMAKE_CURRENT_BINARY_DIR}/${source_basename}_ptx.h")

    set(relative_ptx_generated_files)
    foreach(file ${ptx_generated_files})
      get_filename_component(fname "${file}" NAME)
      list(APPEND relative_ptx_generated_files "${fname}")
    endforeach()

    # Now generate a target that will generate the wrapped version of the ptx
    # files at build time
    set(symbol "${source_basename}_source")
    add_custom_command( OUTPUT ${cpp_wrap}
      COMMAND ${CMAKE_COMMAND} -DCUDA_BIN2C_EXECUTABLE:STRING="${CUDA_BIN2C_EXECUTABLE}"
      -DCPP_FILE:STRING="${cpp_wrap}"
      -DCPP_SYMBOL:STRING="${symbol}"
      -DSOURCE_BASE:STRING="${CMAKE_CURRENT_BINARY_DIR}"
      -DSOURCES:STRING="${relative_ptx_generated_files}"
      ARGS -P "${CMAKE_SOURCE_DIR}/CMake/bin2cpp.cmake"
      DEPENDS ${CMAKE_SOURCE_DIR}/CMake/bin2cpp.cmake ${ptx_generated_files}
      )
    # We know the list of files at configure time, so generate the files here
    include(bin2cpp)
    bin2h("${h_wrap}" ${symbol} ${relative_ptx_generated_files})

    list(APPEND ${generated_files} ${ptx_generated_files} ${cpp_wrap} ${h_wrap})
  endforeach(source)
  pop_variable(CUDA_64_BIT_DEVICE_CODE)

  set(${generated_files} ${${generated_files}} PARENT_SCOPE)
endfunction()

# Helper function to generate the appropiate options for a CUDA compile
# based on the target architectures.
#
# Function selects the higher compute capability available and generates code for that one.
#
# Usage: cuda_generate_runtime_target_options( output_var target_list )
# 
# output[output] is a list-variable to fill with options for CUDA_COMPILE
# ARGN[input] is a list of targets, i.e. sm_11 sm_20 sm_30

function( cuda_generate_runtime_target_options output )
  # remove anything that is not sm_XX
  foreach(target ${ARGN})
    string( REGEX MATCH "^(sm_[0-9][0-9])$" match ${target} )
    if( NOT CMAKE_MATCH_1 )
      list( REMOVE_ITEM ARGN ${target} )
    endif( NOT CMAKE_MATCH_1 )
  endforeach(target)

  list( LENGTH ARGN valid_target_count )
  if( valid_target_count GREATER 0 )

# We will add compute_XX automatically, infer max compatible compute capability.
    # check targets for max compute capability
    set( smver_max "0" )
    foreach(target ${ARGN})
      string( REGEX MATCH "sm_([0-9][0-9])$" sm_ver_match ${target} )
      if( CMAKE_MATCH_1 )
        if( ${CMAKE_MATCH_1} STRGREATER smver_max )
          set( smver_max ${CMAKE_MATCH_1} )
        endif( ${CMAKE_MATCH_1} STRGREATER smver_max )
      endif( CMAKE_MATCH_1 )
      unset( sm_ver_match )
    endforeach(target)
    
    # copy the input list to a new one and sort it
    set( sm_versions ${ARGN} )
    list( SORT sm_versions )
    
    # walk to SM versions to generate the entries of gencode
    set( options "" )
    foreach( sm_ver ${sm_versions} )
      string( REGEX MATCH "sm_([0-9][0-9])$" sm_ver_num ${sm_ver} )  

# This adds compute_XX automatically, in order to generate PTX.
      if( ${CMAKE_MATCH_1} STREQUAL ${smver_max} )
        # append the max compute capability, to get compute_XX too.
        # this appends the PTX code for the higher SM_ version
        set(entry -gencode=arch=compute_${CMAKE_MATCH_1},code=\\\"${sm_ver},compute_${smver_max}\\\")
      else( ${CMAKE_MATCH_1} STREQUAL ${smver_max} )
        set(entry -gencode=arch=compute_${CMAKE_MATCH_1},code=\\\"${sm_ver}\\\")     
      endif( ${CMAKE_MATCH_1} STREQUAL ${smver_max} )
      list( APPEND options ${entry} )
    endforeach( sm_ver ${sm_versions} )
    
    # return the generated option string
    set( ${output} ${options} PARENT_SCOPE )
    
    unset( smver_max )
    unset( sm_versions )
  else( valid_target_count GREATER 0 )
    # return empty string
    set( ${output} "" PARENT_SCOPE )    
  endif( valid_target_count GREATER 0 )

endfunction(cuda_generate_runtime_target_options)



# Create multiple bitness targets for mac universal builds
#
# Usage: OPTIX_MAKE_UNIVERSAL_CUDA_RUNTIME_OBJECTS( 
#          target_name
#          generated_files_var
#          FILE0.cu FILE1.cu ... FILEN.cu
#          OPTIONS
#          option1 option2
#          )
# 
# target_name     [input ] name prefix for the resulting files 
# generated_files [output] list of filenames of resulting object files
# ARGN            [input ] is a list of source files plus possibly options 
function( OPTIX_MAKE_UNIVERSAL_CUDA_RUNTIME_OBJECTS target_name generated_files ) 

  # If you specified CMAKE_OSX_ARCHITECTURES that means you want a universal build (though
  # this should work for a single architecture).
  if (CMAKE_OSX_ARCHITECTURES)
    set(cuda_files)

    push_variable(CUDA_64_BIT_DEVICE_CODE OFF)
    list(LENGTH CMAKE_OSX_ARCHITECTURES num_arches)
    if (num_arches GREATER 1)
      # If you have more than one architecture then you don't want to attach the build rule
      # to the file itself otherwise you could compile some files in multiple targets.
      set(CUDA_ATTACH_VS_BUILD_RULE_TO_CUDA_FILE OFF)
    endif()
    foreach(arch ${CMAKE_OSX_ARCHITECTURES})
      # Set the bitness of the build specified by nvcc to the one matching the OSX
      # architecture.
      if (arch STREQUAL "i386")
        set(CUDA_64_BIT_DEVICE_CODE OFF)
      elseif (arch STREQUAL "x86_64")
        set(CUDA_64_BIT_DEVICE_CODE ON)
      else()
        message(SEND_ERROR "Unknown OSX arch ${arch}")
      endif()

      CUDA_WRAP_SRCS(
        ${target_name}_${arch}
        OBJ
        cuda_sources
        ${ARGN}
        )
      list( APPEND cuda_files ${cuda_sources} )
    endforeach()
    pop_variable(CUDA_64_BIT_DEVICE_CODE)

  else()
    CUDA_WRAP_SRCS(
      ${target_name}
      OBJ
      cuda_files
      ${ARGN}
      )
  endif()

  set( ${generated_files} ${cuda_files} PARENT_SCOPE )
endfunction()
