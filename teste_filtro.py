
'''
    Utilizando a biblioteca OpenCV para verificar se o resultado dos filtros está de acordo.
'''
import cv2
import numpy as np
from matplotlib import pyplot as plt

# Caminho da imagem BMP
imagem_bmp = 'borboleta.bmp' 

# Carrega a imagem em escala de cinza
img = cv2.imread(imagem_bmp, cv2.IMREAD_GRAYSCALE)

if img is None:
    print("Erro ao carregar a imagem.")
    exit()


mask = 3
# Aplica filtro de mediana
img_mediana = cv2.medianBlur(img, mask)

# Aplica filtro Laplaciano
laplaciano = cv2.Laplacian(img_mediana, cv2.CV_64F, ksize=mask)
laplaciano = cv2.convertScaleAbs(laplaciano)  # Converte para 8 bits

# Exibe as imagens
plt.figure(figsize=(12, 4))
plt.subplot(1, 3, 1), plt.imshow(img, cmap='gray'), plt.title('Original')
plt.subplot(1, 3, 2), plt.imshow(img_mediana, cmap='gray'), plt.title('Filtro Mediana')
plt.subplot(1, 3, 3), plt.imshow(laplaciano, cmap='gray'), plt.title('Filtro Laplaciano')
plt.tight_layout()
plt.show()



