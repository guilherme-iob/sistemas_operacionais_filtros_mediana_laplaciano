#include <stdio.h>
#include <stdlib.h>
#include <sys/shm.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/sem.h>

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
/* ETAPA DO FILTRO ESCALA DE CINZA */
void converter_escala_de_cinza(RGB *imagem, unsigned char *memoria_compartilhada, int height, int width, int id_seq, int n_proc)
{
    RGB pixel;
    for (int i = id_seq; i < height; i += n_proc)
    {
        for (int j = 0; j < width; j++)
        {
            pixel = imagem[i * width + j];
            unsigned char gray = 0.299 * pixel.red + 0.587 * pixel.green + 0.114 * pixel.blue;
            memoria_compartilhada[i * width + j] = gray;
        }
    }
}
/*------------------------------------------------------------------*/
/* ETAPA DO FILTRO MEDIANA */

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

void filtro_mediana(unsigned char *entrada, unsigned char *memoria_compartilhada, int height, int width, int n_mask, int id_seq, int n_proc)
{
    int pixel_central = n_mask / 2;
    int size_mask_array = n_mask * n_mask;

    unsigned char *mask_array = (unsigned char *)malloc(size_mask_array * sizeof(unsigned char));
    unsigned char *saida = (unsigned char *)malloc(height * width * sizeof(unsigned char));

    for (int idx = id_seq; idx < height * width; idx += n_proc)
    {
        int i = idx / width;
        int j = idx % width;
        int k = 0;

        for (int m = 0; m < n_mask; m++)
        {
            for (int n = 0; n < n_mask; n++)
            {
                int y = i + m - pixel_central;
                int x = j + n - pixel_central;

                if (x >= 0 && x < width && y >= 0 && y < height)
                    mask_array[k++] = entrada[y * width + x];
                else
                    mask_array[k++] = 0;
            }
        }

        bubble_sort(mask_array, size_mask_array);
        saida[i * width + j] = mask_array[size_mask_array / 2];
    }

    // Cópia segura do resultado para a memória compartilhada
    for (int idx = id_seq; idx < height * width; idx += n_proc)
    {
        memoria_compartilhada[idx] = saida[idx];
    }

    free(mask_array);
    free(saida);
}



/*------------------------------------------------------------------*/
/* FILTRO LAPLACIANO */

void gerar_laplace_mask(int size, int **mask)
{
    int center = size / 2;
    for (int i = 0; i < size; i++)
        for (int j = 0; j < size; j++)
            mask[i][j] = -1;
    mask[center][center] = size * size - 1;
}


