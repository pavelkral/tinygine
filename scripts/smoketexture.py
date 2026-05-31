import numpy as np
from PIL import Image

size = 256

x = np.linspace(-1, 1, size)
y = np.linspace(-1, 1, size)
xx, yy = np.meshgrid(x, y)
radius = np.sqrt(xx**2 + yy**2)


alpha = np.clip(1.0 - radius, 0.0, 1.0)
alpha = alpha ** 2.2 


noise = np.random.rand(size, size) * 0.3 + 0.7
alpha = np.clip(alpha * noise, 0.0, 1.0)


img = np.zeros((size, size, 4), dtype=np.uint8)
img[:,:,0] = 255 # R
img[:,:,1] = 255 # G
img[:,:,2] = 255 # B
img[:,:,3] = (alpha * 255).astype(np.uint8)

im = Image.fromarray(img)
im.save("smoke.png")
print("smoke.png created!")