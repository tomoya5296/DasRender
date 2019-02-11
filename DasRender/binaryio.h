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
#include <iostream>
#include <fstream>

using namespace optix;

void saveBin(const float *Pix, const char *fname,
	const int wid, const int hgt,const int chan)
{
	if (Pix == NULL || wid < 1 || hgt < 1)
		throw Exception("Image is ill-formed. Not saving");

	if (chan != 1 && chan != 3 && chan != 4)
		throw Exception("Attempting to save image with channel count != 1, 3, or 4.");
	
	std::ofstream OutFile(fname, std::ios::out | std::ios::binary);
	if (!OutFile.is_open())
		throw Exception("Could not open file for SaveBIN");

	OutFile.write((char*)&wid, sizeof(int));
	OutFile.write((char*)&hgt, sizeof(int));
	OutFile.write((char*)const_cast<float*>(Pix), sizeof(float) * wid * hgt * chan);

	OutFile.close();
}

namespace binaryio
{
	//ImageIO Declaratioms

	//TODO Implement Instream

	void save3dBinary(const char* filename, Buffer buffer_) {
		GLsizei width, height;
		RTsize buffer_width, buffer_height;
		RTbuffer buffer = buffer_->get();

		GLvoid* imageData;
		RT_CHECK_ERROR(rtBufferMap(buffer, &imageData));

		RT_CHECK_ERROR(rtBufferGetSize2D(buffer, &buffer_width, &buffer_height));
		width = static_cast<GLsizei>(buffer_width);
		height = static_cast<GLsizei>(buffer_height);

		std::vector<float> pix(width * height * 3);

		RTformat buffer_format;
		RT_CHECK_ERROR(rtBufferGetFormat(buffer, &buffer_format));

		switch (buffer_format) {
		case RT_FORMAT_UNSIGNED_BYTE4:
			// Data is BGRA and upside down, so we need to swizzle to RGB
			for (int j = height - 1; j >= 0; --j) {
				float *dst = &pix[0] + (3 * width*(height - 1 - j));
				float *src = ((float*)imageData) + (4 * width*j);
				for (int i = 0; i < width; i++) {
					*dst++ = *(src + 2);
					*dst++ = *(src + 1);
					*dst++ = *(src + 0);
					src += 4;
				}
			}
			break;

		case RT_FORMAT_FLOAT:
			// This buffer is upside down
			for (int j = height - 1; j >= 0; --j) {
				float *dst = &pix[0] + width*(height - 1 - j);
				float* src = ((float*)imageData) + (3 * width*j);
				for (int i = 0; i < width; i++) {
					// write the pixel to all 3 channels
					float P = *src++;
					*dst++ = P;
					*dst++ = P;
					*dst++ = P;
				}
			}
			break;

		case RT_FORMAT_FLOAT3:
			// This buffer is upside down
			for (int j = height - 1; j >= 0; --j) {
				float *dst = &pix[0] + (3 * width*(height - 1 - j));
				float* src = ((float*)imageData) + (3 * width*j);
				for (int i = 0; i < width; i++) {
					for (int elem = 0; elem < 3; ++elem) {
						float P = *src++;
						*dst++ = P;
					}
				}
			}
			break;

		case RT_FORMAT_FLOAT4:
			// This buffer is upside down
			for (int j = height - 1; j >= 0; --j) {
				float *dst = &pix[0] + (3 * width*(height - 1 - j));
				float* src = ((float*)imageData) + (4 * width*j);
				for (int i = 0; i < width; i++) {
					for (int elem = 0; elem < 3; ++elem) {
						float P = *src++;
						*dst++ = P;
					}

					// skip alpha
					src++;
				}
			}
			break;

		default:
			fprintf(stderr, "Unrecognized buffer data type or format.\n");
			exit(2);
			break;
		}

		saveBin(&pix[0], filename, width, height, 3);

		// Now unmap the buffer
		RT_CHECK_ERROR(rtBufferUnmap(buffer));
	}

	void save1dBinary(const char* filename, Buffer buffer_) {
		GLsizei width, height;
		RTsize buffer_width, buffer_height;
		RTbuffer buffer = buffer_->get();

		GLvoid* imageData;
		RT_CHECK_ERROR(rtBufferMap(buffer, &imageData));

		RT_CHECK_ERROR(rtBufferGetSize2D(buffer, &buffer_width, &buffer_height));
		width = static_cast<GLsizei>(buffer_width);
		height = static_cast<GLsizei>(buffer_height);

		std::vector<float> pix(width * height * 1);

		RTformat buffer_format;
		RT_CHECK_ERROR(rtBufferGetFormat(buffer, &buffer_format));

		switch (buffer_format) {
		case RT_FORMAT_UNSIGNED_BYTE4:
			// Data is BGRA and upside down, so we need to swizzle to RGB
			fprintf(stderr, "save1DBinary can't deal RT_FORMAT_UNSIGNED_BYTE4.\n");
			exit(2);
			/*for (int j = height - 1; j >= 0; --j) {
				float *dst = &pix[0] + (3 * width*(height - 1 - j));
				float *src = ((float*)imageData) + (4 * width*j);
				for (int i = 0; i < width; i++) {
					*dst++ = *(src + 2);
					*dst++ = *(src + 1);
					*dst++ = *(src + 0);
					src += 4;
				}
			}*/
			break;

		case RT_FORMAT_FLOAT:
			// This buffer is upside down
			fprintf(stderr, "save1DBinary can't deal RT_FORMAT_FLOAT.\n");
			exit(2);
			//for (int j = height - 1; j >= 0; --j) {
			//	float *dst = &pix[0] + width*(height - 1 - j);
			//	float* src = ((float*)imageData) + (3 * width*j);
			//	for (int i = 0; i < width; i++) {
			//		// write the pixel to all 3 channels
			//		float P = *src++;
			//		*dst++ = P;
			//		*dst++ = P;
			//		*dst++ = P;
			//	}
			//}
			break;

		case RT_FORMAT_FLOAT3:
			// This buffer is upside down
			fprintf(stderr, "save1DBinary can't deal RT_FORMAT_FLOAT3.\n");
			exit(2);
			/*for (int j = height - 1; j >= 0; --j) {
				float *dst = &pix[0] + (3 * width*(height - 1 - j));
				float* src = ((float*)imageData) + (3 * width*j);
				for (int i = 0; i < width; i++) {
					for (int elem = 0; elem < 3; ++elem) {
						float P = *src++;
						*dst++ = P;
					}
				}
			}*/
			break;

		case RT_FORMAT_FLOAT4:
			// This buffer is upside down
			for (int j = height - 1; j >= 0; --j) {
				float *dst = &pix[0] + (1 * width*(height - 1 - j));
				float *src = ((float*)imageData) + (4 * width*j);
				for (int i = 0; i < width; i++) {
					float P = *src++;
					*dst++ = P;
					// skip green, blue and alpha
					src+= 3;
				}
			}
			break;

		default:
			fprintf(stderr, "Unrecognized buffer data type or format.\n");
			exit(2);
			break;
		}

		saveBin(&pix[0], filename, width, height, 1);

		// Now unmap the buffer
		RT_CHECK_ERROR(rtBufferUnmap(buffer));
	}
}