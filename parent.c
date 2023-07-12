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


#define INITIAL_VALUE 1
#define SEM_PERMS(S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP)
#define INITIAL_VALUE 1

#define SHM_SIZE 7000

struct shared_vars {
  int segments_num;
  int request_num;
  int segment_requested;
  int completed_requests;
};

void namegen(char * dst, int suffix) {
  sprintf(dst, "/sem_seg%d", suffix);
}

int main(int argc, char * argv[]) {
  FILE * fp;
  char * filename;
  char ch;
  int partition;
  int request_num;
  int children_num;
  int segments_num;
  int count = 0;
  char c;
  int array_size;
  int shmid;
  char * data;
  int err;
  key_t key;
  key_t key2;
  int total_requests;

  // Check if all necessary arguments have been specified in the command
  if (argc < 5) {
    printf("Missing arguments\n");
    return (1);
  } else {
    filename = argv[1];
    partition = atoi(argv[2]);
    request_num = atoi(argv[3]);
    children_num = atoi(argv[4]);
    printf("Filename : %s\n Partition: %d\n Request_num: %d\n Children_num: %d\n", filename, partition, request_num, children_num);
  }

  total_requests = (children_num * request_num);

  // Open file in read-only mode
  fp = fopen(filename, "r");

  // Check if file exists
  if (fp == NULL) {
    printf("Could not open file %s", filename);
    return 0;
  }

  // Extract characters from file and store in character c
  for (c = getc(fp); c != EOF; c = getc(fp)) {
    if (c == '\n') {
      count = count + 1;
    } // Increment count if this character is newline
  }

  printf("The file has %d lines\n", count);
  fflush(stdout);

  // fclose(fp);

  // fp = fopen(filename,"r");

  segments_num = count / partition;

  printf("The number of segments will be %d\n", segments_num);
  fflush(stdout);

  array_size = segments_num;

  // CREATE SEMAPHORES DYNAMICALLY

  sem_t ** semaphores = malloc(array_size * sizeof(sem_t * ));

  if (semaphores == NULL) {
    printf("Error: Couldn't create array of semaphores");
  } else {
    //creating array of posix semaphores
    for (int i = 0; i < array_size; i++) {
      char name[128];
      namegen(name, i);

      semaphores[i] = sem_open(name, O_CREAT | O_EXCL, SEM_PERMS, INITIAL_VALUE);

      if (semaphores[i] == SEM_FAILED) {
        // An error occurred, check the value of errno
        if (errno == EEXIST) {
          // Semaphore already exists, open it instead of creating it
          semaphores[i] = sem_open(name, 0);
          if (semaphores[i] == SEM_FAILED) {
            perror("sem_open error after trying to handle EEXIST");
            exit(1);
          } else {
            printf("semaphore %d just opened\n", i);
          }
        } else {
          // Some other error occurred
          perror("sem_open semaphores array");
          exit(1);
        }
      }

    }
  }

  // Create sem1 semaphore 

  sem_t * sem1;
  sem1 = sem_open("/my_sem1", O_CREAT | O_EXCL, SEM_PERMS, INITIAL_VALUE);

  if (sem1 == SEM_FAILED) {
    if (errno == EEXIST) {
      // Semaphore already exists, open it instead of creating it
      sem1 = sem_open("/my_sem1", 0);
      if (sem1 == SEM_FAILED) {
        perror("/my_sem1 sem_open error after trying to handle EEXIST");
        exit(1);
      } else {
        printf("/my_sem1 just opened\n");
      }
    } else {
      // Some other error occurred
      perror("/my_sem1 rw_mutex_sem error");
      exit(EXIT_FAILURE);
    }
  }

  // Create sem2 semaphore 

  sem_t * sem2;
  // sem_unlink("/my_rw_mutex_sem");
  sem2 = sem_open("/my_sem2", O_CREAT | O_EXCL, SEM_PERMS, 0);

  if (sem2 == SEM_FAILED) {
    if (errno == EEXIST) {
      // Semaphore already exists, open it instead of creating it
      sem2 = sem_open("/my_sem2", 0);
      if (sem2 == SEM_FAILED) {
        perror("/my_sem2 sem_open error after trying to handle EEXIST");
        exit(1);
      } else {
        printf("/my_sem2 just opened\n");
      }
    } else {
      // Some other error occurred
      perror("/my_sem2 rw_mutex_sem error");
      exit(EXIT_FAILURE);
    }
  }

  // Create rw_mutex_sem

  sem_t * readwrite_mutex_sem;
  // sem_unlink("/my_rw_mutex_sem");
  readwrite_mutex_sem = sem_open("/my_rw_mutex_sem", O_CREAT | O_EXCL, SEM_PERMS, 0);

  if (readwrite_mutex_sem == SEM_FAILED) {
    if (errno == EEXIST) {
      // Semaphore already exists, open it instead of creating it
      readwrite_mutex_sem = sem_open("/my_rw_mutex_sem", 0);
      if (readwrite_mutex_sem == SEM_FAILED) {
        perror("readwrite_mutex_sem sem_open error after trying to handle EEXIST");
        exit(1);
      } else {
        printf("readwrite_mutex_sem just opened\n");
      }
    } else {
      // Some other error occurred
      perror("sem_open rw_mutex_sem error");
      exit(EXIT_FAILURE);
    }
  }

  sem_t * sem_completed_requests;
  // sem_unlink("/my_sem_completed_requests");
  sem_completed_requests = sem_open("/my_sem_completed_requests", O_CREAT | O_EXCL, SEM_PERMS, INITIAL_VALUE);

  if (sem_completed_requests == SEM_FAILED) {
    if (errno == EEXIST) {
      // Semaphore already exists, open it instead of creating it
      sem_completed_requests = sem_open("/my_sem_completed_requests", 0);
      if (sem_completed_requests == SEM_FAILED) {
        perror("/my_sem_completed_requests sem_open error after trying to handle EEXIST");
        exit(1);
      } else {
        printf("/my_sem_completed_requests just opened\n");
      }
    } else {
      // Some other error occurred
      perror("sem_open sem_completed_requests error");
      exit(EXIT_FAILURE);
    }

  }

  //CREATE SHARED MEMORY SEGMENT

  /* make the key for data segment */

  if ((key = ftok("parent.c", 'R')) == -1) {
    perror("ftok");
    exit(1);
  }

  /* connect to and create the segment */

  if ((shmid = shmget(key, SHM_SIZE, 0644 | IPC_CREAT)) == -1) {
    perror("data shmget");
    exit(1);
  }

  /* attach to the segment to get a pointer to it: */
  data = shmat(shmid, NULL, 0); //char *data;
  if (data == (char * ) - 1) { // Check if the shared memory segment was mapped successfuly
    perror("shmat");
    exit(1);
  }
  printf("Shared memory segment with id %d attached at %p\n", shmid, data);
  fflush(stdout);

  // Convert the shared memory identifier to a string
  char shmid_str[16];
  sprintf(shmid_str, "%d", shmid);

  /* make the key for struct */

  if ((key = ftok("parent.c", 'E')) == -1) {
    perror("ftok");
    exit(1);
  }

  // Create a shared memory segment for the struct
  int shmid_struct = shmget(key, sizeof(struct shared_vars), IPC_CREAT | 0666);
  if (shmid_struct < 0) {
    perror("shmid_struct shmget");
    return 1;
  }

  // Attach the shared memory segment to the parent process's address space
  struct shared_vars * vars = (struct shared_vars * ) shmat(shmid_struct, NULL, 0);
  if (vars == (void * ) - 1) {
    perror("shared_vars shmat");
    return 1;
  }

  // Convert the shared memory identifier to a string
  char shmid_struct_str[16];
  sprintf(shmid_struct_str, "%d", shmid_struct);

  // Create array for read_count

  /* make the key for data segment */

  if ((key2 = ftok("shakespeare.txt", 'E')) == -1) {
    fprintf(stderr, "ftok: %s\n", strerror(errno));
    exit(1);
  }

  // Create a shared memory segment for the array
  int shmid_array = shmget(key2, array_size * sizeof(int), IPC_CREAT | 0666);
  if (shmid_array < 0) {
    perror("shmid readers_count_array shmget");
    return 1;
  }

  // Attach the shared memory segment to a dynamic array
  int * readers_count = (int * ) shmat(shmid_array, NULL, 0);
  if (readers_count == (int * ) - 1) {
    perror("readers_count array shmat");
    // Error attaching shared memory segment
    exit(1);
  }

  // Initialize all elements to 0
  for (int i = 0; i < array_size; i++) {
    readers_count[i] = 0;
  }

  // Convert the shared memory identifier to a string
  char shmid_array_str[16];
  sprintf(shmid_array_str, "%d", shmid_array);

  // Initialize the shared variables
  vars -> segments_num = segments_num;
  vars -> request_num = request_num;
  vars -> completed_requests = 0;
  // I haven't initialized vars->segment_requested 

  // CREATE THE CHILD PROCESSES

  // Allocate an array to store the child process IDs
  pid_t * children = malloc(children_num * sizeof(pid_t));

  if (children == NULL) { // Check if the memory was allocated successfully
    printf("Error allocating memory!\n");
    return 1;
  }

  for (int i = 0; i < children_num; i++) {

    children[i] = fork();

    if (children[i] == -1) {
      perror("Error creating child process");
      exit(1);
    }

    if (children[i] == 0) {

      printf("I am child process %d.\n", i + 1);
      fflush(stdout);

      if (execlp("./child", "./child", shmid_struct_str, shmid_str, shmid_array_str, NULL) == -1) {
        perror("Error executing child program");
        exit(1);
      }

    } else if (children[i] > 0) {

      char name3[128];
      namegen(name3, i);

      //This code is executed by the parent process
      while (vars -> completed_requests != total_requests) {
        printf("parent: completed requests are %d and total_requests are %d", vars -> completed_requests, total_requests);
        // Parent reads the segment_requested 
        sem_wait(sem2);

        // printf("what was asked is segment number: %d\n", vars->segment_requested);
        namegen(name3, vars -> segment_requested);
        printf("semaphore required: %s\n", name3);

        // finds and writes the corresponding segment
        if (vars -> segment_requested > 0) {
          fseek(fp, (partition * (vars -> segment_requested - 1)) * 72, SEEK_SET);
        }

        char line[72]; // Buffer to store each line of the file
        int line_count = 0; // Counter for the number of lines read
        while (fgets(line, 72, fp) != NULL && line_count < 100) {
          strcat(data, line); // Concatenate the line to the string in the shared memory segment
          //strcpy(data,line); // write the line to the shared memory segment
          line_count++; //increment the line counter
        }
        sem_post(readwrite_mutex_sem);

        if (vars -> completed_requests == total_requests) {
          break;
        }

      } // end of while 

    } else {

      // An error occurred
      printf("Error calling fork!\n");
    }

  }

  // Wait for the child processes to terminate
  for (int i = 0; i < children_num; i++) {

    int status;
    pid_t pid = waitpid(children[i], & status, 0);
    // usleep(1000000);
    if (pid > 0) {
      printf("Child process %d terminated with status %d.\n", pid, status);
    } else {
      printf("Error waiting for child process!\n");
    }

  }

  //######### REMOVING EVERYTHING ###############

  //unlinking and closing semaphores
  for (int i = 0; i < array_size; i++) {
    char name[128];
    namegen(name, i);
    printf("Closing and unlinking %s\n", name);
    fflush(stdout);

    if (sem_close(semaphores[i]) < 0) {
      perror(name);
    }
    if (sem_unlink(name) < 0) {
      perror(name);
    }
  }

  //closing the rw_mutex semaphore
  if (sem_close(readwrite_mutex_sem) < 0) {
    perror("sem_close rw_mutex_sem failed");
    exit(EXIT_FAILURE);
  }

  if (sem_unlink("/my_rw_mutex_sem") < 0) {
    perror("sem unlik my rw mutex failed");
  }

  //closing the sem_completed_requests
  if (sem_close(sem_completed_requests) < 0) {
    perror("sem_close my_sem_completed_requests failed");
    exit(EXIT_FAILURE);
  }

  if (sem_unlink("/my_sem_completed_requests") < 0) {
    perror("sem_unlink my_sem_completed_requests failed");
  }

  // Closing the sem1
  if (sem_close(sem1) < 0) {
    perror("sem_close sem1 failed");
    exit(EXIT_FAILURE);
  }

  if (sem_unlink("/my_sem1") < 0) {
    perror("sem_unlink my_sem1 failed");
  }

  // Closing the sem2
  if (sem_close(sem2) < 0) {
    perror("sem_close sem2 failed");
    exit(EXIT_FAILURE);
  }

  if (sem_unlink("/my_sem2") < 0) {
    perror("sem_unlink my_sem2 failed");
  }

  /* detach from the segment */

  if (shmdt(data) == -1) {
    perror("shmdt");
    exit(1);
  }

  /* remove segment */
  err = shmctl(shmid, IPC_RMID, 0);
  if (err == -1) {
    perror("Removal");
  } else {
    printf("Shared Memory Segment Removed %d\n", err);
    fflush(stdout);

  }

  // Detach the shared memory segment of vars
  if (shmdt(vars) < 0) {
    perror("shmdt");
    return 1;
  }

  // Remove the vars segment
  if (shmctl(shmid_struct, IPC_RMID, NULL) < 0) {
    perror("shmctl shmid struct");
    return 1;
  }

  // Detach the shared memory segment of array
  if (shmdt(readers_count) < 0) {
    perror("readers_count shmdt");
    return 1;
  }

  // Remove the array segment
  if (shmctl(shmid_array, IPC_RMID, NULL) < 0) {
    perror("shmctl readers_count array");
    return 1;
  }

  fclose(fp);

  free(semaphores);

  // Free the memory allocated for the child process IDs
  free(children);

  return (0);
}