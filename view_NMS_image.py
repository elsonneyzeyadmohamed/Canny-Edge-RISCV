import numpy as np
import matplotlib.pyplot as plt
import os

file_path = 'output.raw'

def final_recovery():
    if not os.path.exists(file_path):
        return

    # 1. Read as 16-bit
    raw_data = np.fromfile(file_path, dtype=np.uint16)
    
    # 2. Define the Architecture
    memory_width = 1024  # How the data is stored in RAM
    image_width = 728    # The actual tiger width
    image_height = 485   # The actual tiger height
    
    try:
        # 3. Reshape using the MEMORY width (the stride)
        # We calculate height based on the memory width
        total_height = len(raw_data) // memory_width
        full_buffer = raw_data[:memory_width * total_height].reshape((total_height, memory_width))
        
        # 4. CROP the padding out to see the real image
        final_image = full_buffer[:image_height, :image_width]
        
        plt.figure(figsize=(10, 6))
        plt.imshow(final_image, cmap='gray')
        plt.title("Tiger Recovered - Stride Corrected")
        plt.axis('off')
        
        plt.savefig('NMS_Tiger_Success.png')
        print("Success! The stride has been corrected.")
        plt.show()
        
    except Exception as e:
        print(f"Error: {e}")

if __name__ == "__main__":
    final_recovery()
