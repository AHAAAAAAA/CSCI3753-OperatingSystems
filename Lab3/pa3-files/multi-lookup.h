#include <pthread.h>

//Froom lookup.c
#define USAGE "<inputFilePath> <outputFilePath>"
#define MINARGS 3
#define SBUFSIZE 1025
#define INPUTFS "%1024s"
#define MAX_INPUT_FILES 10
#define MAX_RESOLVE_THREADS 10
#define MIN_RESOLVE_THREADS 2
#define MAX_NAME_LENGTHS 1025
#define MAX_IP_LENGTH INET6_ADDRSTRLEN
typedef int bool; //for readability
#define true 1
#define false 0
bool requests_exist = true;

//Threads
struct thread{
	queue* buffer;
    FILE* thread_file;
    pthread_mutex_t* buffmutex;
    pthread_mutex_t* outmutex;
};
