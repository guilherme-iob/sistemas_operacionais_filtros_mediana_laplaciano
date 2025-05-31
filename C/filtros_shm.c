#include <stdio.h>
#include <stdlib.h>
#include <sys/shm.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>

#pragma pack(1)

typedef struct fileheader
{
    unsigned short type;
    unsigned int size_file;
    unsigned short reservad1;
    unsigned short reservad2;
    unsigned int offset;
} FILEHEADER;

typedef struct imageheader
{
    unsigned int size_image_header;
    int width;
    int height;
    unsigned short planes;
    unsigned short bits_per_pixel;
    unsigned int compression;
    unsigned int image_size;
    int wresolution;
    int hresolution;
    unsigned int number_colors;
    unsigned int significant_colors;
} IMAGEHEADER;

typedef struct rgb
{
    unsigned char blue;
    unsigned char green;
    unsigned char red;
} RGB;

/*------------------------------------------------------------------*/
void bubble_sort(unsigned char *arr, int size)
{
    int i, j;
    unsigned char temp;

    // Percorrer a lista
    for (i = 0; i < size - 1; i++)
    {
        // Percorrer a lista até o penúltimo elemento
        for (j = 0; j < size - 1 - i; j++)
        {
            if (arr[j] > arr[j + 1])
            {
                // Troca os elementos
                temp = arr[j];
                arr[j] = arr[j + 1];
                arr[j + 1] = temp;
            }
        }
    }
}

// void filtro_mediana(unsigned char *memoria_compartilhada, int height, int width, int n_mask, int id_seq, int n_proc)
// {
//     int pixel_central = n_mask / 2;
//     int i, j, m, n;

//     int size_mask_array = n_mask * n_mask;
//     unsigned char *mask_array = (unsigned char *)malloc(size_mask_array * sizeof(unsigned char));

//     unsigned char *saida = (unsigned char *)malloc(height*width*sizeof(unsigned char));

//     // Lendo as linhas da imagem. Começando em i = pixel_central para garantir os valores ao redor do pixel
//     for (i = pixel_central + id_seq; i < height - pixel_central; i += n_proc)
//     {
//         // Lendo as colunas da imagem. Começando em j = pixel_central para garantir os valores ao redor do pixel
//         for (j = pixel_central; j < width - pixel_central; j++)
//         {
//             int k = 0;
//             // Obtendo todos os valores dos pixels da mask
//             for (m = 0; m < n_mask; m++)
//             {
//                 for (n = 0; n < n_mask; n++)
//                 {
//                     // convertendo a vizinhança do pixel atual da imagem para a mask
//                     // i = index da linha real da imagem; j = index da coluna real da imagem
//                     // m = index da linha da mask; n = index da coluna da mask
//                     int y = i + m - pixel_central;
//                     int x = j + n - pixel_central;
//                     mask_array[k++] = memoria_compartilhada[y * width + x]; // armazenando os pixels no array mask
//                 }
//             }
//             bubble_sort(mask_array, size_mask_array);       // ordena os valores armazenados
//             saida[i * width + j] = mask_array[size_mask_array / 2]; // obtém o valor mediano
//         }
//     }

//     // for(i=0; i< height*width;i++){
//     //     memoria_compartilhada[i] = saida[i];
//     // }

//     for (i = pixel_central + id_seq; i < height - pixel_central; i += n_proc) {
//         for (j = pixel_central; j < width - pixel_central; j++) {
//             memoria_compartilhada[i * width + j] = saida[i * width + j];
//         }
//     }

//     free(saida);
//     free(mask_array);
// }

void filtro_mediana(unsigned char *memoria_compartilhada, int height, int width, int n_mask, int id_seq, int n_proc)
{
    int pixel_central = n_mask / 2;
    int i, j, m, n;

    int size_mask_array = n_mask * n_mask;
    unsigned char *mask_array = (unsigned char *)malloc(size_mask_array * sizeof(unsigned char));
    unsigned char *saida = (unsigned char *)malloc(height * width * sizeof(unsigned char));

    // Definindo blocos de linhas válidas para cada processo
    int linhas_validas = height - 2 * pixel_central;
    int bloco = linhas_validas / n_proc;
    int resto = linhas_validas % n_proc;

    // Distribui o restante entre os primeiros processos
    int inicio = pixel_central + id_seq * bloco + (id_seq < resto ? id_seq : resto);
    int linhas_do_processo = bloco + (id_seq < resto ? 1 : 0);
    int fim = inicio + linhas_do_processo;

    for (i = inicio; i < fim; i++) {
        for (j = pixel_central; j < width - pixel_central; j++) {
            int k = 0;

            // Preenche a máscara com os valores da vizinhança
            for (m = 0; m < n_mask; m++) {
                for (n = 0; n < n_mask; n++) {
                    int y = i + m - pixel_central;
                    int x = j + n - pixel_central;
                    mask_array[k++] = memoria_compartilhada[y * width + x];
                }
            }

            // Ordena a máscara e pega o valor mediano
            bubble_sort(mask_array, size_mask_array);
            saida[i * width + j] = mask_array[size_mask_array / 2];
        }
    }

    // Escreve de volta na memória compartilhada apenas os pixels modificados
    for (i = inicio; i < fim; i++) {
        for (j = pixel_central; j < width - pixel_central; j++) {
            memoria_compartilhada[i * width + j] = saida[i * width + j];
        }
    }

    free(mask_array);
    free(saida);
}


