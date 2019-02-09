import numpy as np

exe_file = "../build/bin/Release/DasRender.exe"
outputs_dir = "../output/"
scenename = "cornellbox/"
spp_counts  = np.array([ 1,  1,  1,  1,  2,  2,  2,  4,  4,  8,  8, 16, 1024])
layers_size = np.array([11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11  ])
img_size = 512