#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <semaphore.h>

#define MAXSIZE   150
#define SEPARATOR "|||||"
#define CACHE_MSG_KEY 1
#define SHM_MSG_KEY 2

#define SEMAPHORE_ID 128

typedef struct cachereq {
  int shared_mem_id;
  int mem_size;
  int start_byte;
  char* path_request;
} cachereq;

typedef struct mymsgbuf {
  long mtype;
  char mtext[MAXSIZE];
} mymsgbuf;

typedef struct shmdata {
  sem_t sem_lock;
  int size;
  int size_left;
  void* data;
} shmdata;

int send_message(long type, char *text);
char* read_message(long type);
void remove_msg_queue();

void create_shm_segments(int number, int size);
void remove_shm_segments(int number, int size);
int id_for_shm_segment(int index, int size);
int getSegmentSize();

void queue_shm_segment(int id);
int dequeue_shm_segment();

void sendCacheRequest(cachereq request);
cachereq* recvCacheRequest();
