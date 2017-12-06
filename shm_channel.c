#define     _GNU_SOURCE
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <string.h>
#include <semaphore.h>

#include "shm_channel.h"

static int SEGMENT_SIZE = -1;

void create_shm_segments(int number, int size) {
  SEGMENT_SIZE = size;
  for (int i = 0; i < number; i++) {
    key_t key;
    int   shmid;
    key = ftok(".", i);
    if((shmid = shmget(key, size, IPC_CREAT|IPC_EXCL|0666)) == -1) {
      if((shmid = shmget(key, size, 0)) == -1) {
        exit(1);
      }
    }
  }
}

int getSegmentSize() {
  return SEGMENT_SIZE;
}

int id_for_shm_segment(int index, int size) {
  key_t key = ftok(".", index);
  return shmget(key, size, 0);
}

void remove_shm_segments(int number, int size) {
  for (int i = 0; i < number; i++) {
    key_t key;
    int   shmid;
    key = ftok(".", i);
    shmid = shmget(key, size, 0);
    shmctl(shmid, IPC_RMID, 0);
    // printf("removed %d", shmid);
  }
}

int send_message(long type, char *text) {
  key_t key;
  int   mq_id;
  struct mymsgbuf qbuf;

  key = ftok(".", 'm');

  if((mq_id = msgget(key, IPC_CREAT|0660)) == -1) {
    free(text);
    return -1;
  }

  qbuf.mtype = type;
  strcpy(qbuf.mtext, text);

  if((msgsnd(mq_id, &qbuf, strlen(qbuf.mtext)+1, 0)) ==-1) {
    free(text);
    return -1;
  }
  free(text);
  return 1;
}
char* read_message(long type) {
  key_t key;
  int   mq_id;
  struct mymsgbuf qbuf;

  key = ftok(".", 'm');

  if((mq_id = msgget(key, IPC_CREAT|0660)) == -1) {
    return NULL;
  }

  qbuf.mtype = type;
  msgrcv(mq_id, &qbuf, MAXSIZE, type, 0);
  char *returnString = malloc(strlen(qbuf.mtext) + 1);
  strcpy(returnString, qbuf.mtext);
  return returnString;
}

void remove_msg_queue() {
  key_t key;
  int   mq_id;

  key = ftok(".", 'm');
  mq_id = msgget(key, IPC_CREAT|0660);
  msgctl(mq_id, IPC_RMID, 0);
}

void sendCacheRequest(cachereq request) {
  char *reqString;
  asprintf(&reqString, "%d%s%d%s%d%s%s",
                    request.shared_mem_id,
                    SEPARATOR,
                    request.start_byte,
                    SEPARATOR,
                    request.mem_size,
                    SEPARATOR,
                    request.path_request);
  send_message(CACHE_MSG_KEY, reqString);
}

cachereq* recvCacheRequest() {
  char* requestString = read_message(CACHE_MSG_KEY);
  struct cachereq *request = malloc(sizeof(cachereq));

  char* memStr;
  char* startStr;
  char* memSizeStr;
  char* pathStr;

  memStr = strtok(requestString, SEPARATOR);
  startStr = strtok(NULL, SEPARATOR);
  memSizeStr = strtok(NULL, SEPARATOR);
  pathStr = strtok(NULL, SEPARATOR);

  if (memStr == NULL || startStr == NULL || pathStr == NULL || memSizeStr == NULL) {
    free(requestString);
    free(request);
    return NULL;
  }

  int startByte = atoi(startStr);
  int memSize = atoi(memSizeStr);
  if (memSize < 0) {
    free(request);
    free(requestString);
    return NULL;
  }
  if (startByte < 0) {
    free(request);
    free(requestString);
    return NULL;
  }
  int sharedMem = atoi(memStr);
  request->start_byte = startByte;
  request->shared_mem_id = sharedMem;
  request->mem_size = memSize;
  char *path_in_mem;
  asprintf(&path_in_mem, "%s", pathStr);
  request->path_request = path_in_mem;
  free(requestString);
  return request;
}

void queue_shm_segment(int id) {
  char *idString;
  asprintf(&idString, "%d", id);
  send_message(SHM_MSG_KEY, idString);
}

int dequeue_shm_segment() {
  char* msg = read_message(SHM_MSG_KEY);
  int id = atoi(msg);
  free(msg);
  return id;
}
