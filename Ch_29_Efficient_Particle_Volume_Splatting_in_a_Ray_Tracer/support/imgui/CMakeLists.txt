
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

add_library( imgui SHARED
    imconfig.h
    imgui.cpp
    imgui.h
    imgui_demo.cpp
    imgui_draw.cpp
    imgui_impl_glfw_gl2.cpp
    imgui_impl_glfw_gl2.h
    imgui_internal.h
    stb_rect_pack.h
    stb_textedit.h
    stb_truetype.h
    )

target_link_libraries( imgui glfw ${OPENGL_gl_LIBRARY})