/*------------------------------------------------------------------*/

unsigned char **alocar_matriz(int height, int width)
{
    unsigned char **matrix = (unsigned char **)malloc(height * sizeof(unsigned char *));
    for (int i = 0; i < height; i++)
        matrix[i] = (unsigned char *)malloc(width * sizeof(unsigned char));
    return matrix;
}

void desalocar_matriz(unsigned char **matrix, int height)
{
    for (int i = 0; i < height; i++)
        free(matrix[i]);
    free(matrix);
}
/*------------------------------------------------------------------*/

void converter_escala_de_cinza(RGB *imagem, unsigned char *memoria_compartilhada, int height, int width, int id_seq, int n_proc)
{
    RGB pixel;
    for (int i = id_seq; i < height; i+=n_proc)
    {
        for (int j = 0; j < width; j++)
        {
            pixel = imagem[i*width + j];
            unsigned char gray = 0.299 * pixel.red + 0.587 * pixel.green + 0.114 * pixel.blue;
            memoria_compartilhada[i * width + j] = gray;
        }
    }
}
/*------------------------------------------------------------------*/

void escrever_imagem_saida(FILE *fout, unsigned char *memoria_compartilhada, int height, int width)
{
    RGB pixel;
    for (int i = 0; i < height; i++)
    {
        for (int j = 0; j < width; j++)
        {
            pixel.red = pixel.green = pixel.blue = memoria_compartilhada[i * width + j];
            fwrite(&pixel, sizeof(RGB), 1, fout);
        }
    }
}
/*------------------------------------------------------------------*/

void gerar_laplace_mask(int size, int **mask)
{
    int center = size / 2;
    for (int i = 0; i < size; i++)
        for (int j = 0; j < size; j++)
            mask[i][j] = -1;
    mask[center][center] = size * size - 1;
}

/*------------------------------------------------------------------*/

// void filtro_laplaciano(unsigned char *memoria_compartilhada, int height, int width, int mask_size, int id_seq, int n_proc)
// {
//     int i, j, m, n;
//     int **mask;
//     int center = mask_size / 2;

//     // Alocar máscara
//     mask = (int **)malloc(mask_size * sizeof(int *));
//     for (i = 0; i < mask_size; i++)
//         mask[i] = (int *)malloc(mask_size * sizeof(int));

//     gerar_laplace_mask(mask_size, mask);

//     unsigned char *saida = (unsigned char *)malloc(height*width*sizeof(unsigned char));

//     // Aplicar convolução
//     for (i = center + id_seq; i < height - center; i+=n_proc)
//     {
//         for (j = center; j < width - center; j++)
//         {
//             int sum = 0;
//             for (m = 0; m < mask_size; m++)
//             {
//                 for (n = 0; n < mask_size; n++)
//                 {
//                     int y = i + m - center;
//                     int x = j + n - center;
//                     sum += memoria_compartilhada[y * width + x] * mask[m][n];
//                 }
//             }
//             if (sum < 0)
//                 sum = 0;
//             if (sum > 255)
//                 sum = 255;
//             saida[i * width + j] = (unsigned char)sum;
//         }
//     }

//     // Copiar as linhas processadas de volta para memória compartilhada
//     for (i = center + id_seq; i < height - center; i += n_proc)
//     {
//         for (j = center; j < width - center; j++)
//         {
//             memoria_compartilhada[i * width + j] = saida[i * width + j];
//         }
//     }

//     // Liberar memória
//     free(saida);

//     // Liberar máscara
//     for (i = 0; i < mask_size; i++)
//     {
//         free(mask[i]);
//     }

//     free(mask);
// }


