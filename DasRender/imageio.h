#pragma once
// Note: wglew.h has to be included before sutil.h on Windows
#if defined(__APPLE__)
#  include <GLUT/glut.h>
#else
#  include <GL/glew.h>
#  if defined(_WIN32)
#    include <GL/wglew.h>
#  endif
#  include <GL/glut.h>
#endif

#include <optixu/optixpp_namespace.h>
#include <optixu/optixu_math_stream_namespace.h>
#include <sutil.h>
#include <string>
#include <vector>

using namespace optix;
//ImageIO Declaratioms

//TODO Implement Instream

void writeBinary(const char* filename, Buffer buffer) {

	writeBinary(filename, buffer->get());
}

void writeBinary(const char* filename, RTbuffer buffer) {
	GLsizei width, height;
	RTsize buffer_width, buffer_height;

	GLvoid* imageData;
	RT_CHECK_ERROR( rtBufferMap(buffer, &imageData) );

	RT_CHECK_ERROR( rtBufferGetSize2D(buffer, &buffer_width, &buffer_height) );
	width = static_cast<GLsizei>(buffer_width);
	height = static_cast<GLsizei>(buffer_height);

	std::vector<float> pix(width * height * 3);

}

