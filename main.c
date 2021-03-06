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
#include <time.h>
#include <sys/time.h>
#include <mpi.h>


//#define DEBUG_MAIN
//#define DEBUG_SCATHER
//#define DEBUG_REDUCE
//#define DEBUG_REDUCE_SUMTREE


typedef unsigned long long int ulli;


void generate_floats(FILE* out, unsigned int count);
void read_data(unsigned int *count, float **nums);
int reduce(int my_rank, int comm_sz, float num, float *sum);
int reduce_sumtree(int my_rank, int comm_sz, unsigned int count, float nums[],
		float *sum);
void scather(int my_rank, int comm_sz, unsigned int *my_count, float **my_nums);
void scather_intercalate(int my_rank, int comm_sz, unsigned int *my_count, float
		**my_nums);
static void usage(int argc, char *argv[]);


/**
 * Utilitary function to generate a file containing random data in the
 * appropriate format to be processed by the program.
 */
void generate_floats(FILE* out, unsigned int count) {
    srand(time(NULL));
    fprintf(out, "%d\n", count);
    for (unsigned int i = 0; i < count; i++) {
        float r = (float) rand() / (float) (rand() + 1);
        fprintf(out, "%.2f ", r);
    }
    putc('\n', out);
} /* generate_floats */


int reduce(int my_rank, int comm_sz, float num, float *sum) {
    // Initializes `*sum`.
    *sum = num;

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
                    MPI_Recv(&his_sum, 1, MPI_FLOAT, sender, 3, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                    (*sum) += his_sum;
#ifdef DEBUG_REDUCE
                    printf("[Step %d] : my_rank %d - Receive from %d - his_sum is %.2f and *sum is %.2f\n", step, my_rank, sender, his_sum, *sum);
#endif
                    break;
                } else { /* Send */
                    int receiver = j - pow(2, step);
                    MPI_Send(sum, 1, MPI_FLOAT, receiver, 3, MPI_COMM_WORLD);
#ifdef DEBUG_REDUCE
                    printf("[Step %d] : my_rank %d - Send to %d - *sum is %.2f\n", step, my_rank, receiver, *sum);
#endif
                    finish = 1; // After sending its results, the process returns.
                    break;
                }
            }
            op = -op; // Recv <=> Send.
        }
    }

    return 0;
} /* reduce */