void filtro_laplaciano(unsigned char *memoria_compartilhada, int height, int width, int mask_size, int id_seq, int n_proc)
{
    int i, j, m, n;
    int center = mask_size / 2;

    // Alocar máscara
    int **mask = (int **)malloc(mask_size * sizeof(int *));
    for (i = 0; i < mask_size; i++)
        mask[i] = (int *)malloc(mask_size * sizeof(int));

    gerar_laplace_mask(mask_size, mask);

    unsigned char *saida = (unsigned char *)malloc(height * width * sizeof(unsigned char));

    // Calcular divisão de blocos
    int linhas_validas = height - 2 * center;
    int bloco = linhas_validas / n_proc;
    int resto = linhas_validas % n_proc;

    int inicio = center + id_seq * bloco + (id_seq < resto ? id_seq : resto);
    int linhas_do_processo = bloco + (id_seq < resto ? 1 : 0);
    int fim = inicio + linhas_do_processo;

    //printf("Processo: %d inicio: %d fim: %d linhas validas: %d bloco: %d resto: %d\n", id_seq, inicio, fim, linhas_validas, bloco, resto);
    for (i = inicio; i < fim; i++) {
        for (j = center; j < width - center; j++) {
            int sum = 0;
            for (m = 0; m < mask_size; m++) {
                for (n = 0; n < mask_size; n++) {
                    int y = i + m - center;
                    int x = j + n - center;
                    sum += memoria_compartilhada[y * width + x] * mask[m][n];
                }
            }

            // Clamping
            if (sum < 0)
                sum = 0;
            if (sum > 255)
                sum = 255;

            saida[i * width + j] = (unsigned char)sum;
        }
    }

    // Copiar de volta para a memória compartilhada
    for (i = inicio; i < fim; i++) {
        for (j = center; j < width - center; j++) {
            memoria_compartilhada[i * width + j] = saida[i * width + j];
        }
    }

    // Libera a saída e máscara
    free(saida);
    for (i = 0; i < mask_size; i++)
        free(mask[i]);
    free(mask);
}

/*------------------------------------------------------------------*/


int main(int argc, char **argv)
{

    // char entrada[100], saida[100];
    FILEHEADER file_header;
    IMAGEHEADER header;
    RGB pixel;
    RGB aux;
    int i, j;
    int n_mask, n_proc;

    if (argc != 3)
    {
        printf("%s <n_mask> <n_proc>\n", argv[0]);
        exit(0);
    }

    n_mask = atoi(argv[1]);
    n_proc = atoi(argv[2]);

    printf("n_mask: %d\nn_proc: %d\n", n_mask, n_proc);

    // printf("Digite o nome do arquivo de entrada:\n");
    // scanf("%s", entrada);

    // printf("Digite o nome do arquivo de saida:\n");
    // scanf("%s", saida);

    char entrada[100] = "borboleta.bmp\0";
    char saida[100] = "saida_shm.bmp\0";

    FILE *fin = fopen(entrada, "rb");

    if (fin == NULL)
    {
        printf("Erro ao abrir o arquivo %s\n", entrada);
        exit(0);
    }

    FILE *fout = fopen(saida, "wb");
    if (fout == NULL)
    {
        printf("Erro ao abrir o arquivo %s\n", saida);
        exit(0);
    }

    fread(&file_header, sizeof(FILEHEADER), 1, fin);
    fread(&header, sizeof(IMAGEHEADER), 1, fin);

    printf("Tamanho da imagem: %u\n", file_header.size_file);
    printf("Largura: %d\n", header.width);
    printf("Altura: %d\n", header.height);
    printf("Bits por pixel: %d\n", header.bits_per_pixel);

    fwrite(&file_header, sizeof(FILEHEADER), 1, fout);
    fwrite(&header, sizeof(IMAGEHEADER), 1, fout);

    int width = header.width;
    int height = header.height;


    int shmid, chave = 5;
    int pid, id_seq;

    // Criando a área de memória compartilhada
    shmid = shmget(chave, sizeof(unsigned char) * height * width, 0600 | IPC_CREAT);
    unsigned char *memoria_compartilhada = shmat(shmid, 0, 0); // áre de memória compartilhada é um vetor linear (1D)


    RGB *imagem_rgb = malloc(sizeof(RGB) * height * width);
	fread(imagem_rgb, sizeof(RGB), height * width, fin);

    id_seq = 0;
    for(i=0; i < n_proc; i++){
        pid = fork();
        if(pid == 0){
            id_seq = i;
            converter_escala_de_cinza(imagem_rgb, memoria_compartilhada, height, width, id_seq, n_proc);
            shmdt(memoria_compartilhada);
            break;
        }
    }

    for(i=0;i<n_proc;i++){
        wait(NULL);
    }

    id_seq = 0;
    for(i=0; i < n_proc; i++){
        pid = fork();
        if(pid == 0){
            id_seq = i;
            filtro_mediana(memoria_compartilhada, height, width, n_mask, id_seq, n_proc);
            shmdt(memoria_compartilhada);
            break;
        }
    }

    for(i=0;i<n_proc;i++){
        wait(NULL);
    }

    id_seq = 0;
    for(i=0; i < n_proc; i++){
        pid = fork();
        if(pid == 0){
            id_seq = i;
            filtro_laplaciano(memoria_compartilhada, height, width, n_mask, id_seq, n_proc);
            shmdt(memoria_compartilhada);
            break;
        }
    }

    for(i=0;i<n_proc;i++){
        wait(NULL);
    }

    escrever_imagem_saida(fout, memoria_compartilhada, height, width);
    
    fclose(fin);
    fclose(fout);

    free(imagem_rgb);
    // Liberando a área de memória compartilhada
    shmdt(memoria_compartilhada);
    shmctl(shmid, IPC_RMID, 0);

    
}