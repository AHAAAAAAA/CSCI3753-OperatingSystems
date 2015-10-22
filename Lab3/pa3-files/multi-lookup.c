#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <unistd.h>
#include "util.h"
#include "queue.h"
#include "multi-lookup.h"

void* request(void* id){
	struct thread* thread = id; //Make a thread to hold info
   	char hostname[MAX_NAME_LENGTHS]; //hostname char arrays 
	char *domain;
	bool done = false; //flag
	FILE* inputfp = thread->thread_file; //Input file
	pthread_mutex_t* buffmutex = thread->buffmutex; //Buffer mutex
	queue* buffer = thread->buffer; //Queue

   	while(fscanf(inputfp, INPUTFS, hostname) > 0){ //Read input and push onto buffer
		while(!done){ //Repeat until the current hostname is pushed to the queue
			domain = malloc(SBUFSIZE); //allocate memory for dmain
			strncpy(domain, hostname, SBUFSIZE); //copy hostname to domain, max number of characters
			pthread_mutex_lock(buffmutex);//Lock the buffer, or try
			while(queue_is_full(buffer)){ //When queue is full
					pthread_mutex_unlock(buffmutex); 	//Unlock the buffer, or try
					usleep(rand() % 100 + 5); //Wait 0-100 microseconds
					pthread_mutex_lock(buffmutex); //Lock the buffer, or try
			}
        	queue_push(buffer, domain);
			pthread_mutex_unlock(buffmutex); //Unlock!
			done = true; //Indicate that this hostname was pushed successfully
    	}
		done = false; //Reset pushed for the next hostname
	}
    return NULL;
}

void* resolve(void* id){
	struct thread* thread = id; //Make a thread to hold info
   	char* domain; //domain char arrays 
	FILE* outfile = thread->thread_file; //Output file
	pthread_mutex_t* buffmutex = thread->buffmutex; //Buffer mutex
	pthread_mutex_t* outmutex = thread->outmutex; //Output mutex
	queue* buffer = thread->buffer; //Queue
	char ipstr[MAX_IP_LENGTH]; //IP Addresses
    while(!queue_is_empty(buffer) || requests_exist){ //while the queue has stuff or there's request threads, loop
		pthread_mutex_lock(buffmutex); //lock buffer
		domain = queue_pop(buffer); //pop off queue
		if(domain == NULL){ //if empty, unlock
			pthread_mutex_unlock(buffmutex);
			usleep(rand() % 100 + 5);
		}		
		else { //Unlock and go!
			pthread_mutex_unlock(buffmutex);
			if(dnslookup(domain, ipstr, sizeof(ipstr)) == UTIL_FAILURE)//look up domain, or try
				strncpy(ipstr, "", sizeof(ipstr));
			printf("%s:%s\n", domain, ipstr);
            pthread_mutex_lock(outmutex); //lock output file, if possible
			fprintf(outfile, "%s,%s\n", domain, ipstr); //write to output file
			pthread_mutex_unlock(outmutex); //unlock output, if possible
    	}
			free(domain);
	}
    return NULL;
}

int main(int argc, char * argv[]){
	//Check for correct number of arguments
	//lookup.c
	if(argc<MINARGS){
		fprintf(stderr, "You need at least %d input files to run this program \n",MINARGS);
		fprintf(stderr, "Usage:\n %s %s\n", argv[0], USAGE);
		return EXIT_FAILURE;
	}
	//Local variables (lookup.c)
	queue buffer; //Hostname queue
	int input_files = argc-2; //# of arguments passed
	FILE* outfile   = NULL; //output pointer
	int nCPU        = sysconf( _SC_NPROCESSORS_ONLN );
	FILE* inputfps[input_files]; //Array of inputs
	int resolve_threads;

	//Set amount of threads
	if (nCPU < MAX_RESOLVE_THREADS)
		resolve_threads = nCPU;
	else if (nCPU < MIN_RESOLVE_THREADS)
		resolve_threads = MIN_RESOLVE_THREADS;
	else 
		resolve_threads = MAX_RESOLVE_THREADS;

	//Arrays of threads for requests and resolves
	pthread_t requests[input_files];
	pthread_t resolves[resolve_threads];
	//Arrays of mutexes for queue and output file
	pthread_mutex_t buffmutex;
	pthread_mutex_t outmutex;
	struct thread request_info[input_files];
	struct thread resolve_info[resolve_threads];

	printf("# of requests: %d\n", input_files);
	printf("# of resolves: %d\n\n", resolve_threads);
	if((input_files)>MAX_INPUT_FILES){ //Limit inputs to 10
		fprintf(stderr, "Max number of files allowed is %d\n", MAX_INPUT_FILES);
		fprintf(stderr, "Usage:\n %s %s\n", argv[0], USAGE);
		return EXIT_FAILURE;
	}
	outfile = fopen(argv[argc-1], "w"); //Open output fileor try at least
	if(!outfile){
		perror("Error Opening Output File");
		return EXIT_FAILURE;
	}
	int i;
	for(i=1; i<(argc-1); i++){ 	//Open input files or try at least
		inputfps[i-1] = fopen(argv[i], "r");
	}
	queue_init(&buffer, MAX_INPUT_FILES); //Initialize buffer queue
	//Initialize mutexes
	pthread_mutex_init(&buffmutex, NULL);
	pthread_mutex_init(&outmutex, NULL);
	//Create request pthreads
	for(i=0; i<input_files; i++){
		//Set data for struct to pass to current pthread
		request_info[i].thread_file = inputfps[i];
		request_info[i].buffmutex   = &buffmutex;
		request_info[i].outmutex    = NULL;
		request_info[i].buffer      = &buffer;		
		//pthread_create(pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine) (void *), void *arg); arg==id passed to request
        pthread_create(&(requests[i]), NULL, request, &(request_info[i])); 
		printf("Create request %d\n", i);
    }
	//Create resolve pthreads
    for(i=0; i<resolve_threads; i++){
		resolve_info[i].thread_file = outfile;
		resolve_info[i].buffmutex   = &buffmutex;
		resolve_info[i].outmutex    = &outmutex;
		resolve_info[i].buffer      = &buffer;
		pthread_create(&(resolves[i]), NULL, resolve, &(resolve_info[i]));
		printf("Create resolve %d\n", i);
    }
    //Wait for request threads to complete
    for(i=0; i<input_files; i++){
        pthread_join(requests[i], NULL);
		printf("Requested %d \n", i);
	}
	requests_exist=false;
    //Wait for resolve threads to complete
    for(i=0; i<resolve_threads; i++){
        pthread_join(resolves[i], NULL); // int pthread_join(pthread_t thread, void **retval); joins with a termindated thread
		printf("Resolved %d \n", i);
	}
	queue_cleanup(&buffer); //Clean queue
	fclose(outfile);	//Close output file
	for(i=0; i<input_files; i++){
		fclose(inputfps[i]); //Close input files
	}
	//Destroy the mutexes
	pthread_mutex_destroy(&buffmutex);
	pthread_mutex_destroy(&outmutex);
    return 0;
}