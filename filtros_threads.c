#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/sem.h>
#include <unistd.h>

#pragma pack(1)

/*------------------------------------------------------------------*/
/* STRUCTS */
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

typedef struct parametros
{
    RGB *imagem_original;
    int height;
    int width;
    int id_seq;
    int n_threads;
    int n_mask;
} PARAMETROS;

/*------------------------------------------------------------------*/

/*------------------------------------------------------------------*/
/* VARIÁVEIS GLOBAIS */
unsigned char *IMAGEM_ATUAL;
int barreira;
/*------------------------------------------------------------------*/


/*------------------------------------------------------------------*/
/* ETAPA DO FILTRO ESCALA DE CINZA */

/// @brief Converte uma imagem RGB em uma imagem em escala de cinza utilizando múltiplas threads e armazenando na variável da imagem global.
/// @param imagem Vetor com os valores RGB da imagem original
/// @param imagem_atual Vetor com os dados atuais da imagem
/// @param height Altura da imagem
/// @param width Largura da imagem
/// @param id_seq Número do processo
/// @param n_threads Quantidade total de threads
void converter_escala_de_cinza(RGB *imagem, unsigned char *imagem_atual, int height, int width, int id_seq, int n_threads)
{
    RGB pixel;
    int i;
    unsigned char gray_value;
    for (i = id_seq; i < height; i += n_threads)
    {
        for (int j = 0; j < width; j++)
        {
            pixel = imagem[i * width + j];
            gray_value = 0.299 * pixel.red + 0.587 * pixel.green + 0.114 * pixel.blue;
            imagem_atual[i * width + j] = gray_value;
        }
    }
}
/*------------------------------------------------------------------*/
/* ETAPA DO FILTRO MEDIANA */

/// @brief Ordena um vetor de inteiros utilizando o método de ordenação Bubble Sort
/// @param array Vetor a ser ordenado
/// @param size Tamanho do vetor a ser ordenado
void bubble_sort(unsigned char *array, int size)
{
    int i, j;
    unsigned char aux;
    for (i = 0; i < size - 1; i++)
    {
        for (j = 0; j < size - 1 - i; j++)
        {
            if (array[j] > array[j + 1])
            {
                // Troca os elementos
                aux = array[j];
                array[j] = array[j + 1];
                array[j + 1] = aux;
            }
        }
    }
}

/// @brief Aplica um filtro mediana à uma imagem em escala de cinza, suavizando-a e removendo ruídos.
/// @param entrada Vetor com os dados da imagem em escala de cinza
/// @param imagem_atual Vetor com os dados atuais da imagem
/// @param height Altura da imagem
/// @param width Largura da imagem
/// @param n_mask Tamanho da máscara
/// @param id_seq Número do processo
/// @param n_threads Quantidade total de threads
void filtro_mediana(unsigned char *entrada, unsigned char *imagem_atual, int height, int width, int n_mask, int id_seq, int n_threads)
{
    int index, y, x;
    int mask_y, mask_x, mask_index;
    int y_vizinho, x_vizinho;

    int pixel_central_mask = n_mask / 2;
    int size_mask_array = n_mask * n_mask;

    unsigned char *mask_array = (unsigned char *)malloc(size_mask_array * sizeof(unsigned char));
    unsigned char *saida = (unsigned char *)malloc(height * width * sizeof(unsigned char));

    for (index = id_seq; index < height * width; index += n_threads)
    {
        y = index / width; // coordenada Y da imagem
        x = index % width; // coordenada X da imagem

        // percorrendo a vizinhança do pixel
        int mask_index = 0;
        for (mask_y = 0; mask_y < n_mask; mask_y++)
        {
            for (mask_x = 0; mask_x < n_mask; mask_x++)
            {
                // coordenadas dos vizinhos
                y_vizinho = y + mask_y - pixel_central_mask;
                x_vizinho = x + mask_x - pixel_central_mask;

                // verifica se os vizinhos estão dentro da imagem
                if (x_vizinho >= 0 && x_vizinho < width && y_vizinho >= 0 && y_vizinho < height)
                    mask_array[mask_index++] = entrada[y_vizinho * width + x_vizinho];
                else
                    mask_array[mask_index++] = entrada[y * width + x];
            }
        }

        // ordena os valores da mask e obtém a mediana
        bubble_sort(mask_array, size_mask_array);
        saida[y * width + x] = mask_array[size_mask_array / 2];
    }

    // Cópia segura do resultado para a memória compartilhada
    for (index = id_seq; index < height * width; index += n_threads)
    {
        imagem_atual[index] = saida[index];
    }

    free(mask_array);
    free(saida);
}

