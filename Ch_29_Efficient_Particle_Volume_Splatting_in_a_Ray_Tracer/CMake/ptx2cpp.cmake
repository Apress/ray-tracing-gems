
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

# This script produces a string variable from the contents of a ptx
# script.  The variable is defined in the .cc file and the .h file.

# This script excepts the following variable to be passed in like
# -DVAR:TYPE=VALUE
# 
# CPP_FILE
# PTX_FILE
# VARIABLE_NAME
# NAMESPACE
# CUDA_BIN2C_EXECUTABLE

# message("PTX_FILE      = ${PTX_FILE}")
# message("CPP_FILE      = ${C_FILE}")
# message("VARIABLE_NAME = ${VARIABLE_NAME}")
# message("NAMESPACE     = ${NAMESPACE}")

execute_process( COMMAND ${CUDA_BIN2C_EXECUTABLE} -p 0 -st -c -n ${VARIABLE_NAME}_static "${PTX_FILE}"
  OUTPUT_VARIABLE bindata
  RESULT_VARIABLE result
  ERROR_VARIABLE error
  )
if(result)
  message(FATAL_ERROR "bin2c error:\n" ${error})
endif()

set(BODY
  "${bindata}\n"
  "namespace ${NAMESPACE} {\n\nstatic const char* const ${VARIABLE_NAME} = reinterpret_cast<const char*>(&${VARIABLE_NAME}_static[0]);\n} // end namespace ${NAMESPACE}\n")
file(WRITE ${CPP_FILE} "${BODY}")
