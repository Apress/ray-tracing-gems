IF (WIN32 AND MSVC_VERSION LESS 1700)
  # Starting with Windows 8, the DirectX SDK is included as part of the Windows SDK
  # (http://msdn.microsoft.com/en-us/library/windows/desktop/ee663275.aspx)
  # and, in turn, Visual Studio 2012 (even Express) includes the appropriate components of the Windows SDK
  # (http://msdn.microsoft.com/en-us/windows/desktop/hh852363.aspx)
  # so we don't need a DX include path if we're targeting VS2012+

  FIND_PATH(DX9_INCLUDE_PATH d3d9.h 
  PATHS
    "$ENV{DXSDK_DIR}/Include"
    "$ENV{PROGRAMFILES}/Microsoft DirectX SDK/Include" 
    DOC "The directory where d3d9.h resides")
  FIND_PATH(DX10_INCLUDE_PATH D3D10.h
  PATHS 
    "$ENV{DXSDK_DIR}/Include" 
    "$ENV{PROGRAMFILES}/Microsoft DirectX SDK/Include" 
    DOC "The directory where D3D10.h resides") 
  FIND_PATH(DX11_INCLUDE_PATH D3D11.h
  PATHS
    "$ENV{DXSDK_DIR}/Include" 
    "$ENV{PROGRAMFILES}/Microsoft DirectX SDK/Include" 
    DOC "The directory where D3D11.h resides")

  IF (DX9_INCLUDE_PATH)
    SET( DX9_FOUND 1 ) 
  ELSE (DX9_INCLUDE_PATH) 
    SET( DX9_FOUND 0 ) 
  ENDIF (DX9_INCLUDE_PATH)

  IF (DX10_INCLUDE_PATH)
    SET( DX10_FOUND 1 )
  ELSE (DX10_INCLUDE_PATH) 
    SET( DX10_FOUND 0 ) 
  ENDIF (DX10_INCLUDE_PATH)  

  IF (DX11_INCLUDE_PATH)
    SET( DX11_FOUND 1 ) 
  ELSE (DX11_INCLUDE_PATH) 
    SET( DX11_FOUND 0 ) 
  ENDIF (DX11_INCLUDE_PATH)
ENDIF (WIN32 AND MSVC_VERSION LESS 1700) 