/*------------------------------------------------------------------*/
/* FILTRO LAPLACIANO */

/// @brief Gera uma máscara padrão com base no tamanho informado. O ponto central da máscara é o próprio tamanho e seus vizinhos são -1;
/// @param size Tamanho da máscara
/// @param mask Matriz da máscara
void gerar_laplace_mask(int size, int **mask)
{
    int center = size / 2;
    for (int i = 0; i < size; i++)
        for (int j = 0; j < size; j++)
            mask[i][j] = -1;
    mask[center][center] = size * size - 1;
}

/// @brief Aplica um filtro laplaciano em uma imagem, destacando as bordas do conteúdo.
/// @param entrada Vetor com os dados da imagem a ser filtrada
/// @param imagem_atual Vetor com os dados atuais da imagem
/// @param height Altura da imagem
/// @param width Largura da imagem
/// @param n_mask Tamanho da máscara
/// @param id_seq Número do processo
/// @param n_threads Quantidade total de threads
void filtro_laplaciano(unsigned char *entrada, unsigned char *imagem_atual, int height, int width, int mask_size, int id_seq, int n_threads)
{
    int i, j;
    int total_pixels = height * width;
    int index, y, x;
    int mask_y, mask_x;
    int y_vizinho, x_vizinho;
    int valor_pixel;

    int pixel_central_mask = mask_size / 2;

    unsigned char *saida = (unsigned char *)malloc(height * width * sizeof(unsigned char));

    // Alocar máscara
    int **mask = (int **)malloc(mask_size * sizeof(int *));
    for (i = 0; i < mask_size; i++)
        mask[i] = (int *)malloc(mask_size * sizeof(int));

    // Preencher a máscara conforme o tamanho
    if (mask_size == 3)
    {
        int template_mask[3][3] = {
            {0, -1, 0},
            {-1, 4, -1},
            {0, -1, 0}};
        for (i = 0; i < mask_size; i++)
        {
            for (j = 0; j < mask_size; j++)
            {
                mask[i][j] = template_mask[i][j];
            }
        }
    }
    else if (mask_size == 5)
    {
        int template_mask[5][5] = {
            {0, 0, -1, 0, 0},
            {0, -1, -2, -1, 0},
            {-1, -2, 16, -2, -1},
            {0, -1, -2, -1, 0},
            {0, 0, -1, 0, 0}};
        for (i = 0; i < mask_size; i++)
        {
            for (j = 0; j < mask_size; j++)
            {
                mask[i][j] = template_mask[i][j];
            }
        }
    }
    else if (mask_size == 7)
    {
        // mascara padrão (resultado ruim)
        // int template_mask[7][7] = {
        //     {  0,  0,  0, -1,  0,  0,  0 },
        //     {  0,  0, -1, -2, -1,  0,  0 },
        //     {  0, -1, -2, -3, -2, -1,  0 },
        //     { -1, -2, -3, 48, -3, -2, -1 },
        //     {  0, -1, -2, -3, -2, -1,  0 },
        //     {  0,  0, -1, -2, -1,  0,  0 },
        //     {  0,  0,  0, -1,  0,  0,  0 }
        // };

        // mascara com mais detalhes (resultado bom)
        // int template_mask[7][7] = {
        //     {   0,   0,  -1,  -2,  -1,   0,   0 },
        //     {   0,  -3, -13, -20, -13,  -3,   0 },
        //     {  -1, -13, -35, -40, -35, -13,  -1 },
        //     {  -2, -20, -40, 512, -40, -20,  -2 },
        //     {  -1, -13, -35, -40, -35, -13,  -1 },
        //     {   0,  -3, -13, -20, -13,  -3,   0 },
        //     {   0,   0,  -1,  -2,  -1,   0,   0 }
        // };

        // mascara mais suave (resultado bom)
        int template_mask[7][7] = {
            {0, 0, -1, -1, -1, 0, 0},
            {0, -1, -3, -3, -3, -1, 0},
            {-1, -3, 0, 7, 0, -3, -1},
            {-1, -3, 7, 24, 7, -3, -1},
            {-1, -3, 0, 7, 0, -3, -1},
            {0, -1, -3, -3, -3, -1, 0},
            {0, 0, -1, -1, -1, 0, 0}};

        for (i = 0; i < mask_size; i++)
        {
            for (j = 0; j < mask_size; j++)
            {
                mask[i][j] = template_mask[i][j];
            }
        }
    }
    else
    {
        fprintf(stderr, "Tamanho de máscara inválido: %d\n", mask_size);
        for (i = 0; i < mask_size; i++)
            free(mask[i]);
        free(mask);
        return;
    }

    for (index = id_seq; index < total_pixels; index += n_threads)
    {
        y = index / width; // coordenada y da imagem
        x = index % width; // coordenada x da imagem

        // aplica a convolução
        valor_pixel = 0;
        for (mask_y = 0; mask_y < mask_size; mask_y++)
        {
            for (mask_x = 0; mask_x < mask_size; mask_x++)
            {

                // coordenadas dos vizinhos
                y_vizinho = y + mask_y - pixel_central_mask;
                x_vizinho = x + mask_x - pixel_central_mask;

                // verifica se os vizinhos estão dentro da imagem
                if (x_vizinho >= 0 && x_vizinho < width && y_vizinho >= 0 && y_vizinho < height)
                    valor_pixel += entrada[y_vizinho * width + x_vizinho] * mask[mask_y][mask_x];
            }
        }

        // Clamping
        if (valor_pixel < 0)
            valor_pixel = 0;
        if (valor_pixel > 255)
            valor_pixel = 255;

        saida[y * width + x] = (unsigned char)valor_pixel;
    }

    for (index = id_seq; index < height * width; index += n_threads)
    {
        imagem_atual[index] = saida[index];
    }

    // Liberar memória
    for (i = 0; i < mask_size; i++)
        free(mask[i]);
    free(mask);
}

