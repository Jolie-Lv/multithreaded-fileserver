#include <stdlib.h>
#include <fcntl.h>
#include <curl/curl.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <semaphore.h>
#include "shm_channel.h"

#include "gfserver.h"

//Replace with an implementation of handle_with_cache and any other
//functions you may need.

ssize_t handle_with_cache(gfcontext_t *ctx, char *path, void* arg){
	struct cachereq request;
	request.shared_mem_id = dequeue_shm_segment();
	request.path_request = path;
	request.start_byte = 0;
	request.mem_size = getSegmentSize();

	void* shm_ptr = (void *)shmat(request.shared_mem_id, 0, 0);
	shmdata* shm_data = (shmdata*)shm_ptr;
	sem_init(&shm_data->sem_lock, 1, 0);

	sendCacheRequest(request);
	sem_wait(&shm_data->sem_lock);

	if (shm_data->size <= 0) {
		queue_shm_segment(request.shared_mem_id);
		sem_destroy(&shm_data->sem_lock);
		return gfs_sendheader(ctx, GF_FILE_NOT_FOUND, 0);
	}
	int file_size = shm_data->size + shm_data->size_left;
	gfs_sendheader(ctx, GF_OK, file_size);

	int bytes_transferred = 0;

	while(bytes_transferred < file_size) {
		gfs_send(ctx, &shm_data->data, shm_data->size);
		bytes_transferred += shm_data->size;
		if (shm_data->size_left > 0) {
			struct cachereq next_request;
			next_request.path_request = path;
			next_request.start_byte = bytes_transferred;
			next_request.mem_size = getSegmentSize();
			next_request.shared_mem_id = request.shared_mem_id;
			sendCacheRequest(next_request);
			sem_wait(&shm_data->sem_lock);
		}
	}

	sem_destroy(&shm_data->sem_lock);
	queue_shm_segment(request.shared_mem_id);
	return bytes_transferred;
}
