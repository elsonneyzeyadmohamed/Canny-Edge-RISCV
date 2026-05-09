from PIL import Image
# Open the image, convert to grayscale ('L'), resize to 512x512
img = Image.open('tiger-animals-cat-predator-preview.jpg').convert('L').resize((512, 512))
# Save as raw binary data
with open('tiger.raw', 'wb') as f:
    f.write(img.tobytes())
print("Converted successfully to tiger.raw!")