/*------------------------------------------------------------------*/

/// @brief Escreve a imagem de saída em um arquivo.
/// @param fout Arquivo de saída
/// @param imagem_atual Vetor global que contém a imagem a ser gravada
/// @param height Altura da imagem
/// @param width Largura da imagem
void escrever_imagem_saida(FILE *fout, unsigned char *imagem_atual, int height, int width)
{
    RGB pixel;
    for (int i = 0; i < height; i++)
    {
        for (int j = 0; j < width; j++)
        {
            pixel.red = pixel.green = pixel.blue = imagem_atual[i * width + j];
            fwrite(&pixel, sizeof(RGB), 1, fout);
        }
    }
}

/*------------------------------------------------------------------*/
/* SEMAFOROS */

/// @brief Sinaliza à barreira de sincronização que um processo chegou em uma determinada etapa
/// @param id_barreira ID do conjunto de semáforos (barreira)
/// @param num_etapa Número que representa a etapa atual
void sinalizar_chegada(int id_barreira, int num_etapa)
{
    struct sembuf operacao;
    operacao.sem_num = num_etapa;
    operacao.sem_op = 1; // incrementa a barreira
    operacao.sem_flg = 0;
    semop(id_barreira, &operacao, 1);
}

/// @brief Espera e verifica se todos os processos chegaram até uma determinada etapa
/// @param id_barreira  ID do conjunto de semáforos (barreira)
/// @param num_etapa Número que representa a etapa atual
/// @param total Número total de processos que devem chegar na barreira
void esperar_todos_chegarem(int id_barreira, int num_etapa, int total)
{
    while (1)
    {
        int valor_atual = semctl(id_barreira, num_etapa, GETVAL);
        if (valor_atual >= total)
            break;
        usleep(1000); // espera para checar novamente
    }
}

/*------------------------------------------------------------------*/

void * aplicar_filtros(void *args)
{
    PARAMETROS *parametros = (PARAMETROS *)args;

    RGB *imagem_original = parametros->imagem_original;
    int height = parametros->height;
    int width = parametros->width;
    int id_seq = parametros->id_seq;
    int n_threads = parametros->n_threads;
    int n_mask = parametros->n_mask;

    int i;

    // filtro cinza
    converter_escala_de_cinza(imagem_original, IMAGEM_ATUAL, height, width, id_seq, n_threads);
    // sinaliza chegada
    sinalizar_chegada(barreira, 0);
    // espera todos chegarem
    esperar_todos_chegarem(barreira, 0, n_threads);
    printf("Escala de Cinza Finalizada - THREAD %d\n", id_seq);

    // Copia a variável global para um vetor para evitar condições de corrida entre threads durante o processamento do próximo filtro
    unsigned char *imagem_cinza = malloc(sizeof(unsigned char) * width * height);
    for (i = 0; i < height * width; i++)
    {
        imagem_cinza[i] = IMAGEM_ATUAL[i];
    }

    // filtro mediana
    filtro_mediana(imagem_cinza, IMAGEM_ATUAL, height, width, n_mask, id_seq, n_threads);
    // sinaliza chegada
    sinalizar_chegada(barreira, 1);
    // espera todos chegarem
    esperar_todos_chegarem(barreira, 1, n_threads);
    printf("Mediana Finalizada - THREAD %d\n", id_seq);

    // Copia a variável global para um vetor para evitar condições de corrida entre threads durante o processamento do próximo filtro
    unsigned char *imagem_pos_mediana = malloc(sizeof(unsigned char) * width * height);
    for (i = 0; i < height * width; i++)
    {
        imagem_pos_mediana[i] = IMAGEM_ATUAL[i];
    }

    // filtro laplace
    filtro_laplaciano(imagem_pos_mediana, IMAGEM_ATUAL, height, width, n_mask, id_seq, n_threads);
    // sinaliza chegada
    sinalizar_chegada(barreira, 2);
    // espera todos chegarem
    esperar_todos_chegarem(barreira, 2, n_threads);
    printf("Laplaciano Finalizado - Processo %d\n", id_seq);

    // Liberando variáveis
    free(imagem_cinza);
    free(imagem_pos_mediana);
}

