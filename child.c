#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <semaphore.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <time.h>
#include <sys/time.h>
#include <errno.h>
#include <ctype.h>


struct shared_vars {
  int segments_num;
  int request_num;
  int segment_requested;
  int completed_requests;
};

void namegen(char * dst, int suffix) {
  sprintf(dst, "/sem_seg%d", suffix);
}

int main(int argc, char ** argv) {

  struct shared_vars * vars;

  vars = (struct shared_vars * ) shmat(atoi(argv[1]), NULL, 0);

  // Check if the attachment was successful
  if (vars == (void * ) - 1) {
    perror("vars shmat");
    exit(EXIT_FAILURE);
  }

  // Access data
  char * data = (char * ) shmat(atoi(argv[2]), NULL, 0);
  if (data == (void * ) - 1) {
    perror("data shmat");
    exit(EXIT_FAILURE);
  }

  // Access readers_count array
  int * readers_count = (int * ) shmat(atoi(argv[3]), NULL, 0);
  if (readers_count == (void * ) - 1) {
    perror("readers_count array shmat");
    exit(EXIT_FAILURE);
  }

  // Access the shared variables
  int segments_num = vars -> segments_num;
  int request_num = vars -> request_num;

  // Access semaphores

  sem_t * readwrite_mutex_sem = sem_open("/my_rw_mutex_sem", 0);
  if (readwrite_mutex_sem == SEM_FAILED) {
    perror("readwrite_mutex_sem_open failed");
    exit(EXIT_FAILURE);
  }

  sem_t * sem_completed_requests = sem_open("/my_sem_completed_requests", 0);
  if (sem_completed_requests == SEM_FAILED) {
    perror("sem_completed_requests_open failed");
    exit(EXIT_FAILURE);
  }

  sem_t * sem1 = sem_open("/my_sem1", 0);
  if (sem1 == SEM_FAILED) {
    perror("sem1 sem_open failed");
    exit(EXIT_FAILURE);
  }

  sem_t * sem2 = sem_open("/my_sem2", 0);
  if (sem2 == SEM_FAILED) {
    perror("sem1 sem_open failed");
    exit(EXIT_FAILURE);
  }

  // // Open the named semaphores in the child process
  sem_t ** semaphores = malloc(segments_num * sizeof(sem_t * ));

  if (semaphores == NULL) {
    printf("Error: Couldn't create array of semaphores");
  } else {
    //creating array of posix semaphores
    for (int i = 0; i < segments_num; i++) {
      char name2[128];
      namegen(name2, i);
      // Semaphore already exists, open it
      semaphores[i] = sem_open(name2, 0);
      if (semaphores[i] == SEM_FAILED) {
        perror("sem_open error after trying to handle EEXIST");
        exit(1);
      }
    }
  }

  // Initialize the random number generator state
  struct random_data buffer;
  char state[64];
  memset(state, 0, sizeof(state));
  initstate_r(getpid(), state, sizeof(state), & buffer);

  // Choose a random integer between 0 and (segments_num-1)
  int n = (segments_num - 1);
  int random_int;
  random_r( & buffer, & random_int);
  // Ensure that random_int and n are positive
  if (random_int < 0) {
    random_int = -random_int;
  }
  if (n < 0) {
    n = -n;
  }
  random_int = random_int % (n + 1);

  printf("Random integer: %d\n", random_int);
  fflush(stdout);

  // get process ID
  pid_t pid = getpid();
  // create filename using process ID
  char * filename;
  asprintf( & filename, "log-%d.txt", pid);

  // Open log file in append mode.
  // This means that if the file already exists, 
  // the new log entries will be added to the end of the file, 
  // without overwriting the existing content. 
  // If the file does not exist, it will be created.

  FILE * log_file = fopen(filename, "a");
  if (log_file == NULL) {
    // handle error
    perror("Error opening log file");
    return 1;
  }
  struct timeval tv;

  for (int j = 0; j < request_num; j++) {

    // This code is executed by the child process

    // ######## USE THE random_int to request the corresponding segment_num and use the corresponding sem_num ##########

    // Choose a random integer between 1 and n_line 
    int n_line = 10;
    int random_int_line;
    random_r( & buffer, & random_int_line);
    random_int_line = abs(random_int_line);
    random_int_line = random_int_line % (n_line + 1);
    if (random_int_line == 0) {
      random_int_line = 1;
    }

    printf("Requested the following <segment,line> : <%d, %d>\n", random_int, random_int_line);

    //if (vars->segment_requested != random_int){
    // Child updates segment_requested variable and signals the parent

    sem_wait(semaphores[random_int]); //children enter the semaphore that corresponds to their requested segment

    readers_count[random_int] = readers_count[random_int] + 1;

    if (readers_count[random_int] == 1) { // if child is the first reader, binds parent to the specific segment and blocks others
      sem_wait(sem1); // 
      vars -> segment_requested = random_int;
      sem_post(sem2); // signals parent
      gettimeofday( & tv, NULL);
      fprintf(log_file, "Time of request: %ld.%06ld \nRequest: < %d, %d >\n", tv.tv_sec, tv.tv_usec, random_int, random_int_line);

      sem_wait(readwrite_mutex_sem); // waits for parent's reply before proceeding
      gettimeofday( & tv, NULL);
      fprintf(log_file, "Time of reply: %ld.%06ld \n", tv.tv_sec, tv.tv_usec);

    }
    sem_post(semaphores[random_int]);
    //}

    //READING CS                
    int line_count = 1; // Counter for the number of lines read
    char line[72]; // Buffer to store the n-th line

    // Read the contents of the shared memory segment one line at a time
    char * line2 = strtok(data, "\n"); // Split the contents of the shared memory segment into lines using the newline character as a delimiter
    while (line2 != NULL) { // Continue reading lines until there are no more
      if (line_count == random_int_line) { // If the n-th line is reached
        printf("Selected line was: %s\n", line2); // Print the line to the screen
        // strcpy(line, line2);  // Save the line to the buffer
        break; // Exit the loop
      }
      // printf("%s\n", line2);  // Print the line to the screen
      line2 = strtok(NULL, "\n"); // Get the next line
      line_count++; // Increment the line counter
    }

    fprintf(log_file, "Line: %s\n", line2);
    // After having received the requested line, sleep for 20 milliseconds
    usleep(20000);

    //END OF READING CS

    sem_wait(semaphores[random_int]);
    readers_count[random_int] = readers_count[random_int] - 1;
    if (readers_count[random_int] == 0) {
      sem_post(sem1);
    }
    sem_post(semaphores[random_int]);

    sem_wait(sem_completed_requests);
    vars -> completed_requests++;
    printf("completed requests are %d \n", vars -> completed_requests);
    sem_post(sem_completed_requests);

    // Choose a new segment num with probability 0.3
    int random_double;
    random_r( & buffer, & random_double);
    if (random_double < 0.3) {
      random_r( & buffer, & random_int);
      random_int = random_int % (n + 1);
    }

    printf("New Random integer: %d\n", random_int);
    fflush(stdout);

  } //end of requests
  fclose(log_file);
  // return 0;  // Exit the child process
  exit(0);
}