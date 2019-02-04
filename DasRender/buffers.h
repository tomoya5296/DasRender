#pragma once
#include <optixu/optixpp_namespace.h>
#include <optixu/optixu_math_stream_namespace.h>
#include <sutil.h>


using namespace optix;

class Buffers
{
public:
	Buffers() {
	};

	~Buffers() {};

	Buffer output;
	Buffer depth;
	Buffer texture;
	Buffer normal;
	Buffer shadow;
};
