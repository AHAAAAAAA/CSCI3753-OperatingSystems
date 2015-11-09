// CSCI 3753: PA4
// Linux Scheduler Testing
// Ahmed Almutawa
// w/ help of Andy Sayler

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <errno.h>
#include <sched.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>

/* Include Flags */
#define _GNU_SOURCE
#define DEFAULT_ITERATIONS 500000
#define RADIUS (RAND_MAX / 2)
#define MAXFILENAMELENGTH 80
#define DEFAULT_INPUTFILENAME "rwinput"
#define DEFAULT_OUTPUTFILENAMEBASE "/home/user/Desktop/out/rwoutput"
#define DEFAULT_BLOCKSIZE 1024
#define DEFAULT_TRANSFERSIZE 1024*100

inline double dist(double x0, double y0, double x1, double y1){
    return sqrt(pow((x1-x0),2) + pow((y1-y0),2));
}

inline double zeroDist(double x, double y){
    return dist(0, 0, x, y);
}

int main(int argc, char* argv[]){

    pid_t pid, wpid;
    int policy = SCHED_OTHER, num_processes = 10, status, j=0;
    long i, iterations = DEFAULT_ITERATIONS;
    struct sched_param param;
    double x, y;
    double inCircle = 0.0;
    double inSquare = 0.0;
    double pCircle = 0.0;
    double piCalc = 0.0;

    int rv;
    int inputFD;
    int outputFD;
    char inputFilename[MAXFILENAMELENGTH];
    char outputFilename[MAXFILENAMELENGTH];
    char outputFilenameBase[MAXFILENAMELENGTH];

    ssize_t transfersize = DEFAULT_TRANSFERSIZE;
    ssize_t blocksize = DEFAULT_BLOCKSIZE; 
    char* transferBuffer = NULL;
    ssize_t buffersize;

    ssize_t bytesRead = 0;
    ssize_t totalBytesRead = 0;
    int totalReads = 0;
    ssize_t bytesWritten = 0;
    ssize_t totalBytesWritten = 0;
    int totalWrites = 0;
    int inputFileResets = 0;

    /* Set i/o files */
    strncpy(inputFilename, DEFAULT_INPUTFILENAME, MAXFILENAMELENGTH);
    strncpy(outputFilenameBase, DEFAULT_OUTPUTFILENAMEBASE, MAXFILENAMELENGTH);

    /* Set default policy if not supplied */
    if(argc < 2){
        policy = SCHED_OTHER;
    }
    /* Set policy if supplied */
    if(argc > 1){
	if(!strcmp(argv[1], "SCHED_OTHER")){
	    policy = SCHED_OTHER;
	}
	else if(!strcmp(argv[1], "SCHED_FIFO")){
	    policy = SCHED_FIFO;
	}
	else if(!strcmp(argv[1], "SCHED_RR")){
	    policy = SCHED_RR;
	}
	else{
	    fprintf(stderr, "Unhandled scheduling policy\n");
	    exit(EXIT_FAILURE);
	}
    }

    /* Set # Of Processes */
    if(argc > 2){
        if(!strcmp(argv[2], "SMALL")){
            num_processes = 10;
        }
        else if(!strcmp(argv[2], "MEDIUM")){
            num_processes = 100;
        }
        else if(!strcmp(argv[2], "LARGE")){
            num_processes = 1000;
        }
        else{
            fprintf(stderr, "Unhandled number of processes\n");
            exit(EXIT_FAILURE);
        }
    }
    
    /* Set process to max prioty for given scheduler */
    param.sched_priority = sched_get_priority_max(policy);
    
    /* Set new scheduler policy */
    fprintf(stdout, "Current Scheduling Policy: %d\n", sched_getscheduler(0));
    fprintf(stdout, "Setting Scheduling Policy to: %d\n", policy);
    if(sched_setscheduler(0, policy, &param)){
	perror("Error setting scheduler policy");
	exit(EXIT_FAILURE);
    }
    fprintf(stdout, "New Scheduling Policy: %d\n", sched_getscheduler(0));
    if (!strcmp(argv[3], "MIXED")){
        //Iterating over # of processes supplied
        printf("%d processes forked \n", num_processes);
        for(i = 0; i < num_processes; i++){
            if((pid = fork())==-1){
                fprintf(stderr, "Error Forking Child Process");
                exit(EXIT_FAILURE); /*Fork Failed*/
            } 
            if(pid == 0){ /* Child Process */
                //FROM THE PI.C
                /* Calculate pi using statistical methode across all iterations*/
                for(i=0; i<iterations; i++){
                x = (random() % (RADIUS * 2)) - RADIUS;
                y = (random() % (RADIUS * 2)) - RADIUS;
                if(zeroDist(x,y) < RADIUS){
                    inCircle++;
                }
                inSquare++;
                }

                /* Finish calculation */
                pCircle = inCircle/inSquare;
                piCalc = pCircle * 4.0;

                /* Print result */
                fprintf(stdout, "pi = %f\n", piCalc);


                //FROM THE RW.C
                /* Confirm blocksize is multiple of and less than transfersize*/
                if(blocksize > transfersize){
                fprintf(stderr, "blocksize can not exceed transfersize\n");
                exit(EXIT_FAILURE);
                }
                if(transfersize % blocksize){
                fprintf(stderr, "blocksize must be multiple of transfersize\n");
                exit(EXIT_FAILURE);
                }

                /* Allocate buffer space */
                buffersize = blocksize;
                if(!(transferBuffer = malloc(buffersize*sizeof(*transferBuffer)))){
                perror("Failed to allocate transfer buffer");
                exit(EXIT_FAILURE);
                }
                
                /* Open Input File Descriptor in Read Only mode */
                if((inputFD = open(inputFilename, O_RDONLY | O_SYNC)) < 0){
                perror("Failed to open input file");
                exit(EXIT_FAILURE);
                }

                /* Open Output File Descriptor in Write Only mode with standard permissions*/
                rv = snprintf(outputFilename, MAXFILENAMELENGTH, "%s-%d",
                      outputFilenameBase, getpid());    
                if(rv > MAXFILENAMELENGTH){
                fprintf(stderr, "Output filename length exceeds limit of %d characters.\n",
                    MAXFILENAMELENGTH);
                exit(EXIT_FAILURE);
                }
                else if(rv < 0){
                perror("Failed to generate output filename");
                exit(EXIT_FAILURE);
                }
                if((outputFD =
                open(outputFilename,
                     O_WRONLY | O_CREAT | O_TRUNC | O_SYNC,
                     S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH)) < 0){
                perror("Failed to open output file");
                exit(EXIT_FAILURE);
                }

                /* Print Status */
                fprintf(stdout, "Reading from %s and writing to %s\n",
                    inputFilename, outputFilename);

                /* Read from input file and write to output file*/
                do{
                /* Read transfersize bytes from input file*/
                bytesRead = read(inputFD, transferBuffer, buffersize);
                if(bytesRead < 0){
                    perror("Error reading input file");
                    exit(EXIT_FAILURE);
                }
                else{
                    totalBytesRead += bytesRead;
                    totalReads++;
                }
                
                /* If all bytes were read, write to output file*/
                if(bytesRead == blocksize){
                    bytesWritten = write(outputFD, transferBuffer, bytesRead);
                    if(bytesWritten < 0){
                    perror("Error writing output file");
                    exit(EXIT_FAILURE);
                    }
                    else{
                    totalBytesWritten += bytesWritten;
                    totalWrites++;
                    }
                }
                /* Otherwise assume we have reached the end of the input file and reset */
                else{
                    if(lseek(inputFD, 0, SEEK_SET)){
                    perror("Error resetting to beginning of file");
                    exit(EXIT_FAILURE);
                    }
                    inputFileResets++;
                }
                
                }while(totalBytesWritten < transfersize);

                /* Output some possibly helpfull info to make it seem like we were doing stuff */
                fprintf(stdout, "Read:    %zd bytes in %d reads\n",
                    totalBytesRead, totalReads);
                fprintf(stdout, "Written: %zd bytes in %d writes\n",
                    totalBytesWritten, totalWrites);
                fprintf(stdout, "Read input file in %d pass%s\n",
                    (inputFileResets + 1), (inputFileResets ? "es" : ""));
                fprintf(stdout, "Processed %zd bytes in blocks of %zd bytes\n",
                    transfersize, blocksize);
                
                    /* Free Buffer */
                    free(transferBuffer);

                    /* Close Output File Descriptor */
                    if(close(outputFD)){
                    perror("Failed to close output file");
                    exit(EXIT_FAILURE);
                    }

                    /* Close Input File Descriptor */
                    if(close(inputFD)){
                    perror("Failed to close input file");
                    exit(EXIT_FAILURE);
                    }
                    exit(0);
                }
            }
            /* Parent Process Waits For All Of the children to terminate */
            while((wpid = wait(&status)) > 0){
                if(WIFEXITED(status)){ /*Process terminated normally */
                printf("Exit status of child process %d was normal \n", wpid);
                j++;
                }
            }
            printf("Total # of Processes terminated was %d\n", j);
            return EXIT_SUCCESS;
            }
    else if (!strcmp(argv[3], "IO")){
        printf("%d processes forked \n", num_processes);
        for(i = 0; i < num_processes; i++){
            if((pid = fork())==-1){
                fprintf(stderr, "Error Forking Child Process");
                exit(EXIT_FAILURE); /*Fork Failed*/
            } 
            if(pid == 0){ /* Child Process */
                /* Confirm blocksize is multiple of and less than transfersize*/
                if(blocksize > transfersize){
                fprintf(stderr, "blocksize can not exceed transfersize\n");
                exit(EXIT_FAILURE);
                }
                if(transfersize % blocksize){
                fprintf(stderr, "blocksize must be multiple of transfersize\n");
                exit(EXIT_FAILURE);
                }

                /* Allocate buffer space */
                buffersize = blocksize;
                if(!(transferBuffer = malloc(buffersize*sizeof(*transferBuffer)))){
                perror("Failed to allocate transfer buffer");
                exit(EXIT_FAILURE);
                }
                
                /* Open Input File Descriptor in Read Only mode */
                if((inputFD = open(inputFilename, O_RDONLY | O_SYNC)) < 0){
                perror("Failed to open input file");
                exit(EXIT_FAILURE);
                }

                /* Open Output File Descriptor in Write Only mode with standard permissions*/
                rv = snprintf(outputFilename, MAXFILENAMELENGTH, "%s-%d",
                      outputFilenameBase, getpid());    
                if(rv > MAXFILENAMELENGTH){
                fprintf(stderr, "Output filenmae length exceeds limit of %d characters.\n",
                    MAXFILENAMELENGTH);
                exit(EXIT_FAILURE);
                }
                else if(rv < 0){
                perror("Failed to generate output filename");
                exit(EXIT_FAILURE);
                }
                if((outputFD =
                open(outputFilename,
                     O_WRONLY | O_CREAT | O_TRUNC | O_SYNC,
                     S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH)) < 0){
                perror("Failed to open output file");
                exit(EXIT_FAILURE);
                }

                /* Print Status */
                fprintf(stdout, "Reading from %s and writing to %s\n",
                    inputFilename, outputFilename);

                /* Read from input file and write to output file*/
                do{
                /* Read transfersize bytes from input file*/
                bytesRead = read(inputFD, transferBuffer, buffersize);
                if(bytesRead < 0){
                    perror("Error reading input file");
                    exit(EXIT_FAILURE);
                }
                else{
                    totalBytesRead += bytesRead;
                    totalReads++;
                }
                
                /* If all bytes were read, write to output file*/
                if(bytesRead == blocksize){
                    bytesWritten = write(outputFD, transferBuffer, bytesRead);
                    if(bytesWritten < 0){
                    perror("Error writing output file");
                    exit(EXIT_FAILURE);
                    }
                    else{
                    totalBytesWritten += bytesWritten;
                    totalWrites++;
                    }
                }
                /* Otherwise assume we have reached the end of the input file and reset */
                else{
                    if(lseek(inputFD, 0, SEEK_SET)){
                    perror("Error resetting to beginning of file");
                    exit(EXIT_FAILURE);
                    }
                    inputFileResets++;
                }
                
                }while(totalBytesWritten < transfersize);

                /* Output some possibly helpfull info to make it seem like we were doing stuff */
                fprintf(stdout, "Read:    %zd bytes in %d reads\n",
                    totalBytesRead, totalReads);
                fprintf(stdout, "Written: %zd bytes in %d writes\n",
                    totalBytesWritten, totalWrites);
                fprintf(stdout, "Read input file in %d pass%s\n",
                    (inputFileResets + 1), (inputFileResets ? "es" : ""));
                fprintf(stdout, "Processed %zd bytes in blocks of %zd bytes\n",
                    transfersize, blocksize);
                
                /* Free Buffer */
                free(transferBuffer);

                /* Close Output File Descriptor */
                if(close(outputFD)){
                perror("Failed to close output file");
                exit(EXIT_FAILURE);
                }

                /* Close Input File Descriptor */
                if(close(inputFD)){
                perror("Failed to close input file");
                exit(EXIT_FAILURE);
                }

                exit(0);
            }
        }
        /* Parent Process Waits For All Of the children to terminate */
        while((wpid = wait(&status)) > 0){
            if(WIFEXITED(status)){ /*Process terminated normally */
            printf("Exit status of child process %d was normal \n", wpid);
            j++;
            }
        }
        printf("Total # of Processes terminated was %d\n", j);
        return EXIT_SUCCESS;  
    }
    else{
         /* Fork number of processes specified */
        printf("%d processes forked \n", num_processes);
        for(i = 0; i < num_processes; i++){
            if((pid = fork())==-1){
                fprintf(stderr, "Error Forking Child Process");
                exit(EXIT_FAILURE); /*Fork Failed*/
            } 
            if(pid == 0){ /* Child Process */
                /* Calculate pi using statistical methode across all iterations*/
                for(i=0; i<iterations; i++){
                x = (random() % (RADIUS * 2)) - RADIUS;
                y = (random() % (RADIUS * 2)) - RADIUS;
                if(zeroDist(x,y) < RADIUS){
                    inCircle++;
                }
                inSquare++;
                }

                /* Finish calculation */
                pCircle = inCircle/inSquare;
                piCalc = pCircle * 4.0;

                /* Print result */
                fprintf(stdout, "pi = %f\n", piCalc);

                exit(0);
            }
        }
        /* Parent Process Waits For All Of the children to terminate */
        while((wpid = wait(&status)) > 0){
            if(WIFEXITED(status)){ /*Process terminated normally */
            printf("Exit status of child process %d was normal \n", wpid);
            j++;
            }
        }
        printf("Total # of Processes terminated was %d\n", j);
        return EXIT_SUCCESS;
    }
}