from struct import *
import cv2
import numpy as np
import argparse
import h5py
import os
import const
import time

def main():
    parser = argparse.ArgumentParser(description='Run render for each spp_count')
    args = parser.parse_args()
    exe_file_abspath = os.path.abspath(const.exe_file)

    index  = 0
    for spp in const.spp_counts:
        output_dir = const.outputs_dir + const.scenename + str(spp) + "spp_" + str(index) + "/"
        if not os.path.exists(output_dir):
            os.makedirs(output_dir)

        os.system('%s --file %s --spp %d --id %d' % (exe_file_abspath, output_dir, spp, index))
        index += 1
        time.sleep(5)



if __name__ == '__main__':
    main()