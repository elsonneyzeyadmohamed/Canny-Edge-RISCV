import numpy as np
from PIL import Image
import sys

raw_file = sys.argv[1] if len(sys.argv) > 1 else '/tmp/nms_out.raw'
out_file = sys.argv[2] if len(sys.argv) > 2 else '/tmp/nms_out.png'

data = np.frombuffer(open(raw_file, 'rb').read(), dtype=np.uint8).reshape(512, 512)
Image.fromarray(data).save(out_file)
print(f'Saved: {out_file}')
