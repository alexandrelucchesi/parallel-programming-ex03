/**
 * Alexandre Lucchesi Alencar
 *
 * Programação Paralela - Prof. George Teodoro
 *
 * Exercício de Programacão 03 - Sum Tree
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <mpi.h>
#include <unistd.h>
#include <math.h>


void generate_floats(FILE* out, int count);


/**
 * Utilitary function to generate a file containing random data in the
 * appropriate format to be processed by the program.
 */
void generate_floats(FILE* out, int count) {
    srand(time(NULL));
    fprintf(out, "%d\n", count);
    for (int i = 0; i < count; i++) {
        float r = (float) rand() / (float) (rand() + 1);
        fprintf(out, "%.2f ", r);
    }
    putc('\n', out);
} /* generate_floats */


int reduce(int my_rank, int comm_sz, unsigned int count, float nums[], float *sum) {

    // Calculates given range.
    (*sum) = 0.0f;
    for (int i = 0; i < count; i++) {
        (*sum) += nums[i];
    }

    // step: defines the current step;
    // i: controls the number of iterations;
    // j: receiver's process index;
    // k: whether the operation to be performed is
    //    MPI_Send() (positive) or MPI_Recv() (negative).
    int n_proc = comm_sz;
    for (int finish = 0, step = 0; !finish; step++) {
        div_t res = div(n_proc, 2);
        int n_iter = res.rem == 0 ? n_proc : n_proc - 1;
        n_proc = res.quot + res.rem; // Number of 'active' processes.

        for (int i = 0, j = 0, k = 1; i < n_iter; i++, j = i * pow(2, step)) {
            if (my_rank == j) {
                if (k > 0) { /* Receive */
                    int sender = j + pow(2, step);
                    float his_sum;
                    MPI_Recv(&his_sum, 1, MPI_FLOAT, sender, 2, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                    (*sum) += his_sum;
                    printf("[Step %d] : my_rank %d - Receive from %d - his_sum is %.2f and *sum is %.2f\n", step, my_rank, sender, his_sum, *sum);
                    if (n_iter == 2)
                        finish = 1; // Last message exchange.
                    break;
                } else { /* Send */
                    int receiver = j - pow(2, step);
                    MPI_Send(sum, 1, MPI_FLOAT, receiver, 2, MPI_COMM_WORLD);
                    printf("[Step %d] : my_rank %d - Send to %d - *sum is %.2f\n", step, my_rank, receiver, *sum);
                    finish = 1; // After sending its results, the process returns.
                    break;
                }
            }
            k = -k; // Recv <=> Send.
        }
    }

    return 0;
}


void get_data(int my_rank, int comm_sz, unsigned int *my_count, float **my_nums) {
    if (my_rank == 0) {
        unsigned int count;
        scanf("%u", &count);  // Get numbers count.

        float nums[count]; // Get numbers.
        for (int i = 0; i < count; i++) {
            scanf("%f", &nums[i]);
        }

        div_t res = div(count, comm_sz);
        // #0 computes the excedent if any (from 0 to *my_count - 1).
        *my_count = res.quot + res.rem;
        *my_nums = (float *) calloc(*my_count, sizeof(float));
        for (int i = 0; i < *my_count; i++)
            (*my_nums)[i] = nums[i];

        // Divides array between other processes.
        unsigned int dest_count = res.quot;
        for (int dest = 1, index = *my_count; dest < comm_sz; dest++) {
            float *dest_nums = (float *) calloc(dest_count, sizeof(float));
            for (int i = 0; i < dest_count; i++, index++)
                dest_nums[i] = nums[index];

            MPI_Send(&dest_count, 1, MPI_UNSIGNED, dest, 0, MPI_COMM_WORLD);
            MPI_Send(dest_nums, dest_count, MPI_FLOAT, dest, 1, MPI_COMM_WORLD);
        }
    } else { /* my_rank != 0 */
        MPI_Recv(my_count, 1, MPI_UNSIGNED, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE); 
        *my_nums = (float *) calloc(*my_count, sizeof(float));
        MPI_Recv(*my_nums, *my_count, MPI_FLOAT, 0, 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE); 
    }
}


int main(int argc, char *argv[]) {
    int comm_sz; /* Number of processes */
    int my_rank; /* My process rank */
    unsigned int my_count;
    float *my_nums;

    MPI_Init(NULL, NULL);
    MPI_Comm_size(MPI_COMM_WORLD, &comm_sz);
    MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);

    // Generate random input data.
    if (argc == 4 && strcmp(argv[1], "-gen") == 0) {
        FILE* out = fopen(argv[2], "w");
        generate_floats(out, atoi(argv[3]));
        return 0;
    }


//    // Get input.
    get_data(my_rank, comm_sz, &my_count, &my_nums);

    // Reduce.
    float sum;

    reduce(my_rank, comm_sz, my_count, my_nums, &sum);
    if (my_rank == 0) {
        printf("Sum is: %.2f\n", sum);
    }

    MPI_Finalize();

	return 0;
}

