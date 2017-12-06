#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <curl/curl.h>
#include "gfserver.h"

#define BUFSIZE (4096)
static size_t serve_data(void *data, size_t size, size_t nmemb, void *stream) {
	gfcontext_t *context = (gfcontext_t *)stream;
	return gfs_send(context, data, size*nmemb);
}

static size_t process_header(void *ptr, size_t size, size_t nmemb, void *data)
{
  return (size_t)(size * nmemb);
}

ssize_t handle_with_curl(gfcontext_t *ctx, char *path, void* arg){
	// proper URL handling needs to happen here
	char* server = (char*) arg;
	char * full_path = (char *) malloc(1 + strlen(server)+ strlen(path) );
  strcpy(full_path, server);
  strcat(full_path, path);

	CURL *curl_size;
	CURL *curl_data;
	CURLcode res;
	double filesize = 0.0;
	// REQUEST 1: Get only the file size in the header. Do not
	// actually download the file. Leverages the process_header function.
	curl_size = curl_easy_init();
	if(curl_size) {
		curl_easy_setopt(curl_size, CURLOPT_URL, full_path);
    curl_easy_setopt(curl_size, CURLOPT_NOBODY, 1L);
    curl_easy_setopt(curl_size, CURLOPT_HEADERFUNCTION, process_header);
    curl_easy_setopt(curl_size, CURLOPT_HEADER, 0L);
    res = curl_easy_perform(curl_size);

    if(CURLE_OK == res) {
      res = curl_easy_getinfo(curl_size, CURLINFO_CONTENT_LENGTH_DOWNLOAD,
                              &filesize);
			if (filesize > 0.0) {
				gfs_sendheader(ctx, GF_OK, filesize);
			} else {
				gfs_sendheader(ctx, GF_FILE_NOT_FOUND, -1);
				curl_easy_cleanup(curl_size);
				free(full_path);
				return SERVER_FAILURE;
			}
    }
    else {
			// bail out, file not found
      gfs_sendheader(ctx, GF_FILE_NOT_FOUND, 0);
			curl_easy_cleanup(curl_size);
			free(full_path);
			return SERVER_FAILURE;
    }
		curl_easy_cleanup(curl_size);
	} else {
		// bail out, another error encountered
		gfs_sendheader(ctx, GF_ERROR, 0);
		free(full_path);
		return SERVER_FAILURE;
	}

	// REQUEST 2: Ignore header this time and download data.
	// Call gfserver_serve for every chunk of data received
	curl_data = curl_easy_init();
	if(curl_data) {
		curl_easy_setopt(curl_data, CURLOPT_URL, full_path);
	   curl_easy_setopt(curl_data, CURLOPT_WRITEFUNCTION, serve_data);
	   curl_easy_setopt(curl_data, CURLOPT_WRITEDATA, ctx);
	   res = curl_easy_perform(curl_data);
		 if (res == CURLE_OK) {
			 free(full_path);
			 return filesize;
		 }
		 curl_easy_cleanup(curl_data);
	}
	free(full_path);
	return SERVER_FAILURE;
}
