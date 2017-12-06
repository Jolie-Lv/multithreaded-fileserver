#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <getopt.h>
#include <stdio.h>
#include <pthread.h>
#include "shm_channel.h"
#include "simplecache.h"
#include "steque.h"
#include <semaphore.h>
#include <sys/stat.h>
#include <sys/mman.h>

#if !defined(CACHE_FAILURE)
#define CACHE_FAILURE (-1)
#endif // CACHE_FAILURE

#define MAX_CACHE_REQUEST_LEN 1024

int get_fd_size(int fd);
int min(int a, int b, int c);
void map_to_shared_rec(shmdata *shm_ptr, int shm_size, int fd, int offset);

static void _sig_handler(int signo){
	if (signo == SIGINT || signo == SIGTERM){
		/* Unlink IPC mechanisms here*/
		exit(signo);
	}
}

#define USAGE                                                                 \
"usage:\n"                                                                    \
"  simplecached [options]\n"                                                  \
"options:\n"                                                                  \
"  -c [cachedir]       Path to static files (Default: ./)\n"                  \
"  -h                  Show this help message\n"                              \
"  -t [thread_count]   Num worker threads (Default: 1, Range: 1-1000)\n"

/* OPTIONS DESCRIPTOR ====================================================== */
static struct option gLongOptions[] = {
  {"cachedir",           required_argument,      NULL,           'c'},
  {"help",               no_argument,            NULL,           'h'},
  {"nthreads",           required_argument,      NULL,           't'},
  {NULL,                 0,                      NULL,             0}
};

void Usage() {
  fprintf(stdout, "%s", USAGE);
}

steque_t *request_queue;
pthread_mutex_t queue_mutex;
pthread_cond_t queue_work_available;

void *thread_work(void *arg){
	while(1) {
  	pthread_mutex_lock (&queue_mutex);
		pthread_cond_wait(&queue_work_available, &queue_mutex);
		while (!steque_isempty(request_queue)) {
			struct cachereq *data;
			data = (cachereq*)steque_pop(request_queue);
			if (data == NULL) {
				continue;
			}
			void* shm_ptr = (void *)shmat(data->shared_mem_id, 0, 0);
			shmdata* shm_data = (shmdata*)shm_ptr;

			int fd = simplecache_get(data->path_request);
			if (fd < 0) {
				shm_data->size = -1;
				sem_post(&(shm_data->sem_lock));
				free(data);
				continue;
			}
			shm_data->size = 0;
			map_to_shared_rec(shm_data, data->mem_size - sizeof(shmdata), fd, data->start_byte);

			sem_post(&(shm_data->sem_lock));
			free(data->path_request);
			free(data);
		}
		pthread_mutex_unlock(&queue_mutex);
	}
}
 void map_to_shared_rec(shmdata *shm_ptr, int shm_size, int fd, int offset) {
   if (shm_size <= 0) {
     return;
   }
   int total_file_size = get_fd_size(fd);
   int file_bytes_left = total_file_size - offset;
   if (file_bytes_left <= 0) {
     return;
   }
   int page_size = getpagesize();
   int mmap_offset = (offset / page_size) * page_size;
   int current_page_offset = offset - mmap_offset;
   int bytes_left_in_page = page_size - current_page_offset;

   int size_to_load = min(file_bytes_left , bytes_left_in_page, shm_size );
   void* theData = mmap(0, page_size, PROT_READ, MAP_SHARED, fd, mmap_offset);
	 int temp_size = shm_ptr->size;
	 void* addr = &shm_ptr->data;
	 addr += temp_size;
   memmove(addr, (theData + current_page_offset), size_to_load);
	 shm_ptr->size = temp_size + size_to_load;
	 shm_ptr->size_left = file_bytes_left - size_to_load;
   munmap(theData, page_size);
	 int new_size = shm_size - size_to_load;
	 int new_offset = offset + size_to_load;
   map_to_shared_rec(shm_ptr, new_size, fd, new_offset);
 }

 int min(int a, int b, int c) {
   if (a <= b && a <= c) {
     return a;
   } else if (b <= a && b <= c) {
     return b;
   } else {
     return c;
   }
 }

 int get_fd_size(int fd) {
	 struct stat file_info;
	 memset(&file_info, 0, sizeof file_info);
	 fstat(fd, &file_info);
	 return file_info.st_size;
}

int main(int argc, char **argv) {
	int nthreads = 1;
	char *cachedir = "locals.txt";
	char option_char;

	/* disable buffering to stdout */
	setbuf(stdout, NULL);

	while ((option_char = getopt_long(argc, argv, "c:ht:", gLongOptions, NULL)) != -1) {
		switch (option_char) {
			case 'c': //cache directory
				cachedir = optarg;
				break;
			case 'h': // help
				Usage();
				exit(0);
				break;
			case 't': // thread-count
				nthreads = atoi(optarg);
				break;
			default:
				Usage();
				exit(1);
		}
	}

	if ((nthreads>1024) || (nthreads < 1)) {
		nthreads = 1;
	}

	if (SIG_ERR == signal(SIGINT, _sig_handler)){
		fprintf(stderr,"Unable to catch SIGINT...exiting.\n");
		exit(CACHE_FAILURE);
	}

	if (SIG_ERR == signal(SIGTERM, _sig_handler)){
		fprintf(stderr,"Unable to catch SIGTERM...exiting.\n");
		exit(CACHE_FAILURE);
	}

	/* Cache initialization */
	simplecache_init(cachedir);

	/* Add your cache code here */
	pthread_mutex_init(&queue_mutex, NULL);
	pthread_cond_init (&queue_work_available, NULL);

	// create work queue
	request_queue = malloc (sizeof(steque_t));
	steque_init(request_queue);

	pthread_t threads[nthreads];
	// pthread_t bossMan;
	long t;
	for (t=0; t<nthreads; t++) {
		pthread_create(&threads[t], NULL, thread_work, NULL);
	}

	while (1) {
		cachereq* test = recvCacheRequest();
		pthread_mutex_lock (&queue_mutex);
		steque_enqueue(request_queue, (void*)test);
		pthread_cond_signal(&queue_work_available);
		pthread_mutex_unlock(&queue_mutex);
	}

	pthread_exit(NULL);
	return 0;
}
