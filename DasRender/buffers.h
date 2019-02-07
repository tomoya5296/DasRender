#pragma once
#include <optixu/optixpp_namespace.h>
#include <optixu/optixu_math_stream_namespace.h>
#include <sutil.h>
#include "binaryio.h"


using namespace optix;

class Buffers
{
public:
	Buffers() {
	};

	~Buffers() {};

	void displayBuffers(const std::string &out_file) {
		sutil::displayBufferPPM((out_file + "_output.ppm").c_str(), output, false);
		sutil::displayBufferPPM((out_file + "_depth.ppm").c_str(), depth, false);
		sutil::displayBufferPPM((out_file + "_texture.ppm").c_str(), texture, false);
		sutil::displayBufferPPM((out_file + "_normal.ppm").c_str(), normal, false);
		sutil::displayBufferPPM((out_file + "_shadow.ppm").c_str(), shadow, false);
	}

	void saveBins(const std::string &out_file) {
		binaryio::save3dBinary((out_file + "_output.bin").c_str(), output);
		binaryio::save1dBinary((out_file + "_depth.bin").c_str(), depth);
		binaryio::save3dBinary((out_file + "_texture.bin").c_str(), texture);
		binaryio::save3dBinary((out_file + "_normal.bin").c_str(), normal);
		binaryio::save1dBinary((out_file + "_shadow.bin").c_str(), shadow);
	}

	void saveCompressedBin(const std::string &out_file) {
		binaryio::save3dBinary((out_file + "_compressed.bin").c_str(), output,  false);
		binaryio::save1dBinary((out_file + "_compressed.bin").c_str(), depth,   true);
		binaryio::save3dBinary((out_file + "_compressed.bin").c_str(), texture, true);
		binaryio::save3dBinary((out_file + "_compressed.bin").c_str(), normal,  true);
		binaryio::save1dBinary((out_file + "_compressed.bin").c_str(), shadow,  true);
	}

	Buffer output;
	Buffer depth;
	Buffer texture;
	Buffer normal;
	Buffer shadow;
};
