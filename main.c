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
#include <math.h>
#include <unistd.h>
#include <sys/time.h>
#include <mpi.h>


typedef unsigned long long int ulli;


void generate_floats(FILE* out, int count);
int reduce(int my_rank, int comm_sz, unsigned int count, float nums[], float *sum);
void get_data(int my_rank, int comm_sz, unsigned int *my_count, float **my_nums);


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


// TODO: Optimize code!
int reduce(int my_rank, int comm_sz, unsigned int count, float nums[], float *sum) {

    // Calculates given range.
    (*sum) = 0.0f;
    for (int i = 0; i < count; i++) {
        (*sum) += nums[i];
    }

    if (comm_sz == 1)
        return 0;

    int n_proc = comm_sz;
    for (int finish = 0, step = 0; n_proc > 1 && !finish; step++) {
        div_t res = div(n_proc, 2);
        int n_iter = res.rem == 0 ? n_proc : n_proc - 1;
        n_proc = res.quot + res.rem; // Number of 'active' processes.

        // i: controls the number of iterations;
        // j: current process index;
        // op: whether the operation to be performed is
        //    MPI_Send() (negative) or MPI_Recv() (positive).
        for (int i = 0, j = 0, op = 1; i < n_iter; i++, j = i * pow(2, step)) {
            if (my_rank == j) {
                if (op > 0) { /* Receive */
                    int sender = j + pow(2, step);
                    float his_sum;
                    MPI_Recv(&his_sum, 1, MPI_FLOAT, sender, 2, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                    (*sum) += his_sum;
                    printf("[Step %d] : my_rank %d - Receive from %d - his_sum is %.2f and *sum is %.2f\n", step, my_rank, sender, his_sum, *sum);
                    break;
                } else { /* Send */
                    int receiver = j - pow(2, step);
                    MPI_Send(sum, 1, MPI_FLOAT, receiver, 2, MPI_COMM_WORLD);
                    printf("[Step %d] : my_rank %d - Send to %d - *sum is %.2f\n", step, my_rank, receiver, *sum);
                    finish = 1; // After sending its results, the process returns.
                    break;
                }
            }
            op = -op; // Recv <=> Send.
        }
    }

    return 0;
}


// TODO: Improve scathering.
void get_data(int my_rank, int comm_sz, unsigned int *my_count, float **my_nums) {
    if (my_rank == 0) {
        unsigned int count;
        scanf("%u", &count);  // Get numbers count.

        //float nums[count]; // Get numbers.
        // It's better to use heap allocation to prevent stack overflows.
        float *nums = (float *) calloc(count, sizeof(float));
        for (int i = 0; i < count; i++) {
            scanf("%f", &nums[i]);
        }

        div_t res = div(count, comm_sz);
        // #0 computes the excedent if any (from 0 to *my_count - 1).
        *my_count = res.quot + res.rem;
        *my_nums = (float *) calloc(*my_count, sizeof(float));
        // TODO: Use `memcpy` instead of `for`.
        for (int i = 0; i < *my_count; i++)
            (*my_nums)[i] = nums[i];

        // Divides array between other processes.
        unsigned int dest_count = res.quot;
        for (int dest = 1, index = *my_count; dest < comm_sz; dest++) {
            float *dest_nums = (float *) calloc(dest_count, sizeof(float));
            // TODO: Use `memcpy` instead of `for`.
            for (int i = 0; i < dest_count; i++, index++)
                dest_nums[i] = nums[index];

            MPI_Send(&dest_count, 1, MPI_UNSIGNED, dest, 0, MPI_COMM_WORLD);
            MPI_Send(dest_nums, dest_count, MPI_FLOAT, dest, 1, MPI_COMM_WORLD);
        }

        free(nums);
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

    // Get input (scather).
    get_data(my_rank, comm_sz, &my_count, &my_nums);

    // Reduce.
    struct timeval start, end;
    // Start
    gettimeofday(&start, NULL);

    float sum;
    reduce(my_rank, comm_sz, my_count, my_nums, &sum);

    // End
    gettimeofday(&end, NULL);
    // Elapsed time in ms.
    ulli seconds  = end.tv_sec - start.tv_sec;
    ulli useconds = end.tv_usec - start.tv_usec; 
    // '+ 0.5' is a technique for rounding positive values (like ceil() would do).
    ulli elapsed_ms = (ulli) (seconds * 1000 + useconds / 1000.0) + 0.5;

    if (my_rank == 0) {
        ulli max_time = elapsed_ms;
        ulli his_elapsed;
        for (int i = 1; i < comm_sz; i++) {
            MPI_Recv(&his_elapsed, 1, MPI_UNSIGNED_LONG_LONG, i, 3, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            if (his_elapsed > max_time)
                max_time = his_elapsed;
        }
        printf("%.2f %llu\n", sum, max_time);
    } else {
        MPI_Send(&elapsed_ms, 1, MPI_UNSIGNED_LONG_LONG, 0, 3, MPI_COMM_WORLD);
    }

    MPI_Finalize();

	return 0;
}