void filtro_laplaciano(unsigned char *entrada, unsigned char *memoria_compartilhada, int height, int width, int mask_size, int id_seq, int n_proc)
{
    int i, j, m, n;
    int center = mask_size / 2;

    // Alocar e gerar máscara
    int **mask = (int **)malloc(mask_size * sizeof(int *));
    for (i = 0; i < mask_size; i++)
        mask[i] = (int *)malloc(mask_size * sizeof(int));
    gerar_laplace_mask(mask_size, mask);

    unsigned char *saida = (unsigned char *)malloc(height * width * sizeof(unsigned char));

    int total_pixels = height * width;

    // Cada processo itera por pixel intercalado
    for (int idx = id_seq; idx < total_pixels; idx += n_proc) {
        int y = idx / width;
        int x = idx % width;

        int sum = 0;
        for (m = 0; m < mask_size; m++) {
            for (n = 0; n < mask_size; n++) {
                int yy = y + m - center;
                int xx = x + n - center;

                if (xx >= 0 && xx < width && yy >= 0 && yy < height)
                    sum += entrada[yy * width + xx] * mask[m][n];
                else
                    sum += 0; // vizinho fora da imagem = zero
            }
        }

        // Clamping
        if (sum < 0) sum = 0;
        if (sum > 255) sum = 255;

        saida[y * width + x] = (unsigned char)sum;
    }

    // Copiar para a memória compartilhada
    for (int idx = id_seq; idx < total_pixels; idx += n_proc) {
        int y = idx / width;
        int x = idx % width;
        memoria_compartilhada[idx] = saida[y * width + x];
    }

    // Liberar memória
    free(saida);
    for (i = 0; i < mask_size; i++)
        free(mask[i]);
    free(mask);
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
/* SEMAFOROS */

void sinalizar_chegada(int id_barreira, int num_etapa)
{
    struct sembuf operacao;
    operacao.sem_num = num_etapa;
    operacao.sem_op = 1; // incrementa a barreira
    operacao.sem_flg = 0;
    semop(id_barreira, &operacao, 1);
}

void esperar_todos_chegarem(int id_barreira, int num_etapa, int total)
{
    while (1)
    {
        int valor_atual = semctl(id_barreira, num_etapa, GETVAL);
        if (valor_atual >= total)
            break;
        usleep(1000); // Espera um pouquinho antes de checar de novo
    }
}

/*------------------------------------------------------------------*/

int main(int argc, char **argv)
{
    FILEHEADER file_header;
    IMAGEHEADER header;
    RGB pixel;
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

    char entrada[100] = "borboleta.bmp\0";
    char saida[100] = "saida_teste.bmp\0";

    FILE *fin = fopen(entrada, "rb");

    if (fin == NULL)
    {
        printf("Erro ao abrir o arquivo %s\n", entrada);
        exit(0);
    }

    fread(&file_header, sizeof(FILEHEADER), 1, fin);
    fread(&header, sizeof(IMAGEHEADER), 1, fin);

    int width = header.width;
    int height = header.height;

    int shmid, chave = 5;
    int pid, id_seq;

    int barreira = semget(1234, 3, IPC_CREAT | 0600); // criando semaforo com 2 posicoes
    semctl(barreira, 0, SETVAL, 0);                   // Inicializa etapa 1 com 0
    semctl(barreira, 1, SETVAL, 0);                   // Inicializa etapa 2 com 0
    semctl(barreira, 2, SETVAL, 0);                   // Inicializa etapa 3 com 0

    shmid = shmget(chave, sizeof(unsigned char) * height * width, 0600 | IPC_CREAT);
    unsigned char *memoria_compartilhada = shmat(shmid, 0, 0);

    RGB *imagem_original = malloc(sizeof(RGB) * height * width);
    for (i = 0; i < height; i++)
    {
        for (j = 0; j < width; j++)
        {
            fread(&pixel, sizeof(RGB), 1, fin);
            imagem_original[i * width + j] = pixel;
            memoria_compartilhada[i*width+j] = 0;
        }
    }
    fclose(fin);

    id_seq = 0;
    for (int i = 0; i < n_proc; i++)
    {
        pid = fork();
        if (pid == 0)
        {
            id_seq = i;
            break;
        }
    }

    if (pid == 0)
    {
        printf("Processo filho: %d\n", id_seq);

        converter_escala_de_cinza(imagem_original, memoria_compartilhada, height, width, id_seq, n_proc);
        sinalizar_chegada(barreira, 0);
        esperar_todos_chegarem(barreira, 0, n_proc);

        unsigned char *imagem_cinza = malloc(sizeof(unsigned char) * width *height);
        for(i = 0; i < height*width;i++){
            imagem_cinza[i] = memoria_compartilhada[i];
        }

        filtro_mediana(imagem_cinza, memoria_compartilhada, height, width, n_mask, id_seq, n_proc);
        sinalizar_chegada(barreira, 1);
        esperar_todos_chegarem(barreira, 1, n_proc);

        unsigned char *imagem_pos_mediana = malloc(sizeof(unsigned char) * width * height);
        for (i = 0; i < height * width; i++) {
            imagem_pos_mediana[i] = memoria_compartilhada[i];
        }
        
        filtro_laplaciano(imagem_pos_mediana, memoria_compartilhada, height, width, n_mask, id_seq, n_proc);
        sinalizar_chegada(barreira, 2);
        esperar_todos_chegarem(barreira, 2, n_proc);

        free(imagem_cinza);
        free(imagem_pos_mediana);
        shmdt(memoria_compartilhada);
    }
    else
    {
        for (int i = 0; i < n_proc; i++)
        {
            printf("Esperando processo %d\n", i);
            wait(NULL);
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

        printf("pid pai = %d\n", pid);
        escrever_imagem_saida(fout, memoria_compartilhada, height, width);

        fclose(fout);

        free(imagem_original);
        semctl(barreira, 0, IPC_RMID);
        shmdt(memoria_compartilhada);
        shmctl(shmid, IPC_RMID, 0);
    }
}