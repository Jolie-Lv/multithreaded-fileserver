#define _GNU_SOURCE
#include <unistd.h>
#include "gfserver.h"
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <fcntl.h>
#include <getopt.h>
// extra libs
#include <sys/stat.h>
#include <sys/sendfile.h>
#include <netinet/in.h>

#define  DEFAULT_MAXN_PENDING 5
#define BUFSIZE 8192

struct gfserver_t {
    int max_npending;
    struct sockaddr_in server_address;
    ssize_t (*handler)(gfcontext_t *, char *, void*);
    void* arg;
    int running;
};

struct gfcontext_t {
  int client_socket;
};

/*
 * Modify this file to implement the interface specified in
 * gfserver.h.
 */

void gfs_abort(gfcontext_t *ctx){
  close(ctx->client_socket);
  free(ctx);
}

gfserver_t* gfserver_create(){
    struct gfserver_t *server = malloc (sizeof (struct gfserver_t));
    server->max_npending = DEFAULT_MAXN_PENDING;
    memset(&server->server_address, 0, sizeof(server->server_address));
    (server->server_address).sin_family = AF_INET;
    server->server_address.sin_addr.s_addr = INADDR_ANY;
    server->running = -1;
    return (gfserver_t *)server;
}

ssize_t gfs_send(gfcontext_t *ctx, void *data, size_t len){
    long int offset = 0;
    int left = len;
    while (left > 0) {
      int sent = send(ctx->client_socket, data+offset, left, 0);
      left -= sent;
    }
    return len;
}

ssize_t gfs_sendheader(gfcontext_t *ctx, gfstatus_t status, size_t file_len){
  // "GETFILE OK/FILE_NOT_FOUND/ERROR SIZE \r\n\r\n CONTENTS"
  char *header;
  int size;
  if (status == GF_OK) {
    size = asprintf(&header, "GETFILE OK %li \r\n\r\n", file_len);
  } else if (status == GF_FILE_NOT_FOUND) {
    size = asprintf(&header, "GETFILE FILE_NOT_FOUND \r\n\r\n");
  } else {
    size = asprintf(&header, "GETFILE ERROR \r\n\r\n");
  }
  send(ctx->client_socket, header, size, 0);
  free(header);
  return size;
}

char* getPathFromRequest(char* buffer) {
  char *temp;
  temp = strtok(buffer, " ");
  // first space should be GETFILE
  if (temp == NULL)
  if (strcmp("GETFILE",temp) != 0) {
    return NULL;
  }
  // second space should be GET
  temp = strtok(NULL, " ");
  if (strcmp("GET", temp) != 0) {
    return NULL;
  }
  // final one is the path
  temp = strtok(NULL, "\r\n\r\n");
  if (temp == NULL) {
    return NULL;
  }
  int pathlen = strlen(temp) + 1;
  char *path = (char*)malloc(pathlen * sizeof(char));
  strcpy(path, temp);
  return path;
}

void gfserver_serve(gfserver_t *gfs){
  int server_socket = socket(AF_INET, SOCK_STREAM, 0);
  int opt = 1;
  setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, (const void *)&opt, sizeof(int));
  bind(server_socket, (struct sockaddr *)&gfs->server_address, sizeof(gfs->server_address));
  listen(server_socket, 5);
  gfs->running = 1;
  while (1) {
    int client_socket = accept(server_socket, (struct sockaddr*) NULL, NULL);
    struct gfcontext_t *context = malloc (sizeof (struct gfcontext_t));
    //memset(&context, 0, sizeof context);
    context->client_socket = client_socket;
    char buffer[BUFSIZE];
    memset(&buffer, 0, sizeof buffer);
    recv(client_socket, buffer, BUFSIZE, 0);
    char* path = getPathFromRequest(buffer);
    if (path != NULL) {
      gfs->handler(context, path, gfs->arg);
    } else {
      gfs_sendheader(context, GF_FILE_NOT_FOUND, 0);
    }
    // close(client_socket);
  }
}

void gfserver_set_handlerarg(gfserver_t *gfs, void* arg){
  gfs->arg = arg;
}

void gfserver_set_handler(gfserver_t *gfs, ssize_t (*handler)(gfcontext_t *, char *, void*)){
  gfs->handler = handler;
}

void gfserver_set_maxpending(gfserver_t *gfs, int max_npending){
    gfs->max_npending = max_npending;
}

void gfserver_set_port(gfserver_t *gfs, unsigned short port){
    gfs->server_address.sin_port = htons(port);
}
