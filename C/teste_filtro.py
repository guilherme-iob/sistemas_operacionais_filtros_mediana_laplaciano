import cv2
import numpy as np
from matplotlib import pyplot as plt

# Caminho da imagem BMP
imagem_bmp = 'borboleta.bmp'  # Substitua pelo caminho da sua imagem

# Carrega a imagem em escala de cinza
img = cv2.imread(imagem_bmp, cv2.IMREAD_GRAYSCALE)

if img is None:
    print("Erro ao carregar a imagem.")
    exit()

# Aplica filtro de mediana com máscara 5x5
img_mediana = cv2.medianBlur(img, 7)

# Aplica filtro Laplaciano com máscara 5x5
laplaciano = cv2.Laplacian(img_mediana, cv2.CV_64F, ksize=7)
laplaciano = cv2.convertScaleAbs(laplaciano)  # Converte para 8 bits

# Exibe as imagens
plt.figure(figsize=(12, 4))
plt.subplot(1, 3, 1), plt.imshow(img, cmap='gray'), plt.title('Original')
plt.subplot(1, 3, 2), plt.imshow(img_mediana, cmap='gray'), plt.title('Filtro Mediana 5x5')
plt.subplot(1, 3, 3), plt.imshow(laplaciano, cmap='gray'), plt.title('Filtro Laplaciano 5x5')
plt.tight_layout()
plt.show()
