# Volume Path Tracer

This is the complete source code for the volume path tracer described in Chapter 28 ("Ray Tracing Inhomogeneous Volumes") of _Ray Tracing Gems_. The CUDA rendering kernel featured in the book is called from a small interactive OpenGL application.

## Compiling

In order to compile the application, you may need to install some dependencies:

- NVIDIA CUDA Toolkit (to compile the CUDA kernel)
- GLEW and GLFW (for the OpenGL host code)

A simple Makefile for Linux is included (assuming GLEW and GLFW development packages are installed in system paths and CUDA resides under /opt/cuda). Adapting the CUDA installation path and changing the C++ compiler in the Makefile should be straightforward.

While the code does build on Windows, no build system is included here.

## Running

By default, only a procedural enviroment map will be used. To specify an environment map in HDR format an optional command-line argument can be passed to the executable, i.e.:

```bash
./trace_volume /path/to/envmap.hdr
```

The appliation offers the following controls:

- "ESC" terminates the programm.
- With the left mouse button clicked mouse movements rotate the camera around the volume.
- Using the mouse wheel (scrolling) allows to zoom in or out.
- "Space" will toggle through volume procedural (cube or spiral) and environment (procedural or map) configurations.
- Tonemapper exposure (brightness) can be decreased using "[" or "Keypad-Minus" and increased using "]" or "Keypad-Plus".




