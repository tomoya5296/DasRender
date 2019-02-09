from struct import *
import cv2
import numpy as np
import argparse
import h5py
import os

exe_file = "C:/Users/Tomoya/Rendering/DasRender/build/bin/Release/DasRender.exe"
outputs_dir = "../output/"
scenename = "cornellbox/"
spp_counts = np.array([1, 1, 1, 1, 2, 2, 2, 4, 4, 8, 8, 16, 1024])

def main():
    parser = argparse.ArgumentParser(description='Run render for each spp_count')
    args = parser.parse_args()

    index  = 0
    for spp in spp_counts:
        output_dir = outputs_dir + scenename + str(spp) + "spp_" + str(index) + "/"
        if not os.path.exists(output_dir):
            os.makedirs(output_dir)

        os.system('%s --file %s --spp %d --id %d' % (exe_file, output_dir, spp, index))
        index += 1


if __name__ == '__main__':
    main()