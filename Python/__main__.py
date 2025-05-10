from PIL import Image
import numpy as np
import sys

def filtro_mediana_rgb(img, n):
    largura, altura = img.size
    pixels = np.array(img)
    resultado = np.zeros_like(pixels)

    for i in range(altura):
        for j in range(largura):
            vizinhos_r, vizinhos_g, vizinhos_b = [], [], []
            for k in range(-n//2, n//2 + 1):
                for l in range(-n//2, n//2 + 1):
                    y, x = i + k, j + l
                    if 0 <= y < altura and 0 <= x < largura:
                        r, g, b = pixels[y, x]
                        vizinhos_r.append(r)
                        vizinhos_g.append(g)
                        vizinhos_b.append(b)
            vizinhos_r.sort()
            vizinhos_g.sort()
            vizinhos_b.sort()
            meio = len(vizinhos_r) // 2
            resultado[i, j] = (vizinhos_r[meio], vizinhos_g[meio], vizinhos_b[meio])

    return Image.fromarray(resultado)

def laplaciano(img_gray, n_mask):

    pixels_rgb = np.array(img_gray, dtype=np.int32)

    if pixels_rgb.ndim == 3:
        # Imagem RGB — extrai apenas um dos canais (são iguais)
        pixels = pixels_rgb[:, :, 0]
    else:
        # Imagem já está em modo L (1 canal)
        pixels = pixels_rgb

    altura, largura = pixels.shape
    resultado = np.zeros_like(pixels)

    # Criar máscara Laplaciana
    mask = -1 * np.ones((n_mask, n_mask), dtype=np.int32)
    meio = n_mask // 2
    mask[meio, meio] = (n_mask * n_mask) - 1

    # Aplicar convolução
    for i in range(meio, altura - meio):
        for j in range(meio, largura - meio):
            regiao = pixels[i - meio:i + meio + 1, j - meio:j + meio + 1]
            valor = np.sum(regiao * mask)
            valor = max(0, min(255, valor))  # Limita entre 0 e 255
            resultado[i, j] = valor

    return Image.fromarray(resultado.astype(np.uint8))


def main():
    if len(sys.argv) != 3:
        print(f"{sys.argv[0]} <n_mask> <n_proc>")
        sys.exit(1)

    n_mask = int(sys.argv[1])
    n_proc = int(sys.argv[2])

    print(f"n_mask: {n_mask}\nn_proc: {n_proc}")

    entrada = "borboleta.bmp"
    saida = "teste.bmp"

    try:
        img = Image.open(entrada)
    except IOError:
        print(f"Erro ao abrir o arquivo {entrada}")
        sys.exit(1)

    print(f"Tamanho da imagem: {img.size}")
    print(f"Modo: {img.mode}")  # Ex: 'RGB'

    # Converte para escala de cinza manualmente (como no C)
    gray_img = Image.new("RGB", img.size)
    pixels = img.load()
    gray_pixels = gray_img.load()

    for i in range(img.height):
        for j in range(img.width):
            r, g, b = pixels[j, i]
            gray = int(0.299 * r + 0.587 * g + 0.114 * b)
            gray_pixels[j, i] = (gray, gray, gray)

    # Aplica filtro mediana com máscara n x n
    n = 3  # ou qualquer valor ímpar >= 3
    filtrada = filtro_mediana_rgb(gray_img, n)

    filtrada.save("saida_mediana.bmp")
    
    img_bordas = laplaciano(filtrada, n_mask)
    img_bordas.save("imagem_bordas.bmp")

if __name__ == "__main__":
    main()
