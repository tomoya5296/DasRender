from struct import *
import cv2
import numpy as np
import argparse
import h5py
import os
import const
import inspect
import copy


class ByteIO:
	def __init__(self, name, mode):
		enable_mode = ("rb","wb","ab")
		if mode not in enable_mode:
			raise Exception('ByteIO: file mode error. mode="' + mode + '" is unsupport.')
		self.file = open(name, mode)

	def __del__(self):
		self.close()

	def close(self):
		self.file.close()

	def writeInt(self, num):
		self.file.write(pack('<i',num))

	def writeFloat(self, real_num):
		self.file.write(pack('<f',real_num))

	def readInt(self):
		return unpack('<i',self.file.read(4))[0]

	def readFloat(self):
		return unpack('<f',self.file.read(4))[0]

class array_rgb:
	def __init__(self, filename):
		src_bin = ByteIO(filename, "rb")
		self.width = src_bin.readInt()
		self.height = src_bin.readInt()
		self.rgb = np.empty((3, self.height, self.width))
		for yd in range(0, self.height):
			for xd in range(0, self.width):
				self.rgb[0][yd][xd] = src_bin.readFloat()
				self.rgb[1][yd][xd] = src_bin.readFloat()
				self.rgb[2][yd][xd] = src_bin.readFloat()
		src_bin.close()

def clamp(x):
    if (x < 0.0):
        return 0.0
    if (x > 255.0):
        return 255.0
    return x

def GammaCorrect(value):
	if (value <= 0.0031308):
		return 12.92 * value
	return 1.055 * np.power(value, (float)(1.0 / 2.4)) - 0.055

def to_int(value):
	return int(clamp(255.0 * GammaCorrect(value) + 0.5))

def ensure_dir(dir_path):
    directory = os.path.dirname(dir_path)
    if not os.path.exists(directory):
        os.makedirs(directory)

def make_compressed_result(src_dir):
	output_img  = array_rgb(src_dir + "output.bin")
	depth_img   = array_rgb(src_dir + "depth.bin")
	texture_img = array_rgb(src_dir + "texture.bin")
	normal_img  = array_rgb(src_dir + "normal.bin")
	shadow_img  = array_rgb(src_dir + "shadow.bin")

	compressed_result = np.append(output_img.rgb, depth_img.rgb, axis=0)
	compressed_result = np.delete(compressed_result, np.s_[-2:], 0)
	compressed_result = np.append(compressed_result, texture_img.rgb, axis=0)
	compressed_result = np.append(compressed_result, normal_img.rgb, axis=0)
	compressed_result = np.append(compressed_result, shadow_img.rgb, axis=0)
	compressed_result = np.delete(compressed_result, np.s_[-2:], 0)

	return compressed_result

def makehdf(outputs_dir, src_scene):

	#load results
    index  = 0
    layers_size = copy.deepcopy(const.layers_size)
    layers_size_accum = np.cumsum(layers_size)
    layers_size_accum = np.insert(layers_size_accum, 0, 0)

    compressed_results = np.empty((layers_size_accum[-1], const.img_size, const.img_size))
    for spp in const.spp_counts:
        output_dir = outputs_dir + src_scene + str(spp) + "spp_" + str(index) + "/"
        if not os.path.exists(output_dir):
            print(output_dir + "doesn't exist")
            exit()
        
        compressed_result = make_compressed_result(output_dir)
        compressed_results[ layers_size_accum[index] : layers_size_accum[index] + layers_size[index] \
        ,:,:] = compressed_result
        print(spp, index)
        index += 1

	# #make hd5f results
    hd5f_name = outputs_dir + src_scene + "results.hdf5"
    fw = h5py.File(hd5f_name, "w")	
    fw.attrs["spp_count"] = const.spp_counts
    fw.attrs["layers_size"] = const.layers_size
    fw.create_dataset(src_scene, data=compressed_results)
    fw.close()   # be CERTAIN to close the file
	
def bin2png(dir, scene):
	src_img = array_rgb(dir + scene + "_output.bin")
	height = src_img.height
	width = src_img.width
	im = np.zeros((height, width, 3), dtype="int")
	for i in range(0, width):
		for j in range(0, height):
			im[j][i][0] = to_int(src_img.rgb[2][j][i])
			im[j][i][1] = to_int(src_img.rgb[1][j][i])
			im[j][i][2] = to_int(src_img.rgb[0][j][i])
	cv2.imwrite('scene.png', im)


if __name__ == '__main__':
	parser = argparse.ArgumentParser(
		prog = 'read_bin.py',
		description ='Read bin hdr files and save the hdf5 file'
		)

	makehdf(const.outputs_dir, const.scenename)