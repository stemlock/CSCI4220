#include <mpi.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>

int end_now = 0;
int all_end_now = 0;

void sig_handler(int signo)
{   
    if (signo == SIGUSR1) {        
        end_now = 1;
    }
}

int main(int argc, char **argv)
{
    int id, p, count, global_count, temp_n, pow_n, last_calc, p_size;
    unsigned int n;
    double start_time, elapsed_time;
    
    //Initailize MPI and processes
    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &p);
    MPI_Comm_rank(MPI_COMM_WORLD, &id);
    
    signal(SIGUSR1, sig_handler);

    //set timer
    MPI_Barrier(MPI_COMM_WORLD);
    start_time = MPI_Wtime();
    
    n = 0;
    count = 0;
    last_calc = 0;
    pow_n = 1;

    if (id == 0){
        printf("%12s\t%12s\n", "N","PRIMES");
        fflush(stdout);
    }

    //will loop until all 32 unsigned bit primes are exahusted
    while (pow_n < 10) {

        //assign low and high ends of N bracket
        temp_n = n + 1;
        n = pow(10,pow_n);

        if (pow_n == 9){
            n = 4294967295;
        }

        //create the sieve of prime numbers
        int sqrt_n = sqrt(n);
        unsigned int* sieve = (unsigned int*)calloc(2 + sqrt_n + 1,sizeof(unsigned int));
        int i = 1;
        int j;
        sieve[0] = 2;

        for (j = 3; j <= sqrt_n; j+=2) {
            sieve[i] = j;
            i++;
        }
        
        //calculate block of numbers to test for each process
        p_size = (n-temp_n+1)/p;
        if ((n-temp_n+1)%p > id){
            p_size++;
        }

        int k = ((n-temp_n+1)/p)*id+temp_n;
        int l;

        if (id != 0){
            if ((n-temp_n+1)%p > id){
                k++;
            }
            else{
                k+=((n-temp_n+1)%p); 
            }
        }

        //create array of numbers per process
        unsigned int* block = (unsigned int*)calloc(p_size + 1,sizeof(unsigned int));
        if (block == NULL)    {
            perror("block calloc failed");
            fflush(stdout);
            exit(1);
        } 

        for(l = 0; l < p_size; l++){
            block[l] = k;
            k++;
        }
    
        //Use sieve of eratosthenes to find primes
        int m;
        int q;
        for (m = 0; m < p_size; m++){
            if (m%10 == 0){
                MPI_Allreduce(&end_now, &all_end_now, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);
                if (end_now == 1){
                    break;
                }
            }
            if (block[m] != 0){
                for (q = 0; q < i; q++){
                    if (block[m]%sieve[q] == 0 && block[m]!=sieve[q]){
                        block[m] = 0;
                        break;
                    }    
                }
            }
            if (block[m] > 1){
                count++;
            }
        }

        //if signal was received, print N found
        if(end_now == 1){
            if (id == 0){
                printf("<Signal received>\n");
                elapsed_time = MPI_Wtime() - start_time;
                printf("%12d\t%12d\n", block[m]-1, last_calc + count);
                fflush(stdout);
                printf("Total elapsed time: %10.6fs\n", elapsed_time);
                fflush(stdout);                
            }
            free(sieve);
            free(block);
            break;
        }

        //add all processes' counts
        MPI_Reduce(&count, &global_count, 1, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);
        last_calc += global_count;

        //print N bracket and number of primes found
        if (id == 0){
            printf("%12d\t%12d\n", n, last_calc);
            fflush(stdout);
            elapsed_time = MPI_Wtime() - start_time;
            printf("Total elapsed time: %10.6fs\n", elapsed_time);
            fflush(stdout);
        }   
        free(sieve);
        free(block);

        //move onto next bracket
        pow_n++; 
        count = 0;    
    } 
    
    MPI_Finalize();    
    return 0; 
}