/*------------------------------------------------------------------*/

int main(int argc, char **argv)
{

    /*
        Processamento paralelo de imagens utilizando processos e área de memória compartilhada


        1. Leitura de um arquivo de imagem .BMP RGB de 24 bits.
        2. Aplicar filtro de escala de cinza, mediana e laplaciano na imagem utilizando múltiplas threads.
        3. Barreira de sincronização com semáforos para garantir que um filtro só começe quando o anterior ser finalizado.
        4. Ao final, armazenar o resultado em uma imagem de saída.

    */

    FILEHEADER file_header;
    IMAGEHEADER header;
    RGB pixel;
    int i, j;
    int n_mask, n_threads;

    // Parâmetros de entrada
    if (argc != 3)
    {
        printf("%s <n_mask> <n_threads>\n", argv[0]);
        exit(0);
    }

    n_mask = atoi(argv[1]);
    n_threads = atoi(argv[2]);

    printf("n_mask: %d\nn_threads: %d\n", n_mask, n_threads);

    // Nome do arquivo de entrada
    char entrada[100] = "borboleta.bmp\0";
    // Nome do arquivo de saída
    char saida[100] = "saida_filtro.bmp\0";

    // Leitura do arquivo de entrada
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

    
    // ===================== BARREIRA DE SINCRONIZAÇÃO ===================

    // Definindo a barreira de sincronização com semáforos
    barreira = semget(1234, 3, IPC_CREAT | 0600); // criando semaforo com 3 posicoes
    semctl(barreira, 0, SETVAL, 0);               // Inicializa etapa 1 (escala de cinza) com 0
    semctl(barreira, 1, SETVAL, 0);               // Inicializa etapa 2 (mediana) com 0
    semctl(barreira, 2, SETVAL, 0);               // Inicializa etapa 3 (laplaciano) com 0

    // ===================================================================


    // Inicializa a variavel da imagem global
    IMAGEM_ATUAL = malloc(sizeof(unsigned char) * height * width);

    // Transferindo a imagem para um vetor e inicializando a memória compartilhada com valores em 0
    RGB *imagem_original = malloc(sizeof(RGB) * height * width);
    for (i = 0; i < height; i++)
    {
        for (j = 0; j < width; j++)
        {
            fread(&pixel, sizeof(RGB), 1, fin);
            imagem_original[i * width + j] = pixel;
            IMAGEM_ATUAL[i * width + j] = 0;
        }
    }
    fclose(fin);

    // ===================== THREADS ===================
    pthread_t *tid = NULL;
    PARAMETROS *parametros = NULL;

    tid = (pthread_t *)malloc(n_threads * sizeof(pthread_t));
    parametros = (PARAMETROS *)malloc(n_threads * sizeof(PARAMETROS));

    for (i = 0; i < n_threads; i++)
    {
        parametros[i].imagem_original = imagem_original;
        parametros[i].height = height;
        parametros[i].width = width;
        parametros[i].id_seq = i;
        parametros[i].n_threads = n_threads;
        parametros[i].n_mask = n_mask;
        pthread_create(&tid[i], NULL, aplicar_filtros, (void *) &parametros[i]);
    }

    for (i = 0; i < n_threads; i++)
    {
        pthread_join(tid[i], NULL);
    }
    // =================================================

    // Preparando o arquivo de saída
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

    // Armazenando o conteúdo da variável global na saída
    escrever_imagem_saida(fout, IMAGEM_ATUAL, height, width);

    fclose(fout);
    free(imagem_original);
    free(IMAGEM_ATUAL);
}