int reduce_sumtree(int my_rank, int comm_sz, unsigned int count, float nums[], float *sum) {
    if (count == 1) { // Process has only one element.
        return reduce(my_rank, comm_sz, nums[0], sum);
    } else {
        ldiv_t res = ldiv(count, 2);
        unsigned int qty = res.quot;

        //float my_nums[qty];
        //float his_nums[qty];
        //float res_nums[qty];
        float *my_nums  = (float *) malloc(qty * sizeof(float));
        float *his_nums = (float *) malloc(qty * sizeof(float));
        float *res_nums = (float *) malloc(qty * sizeof(float));

        if (my_rank % 2 == 0) {
            int dst = my_rank + 1;

            // Copy second half of `nums[]` to `my_nums`.
            memcpy(my_nums, nums + qty, qty * sizeof(float));

            // Send first half of `nums[]` to `dst`.
            MPI_Send(nums, qty, MPI_FLOAT, dst, 2, MPI_COMM_WORLD);

            // Receive second half of his `nums[]` into `his_nums`.
            MPI_Recv(his_nums, qty, MPI_FLOAT, dst, 2, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        } else {
            int dst = my_rank - 1;

            // Copy first half of `nums[]` to `my_nums`.
            memcpy(my_nums, nums, qty * sizeof(float));

            // Receive first half of his `nums[]` into `his_nums`.
            MPI_Recv(his_nums, qty, MPI_FLOAT, dst, 2, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

            // Send second half of `nums[]` to `dst`.
            MPI_Send(nums + qty, qty, MPI_FLOAT, dst, 2, MPI_COMM_WORLD);
        }

        // Sums the two vectors together.
#ifdef DEBUG_REDUCE_SUMTREE
       printf("#%d:", my_rank);
#endif
        for (unsigned int j = 0; j < qty; j++) {
           res_nums[j] = my_nums[j] + his_nums[j]; 
#ifdef DEBUG_REDUCE_SUMTREE
           printf(" %.2f", res_nums[j]);
#endif
        }
#ifdef DEBUG_REDUCE_SUMTREE
           printf("\n");
#endif
	free(my_nums);
	free(his_nums);

        return reduce_sumtree(my_rank, comm_sz, qty, res_nums, sum);
    }
} /* reduce_sumtree */


void read_data(unsigned int *count, float **nums) {
	scanf("%u", count);  // Get numbers count.

	// NOTE: It's better to use heap allocation to prevent stack overflows.
	//      Another option would be increasing the stack size, which can be
	//      done globally on Mac OS X with the following command:
	//
	//      ulimit -s hard
	//
	//      or by asking the linker with:
	//
	//      gcc ... -Wl,-stack_size,<stack_size> ...
	//
	//      where `<stack_size>` is a multiple of 4 (e.g.: 0xF0000000).
	//
	// float nums[count];
	(*nums) = (float *) malloc((*count) * sizeof(float));
	for (unsigned int i = 0; i < (*count); i++) { // Get numbers.
		scanf("%f", (*nums) + i); 
	}
}


void scather(int my_rank, int comm_sz, unsigned int *my_count, float **my_nums) {
    if (my_rank == 0) {
		unsigned int count;
		float *nums;

		read_data(&count, &nums);

        if (comm_sz > count) {
            fprintf(stderr, "[ERROR] Number of processes bigger than number of floats.\n");
            MPI_Abort(MPI_COMM_WORLD, -1);
        }

        ldiv_t res = ldiv(count, comm_sz);
        // Process #0 computes the excedent if any (from 0 to *my_count - 1).
        *my_count = res.quot + res.rem;
        *my_nums = (float *) malloc((*my_count) * sizeof(float));
        // NOTE: Uses `memcpy` instead of `for` for efficiency.
        memcpy(*my_nums, nums, (*my_count) * sizeof(float));

        // Divides array between other processes.
        unsigned int dest_count = res.quot;
        for (unsigned int dest = 1, index = *my_count; dest < comm_sz; dest++) {
            float *dest_nums = (float *) malloc(dest_count * sizeof(float));
            // NOTE: Uses `memcpy` instead of `for` for efficiency.
            memcpy(dest_nums, nums + index, dest_count * sizeof(float));
            index += dest_count;

            MPI_Send(&dest_count, 1, MPI_UNSIGNED, dest, 0, MPI_COMM_WORLD);
            MPI_Send(dest_nums, dest_count, MPI_FLOAT, dest, 1, MPI_COMM_WORLD);
        }

        free(nums);
    } else { /* my_rank != 0 */
        MPI_Recv(my_count, 1, MPI_UNSIGNED, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE); 
        *my_nums = (float *) malloc((*my_count) * sizeof(float));
        MPI_Recv(*my_nums, *my_count, MPI_FLOAT, 0, 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE); 
    }
} /* scather */


void scather_intercalate(int my_rank, int comm_sz, unsigned int *my_count, float **my_nums) {
    if (my_rank == 0) {
		unsigned int count;
		float *nums;

		read_data(&count, &nums);

        if (comm_sz > count) {
            fprintf(stderr, "[ERROR] Number of processes bigger than number of floats.\n");
            MPI_Abort(MPI_COMM_WORLD, -1);
        }

        ldiv_t res = ldiv(count, comm_sz);
        // Each process will be assigned at least `least_count` numbers.
        // The remainder will be added to each process below...
        unsigned int least_count = res.quot;

        // Tell the processes how many numbers they're going to receive.
        *my_count = 0 < res.rem ? least_count + 1 : least_count;
#ifdef DEBUG_SCATHER_INTERCALATE
        printf("#0 *my_count is: %u\n", *my_count);
#endif
        for (int rank = 1; rank < comm_sz; rank++) {
            int rank_count = rank < res.rem ? least_count + 1 : least_count;
#ifdef DEBUG_SCATHER_INTERCALATE
            printf("#%d *my_count is: %u\n", rank, rank_count);
#endif
            MPI_Send(&rank_count, 1, MPI_UNSIGNED, rank, 0, MPI_COMM_WORLD);
        }

        // Send the numbers dividing the array between the processes.
        *my_nums = (float *) malloc((*my_count) * sizeof(float)); // Allocates #0 numbers vector.
        unsigned int j = 0; // Keeps next available position of `*my_nums` vector for #0.
        for (unsigned int i = 0; i < count; i++) {
            int dest_rank = ldiv(i, comm_sz).rem; // Which process should receive the number.
            if (dest_rank == 0) {
                (*my_nums)[j++] = nums[i];
            } else {
                MPI_Send(&nums[i], 1, MPI_FLOAT, dest_rank, 1, MPI_COMM_WORLD);
#ifdef DEBUG_SCATHER_INTERCALATE
                printf("Sent to #%d number %.1f\n", dest_rank, nums[i]);
#endif
            }
        }

        free(nums);
    } else { /* my_rank != 0 */
        MPI_Recv(my_count, 1, MPI_UNSIGNED, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE); 
#ifdef DEBUG_SCATHER_INTERCALATE
        printf("\t#%d received *my_count: %u\n", my_rank, *my_count);
#endif
        *my_nums = (float *) malloc((*my_count) * sizeof(float));
        // Receives the numbers assigning them to `*my_nums`.
        for (unsigned int i = 0; i < *my_count; i++) {
            MPI_Recv(&(*my_nums)[i], 1, MPI_FLOAT, 0, 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE); 
#ifdef DEBUG_SCATHER_INTERCALATE
                printf("\t#%d received number %.1f\n", my_rank, (*my_nums)[i]);
#endif
        }
    }
} /* scather_intercalate */


static void usage(int argc, char *argv[]) {
    printf("Generate file with random float numbers:\n");
    printf("%s -gen <filename> <num_count>\n", argv[0]);
    printf("\n");
    printf("Process single file:\n");
    printf("mpiexec -n <nproc> %s < <input>\n", argv[0]);
} /* usage */


int main(int argc, char *argv[]) {
    int comm_sz; /* Number of processes */
    int my_rank; /* My process rank */
    unsigned int my_count;
    float *my_nums;

    MPI_Init(NULL, NULL);
    MPI_Comm_size(MPI_COMM_WORLD, &comm_sz);
    MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);

    // Generate random input data.
    if (argc == 2 && strcmp(argv[1], "--help") == 0) {
        usage(argc, argv);
        return 0;
    } else if (argc == 4 && strcmp(argv[1], "-gen") == 0) {
        FILE* out = fopen(argv[2], "w");
        generate_floats(out, atol(argv[3]));
        return 0;
    }


    // Get input (scather).
    scather(my_rank, comm_sz, &my_count, &my_nums);

#ifdef DEBUG_MAIN
    if (my_rank == 0) {
        printf("#0:");
        for (unsigned int i = 0; i < my_count; i++) {
            printf(" %.1f", my_nums[i]);
        }
        printf("\n");

        float *his_nums;
        unsigned int his_count;
        for (int i = 1; i < comm_sz; i++) {
            MPI_Recv(&his_count, 1, MPI_UNSIGNED, i, 90, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            his_nums = (float *) malloc(his_count * sizeof(float));
            MPI_Recv(his_nums, his_count, MPI_FLOAT, i, 91, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

            printf("#%d:", i);
            for (unsigned int i = 0; i < his_count; i++) {
                printf(" %.1f", his_nums[i]);
            }
            printf("\n");

            free(his_nums);
        }
    } else {
        MPI_Send(&my_count, 1, MPI_UNSIGNED, 0, 90, MPI_COMM_WORLD);
        MPI_Send(my_nums, my_count, MPI_FLOAT, 0, 91, MPI_COMM_WORLD);
    }
#endif

    MPI_Barrier(MPI_COMM_WORLD);

    struct timeval start, end;

    if (my_rank == 0) {
        // Start
        gettimeofday(&start, NULL);
    }

    float sum = 0.0f;

    if (comm_sz == 1 && my_rank == 0) { // Sequential version.
        for (unsigned int i = 0; i < my_count; i++)
            sum += my_nums[i];
    } else {
        reduce_sumtree(my_rank, comm_sz, my_count, my_nums, &sum);
    }

    free(my_nums);

    if (my_rank == 0) {
        // End
        gettimeofday(&end, NULL);
        // Elapsed time in ms.
        ulli seconds  = end.tv_sec - start.tv_sec;
        ulli useconds = end.tv_usec - start.tv_usec; 
        // '+ 0.5' is a technique for rounding positive values (like ceil() would do).
        ulli elapsed_ms = (ulli) (seconds * 1000.0 + useconds / 1000.0 + 0.5);

        printf("%.2f %llu\n", sum, elapsed_ms);
    }

    MPI_Finalize();

    return 0;
} /* main */

