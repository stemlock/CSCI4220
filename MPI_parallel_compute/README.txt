mpi_primes.out:
Authors:
Yang Li - liy38
Sam Temlock - temloo

Features:
Uses the OpenMPI library to create parallel computing processes to find the number of primes up to a given N. The Sieve of Eratosthenes algorithm is used to find the number of primes. For each iteration, prime numbers are found up to powers of 10 (i.e. 10^1, 10^2, 10^3, etc). If the signal SIGUSR is received by one of the processes, the program will quit and output the number of primes found up to the last calculated N.

Bugs:
None that we know of