CXX=/usr/bin/g++-7
CUDA_PATH=/opt/cuda

OPT = -ffast-math -O3

INCLUDES=-I$(CUDA_PATH)/include
LIBS= -L$(CUDA_PATH)/lib64 -lglfw -lGLEW -lGL -lcuda -lcudart
NVCC=$(CUDA_PATH)/bin/nvcc --compiler-bindir=$(CXX) $(INCLUDES) --use_fast_math

trace_volume: volume_kernel.o main.cpp hdr_loader.h
	$(CXX) -Wall $(OPT) $(INCLUDES) -o trace_volume main.cpp volume_kernel.o $(LIBS) 

volume_kernel.o: volume_kernel.cu volume_kernel.h Makefile
	$(NVCC) -c volume_kernel.cu

clean:
	rm -f volume_kernel.o trace_volume

