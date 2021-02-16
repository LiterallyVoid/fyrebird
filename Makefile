CFLAGS=-O0 -g -Wall -Wextra
LDFLAGS=-lvulkan

all: compute.spv ray

compute.spv: src/compute.glsl
	glslc -fshader-stage=compute src/compute.glsl -o compute.spv

fyrebird: src/main.c
	$(CC) $^ -o $@ $(CFLAGS) $(LDFLAGS)
