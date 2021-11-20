// Lucas Gabriel Mendes Miranda, 10265892
// Lucas Gabriel de Araujo Silva, 11218880

#include <omp.h>
#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>

// macros
#define NUMTHREADS 2 // número de threads em cada processo MPI
#define N 3          // ordem das matrizes A, B e C
#define SEED 2021    // semente para gerar matriz aleatória
#define SEND_ROWS 1
#define SEND_B 2
#define SEND_C 3

// returns the greater between two ints
int max(int a, int b)
{
    if (a > b)
    {
        return a;
    }
    else
    {
        return b;
    }
}

// retorna uma matriz de inteiros com ordem N aleatória
// retorna NULL se ocorreu algum erro de alocação
int *getRandomMatrix()
{
    int i;
    int *matrix;

    // inicializa gerador de números aleatórios
    srand(SEED);

    // aloca a matriz contiguamente (como se fosse um vetor)
    matrix = (int *)calloc(N * N, sizeof(int));

    // checa erro de alocação
    if (!matrix)
    {
        return NULL;
    }

    // inicializa todos os elementos com inteiros aleatórios
    for (i = 0; i < N * N; i++)
    {
        matrix[i] = rand() % 100;
    }

    // retorna a matriz criada
    return matrix;
}

void print_matrix(int *matrix, int num_el)
{
    for (int i = 0; i < num_el; i++)
    {
        if (i == 0)
        {
            printf("[[");
        }

        if (i % N == 0 && i != 0)
        {
            printf("[");
        }

        printf("%d", matrix[i]);

        if (((i + 1) % N == 0) && (i != num_el - 1))
        {
            printf("],\n");
        }
        else if (i == num_el - 1)
        {
            printf("]]\n\n");
        }
        else
        {
            printf(", ");
        }
    }
}

// multiplica duas matrizes A e B
int *matrix_mult(int *A, int *B, int rows_per_proc)
{
    int i, j, k, offset;
    int new_el;
    int *result = calloc(rows_per_proc * N, sizeof(int)); // guarda o resultado da multiplicação

    #pragma omp parallel for  (i, j, new_el, offset, k) shared(result) num_threads(NUMTHREADS)
    for (i = 0; i < rows_per_proprivatec; i++)
    {
        for (j = 0; j < N; j++)
        {
            offset = j;
            new_el = 0;
            for (k = 0; k < N; k++)
            {
                new_el += A[i * N + k] * B[offset];
                offset += N;
            }
            result[i * N + j] = new_el;
        }
    }

    return result;
}

// função principal
int main(int argc, char *argv[])
{
    int *A, *B, *C;     // matrizes, tal que A * B = C
    int my_rank;        // rank do processo em execução (MPI)
    int num_procs;      // número de processos em execução
    int rows_per_procs; // número de linhas de C que cada processo MPI deverá calcular
    int remainder_rows; // número de linhas de C que o processo 0 deverá calcular
    int els_master;     // número de elementos de C que o processo master MPI deverá calcular
    int els_per_procs;  // número de elementos de C que cada processo MPI deverá calcular
    int i, j, offset;   // para loops

    MPI_Init(&argc, &argv); // inicializa MPI

    MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);   // preenche a variável my_rank
    MPI_Comm_size(MPI_COMM_WORLD, &num_procs); // preenche a variável num_procs

    if (my_rank == 0)
    {
        // inicializa as matrizes A e B (ordem N) com inteiros aleatórios
        A = getRandomMatrix();
        B = getRandomMatrix();

        // também inicializa a matriz C com inteiros aleatórios, mas isso não importa, porque os dados de C vão ser sobrescritos com
        // o resultado de A*B
        C = getRandomMatrix();

        // pega o número de linhas de C que devem ser calculadas por processo MPI
        rows_per_procs = max(N / (num_procs - 1), 1);
        remainder_rows = N % (num_procs - 1);
        els_master = remainder_rows * N;
        els_per_procs = rows_per_procs * N;

        // manda os dados necessários para todos os outros processos
        // por "dados necessários", eu quero dizer:
        // 1. linhas da coluna A necessárias para o cálculo das linhas da coluna C;
        // 2. toda a matriz B.
        offset = 0;
        for (i = 1; i < num_procs; i++)
        {
            if (offset < N * N)
            {
                MPI_Send(&A[offset], els_per_procs, MPI_INT, i, SEND_ROWS, MPI_COMM_WORLD);
                MPI_Send(B, N * N, MPI_INT, i, SEND_B, MPI_COMM_WORLD);
                offset += els_per_procs;
            }
        }

        // armazena elementos lidados pelo processo 0 (se eles existirem)
        int *rows_master;
        int *res_rows_master = NULL;

        // eles só existem, se sobraram linhas
        if (remainder_rows)
        {
            rows_master = calloc(remainder_rows, sizeof(int));

            for (i = 0; i < els_master; i++)
            {
                rows_master[i] = A[i + offset];
            }

            res_rows_master = matrix_mult(rows_master, B, remainder_rows);
        }

        // recebe as linhas de C calculadas pelos outros processos
        offset = 0;
        for (i = 1; i < num_procs; i++)
        {
            if (offset < N * N)
            {
                MPI_Recv(&C[offset], els_per_procs, MPI_INT, i, SEND_C, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                offset += els_per_procs;
            }
        }

        // preenche o restante de C
        if (remainder_rows)
        {
            for (i = offset; i < N * N; i++)
            {
                C[i] = res_rows_master[i - offset];
            }
        }

        printf(">>THIS IS 'A':\n\n");
        print_matrix(A, N * N);
        printf(">>THIS IS 'B':\n\n");
        print_matrix(B, N * N);
        printf(">>THIS IS 'C':\n\n");
        print_matrix(C, N * N);
    }
    else // processos de rank != 0
    {
        // aloca espaço para as linhas que serão recebidas
        rows_per_procs = N / (num_procs - 1);
        els_per_procs = rows_per_procs * N;

        // armazena as linhas de A
        int *received_rows = calloc(els_per_procs, sizeof(int));

        // armazena a matriz B
        int *received_B = calloc(N * N, sizeof(int));

        // deve armazenar o resultado da multiplicação de A com a matriz B
        // essencialmente, são algumas linhas da matriz C
        int *res_rows;

        // recebe linhas da matriz A do processo de rank 0
        MPI_Recv(received_rows, els_per_procs, MPI_INT, 0, SEND_ROWS, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

        // recebe a matriz B do processo de rank 0
        MPI_Recv(received_B, N * N, MPI_INT, 0, SEND_B, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

        // multiplica as linhas recebidas por todas as colunas de B
        res_rows = matrix_mult(received_rows, received_B, rows_per_procs);

        // manda as linhas de 'C' calculadas para o processo de rank 0 (ele deve juntar tudo e imprimir)
        MPI_Send(res_rows, els_per_procs, MPI_INT, 0, SEND_C, MPI_COMM_WORLD);
    }

    MPI_Finalize();
    return 0;
}
