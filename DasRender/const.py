import numpy as np

exe_file = "../build/bin/Release/DasRender.exe"
outputs_dir = "../output/"
scenename = "cornellbox_caustics/"
spp_counts_hdf5  = np.array([ 1,  1,  1,  1,  2,  2,  2,  4,  4,  8,  8, 16, 0])
spp_counts_render  = np.array([ 1,  1,  1,  1,  2,  2,  2,  4,  4,  8,  8, 16, 1024])
layers_size = np.array([11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11  ])
img_size = 512