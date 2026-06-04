from PIL import Image

# 1. Read the raw binary data
with open('output_magnitudes.raw', 'rb') as f:
    raw_data = f.read()

# 2. Tell Python it is a 512x512 Grayscale ('L') image
img = Image.frombytes('L', (512, 512), raw_data)

# 3. Save it as a standard PNG
img.save('final_tiger.png')
print("Success! Image saved as final_tiger.png")
