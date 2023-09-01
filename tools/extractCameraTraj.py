import numpy as np
import os
from os.path import join as pjoin
import sys

filepath = sys.argv[1]

with open(filepath, 'r') as f:
    cameras = f.readlines()

cameras = cameras[1:-1]
num = len(cameras)

with open(filepath.replace(".txt", ".processed.txt"), "w") as f:
    f.write(str(num) + "\n")

    for camera in cameras:
        camera = camera.split("Transform")[1].rstrip("\n")
        camera = camera.split("float3(")[1:]
        pos = camera[0].split(")")[0].replace(",", "")
        tar = camera[1].split(")")[0].replace(",", "")
        up = camera[2].split(")")[0].replace(",", "")

        res = " ".join([pos, tar, up]) + "\n"

        f.write(res)
