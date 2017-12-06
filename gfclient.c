#define  _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include <netinet/in.h>
#include <string.h>
#include <ctype.h>
#include <sys/socket.h>
#include <netdb.h>

#include "gfclient.h"

#define BUFSIZE 8192

struct gfcrequest_t {
  unsigned short portno;
  char* host;
  int socket;
  struct sockaddr_in server_address;

  void (*headerfunc)(void*, size_t, void *);
  void *headerarg;

  void (*writefunc)(void*, size_t, void *);
  void *writearg;

  char* path;
  gfstatus_t status;

  long int file_size;
  long int bytes_received;
};

void gfc_cleanup(gfcrequest_t *gfr){
  free(gfr);
}

gfcrequest_t *gfc_create(){
  struct gfcrequest_t *request = malloc(sizeof( struct gfcrequest_t ));
  memset(&request->server_address, 0, sizeof request->server_address);
  (request->server_address).sin_family=AF_INET;
  request->bytes_received = 0;
  return (gfcrequest_t*)request;
}

size_t gfc_get_bytesreceived(gfcrequest_t *gfr){
    return gfr->bytes_received;
}

size_t gfc_get_filelen(gfcrequest_t *gfr){
    return gfr->file_size;
}

gfstatus_t gfc_get_status(gfcrequest_t *gfr){
    return gfr->status;
}

void gfc_global_init(){

}

void gfc_global_cleanup(){

}

void setStatusAndSize(gfcrequest_t *gfr, gfstatus_t status, int size) {
  gfr->status = status;
  gfr->file_size = size;
}

int parseStatusAndSize(gfcrequest_t *gfr, char* buffer) {
  char *bufferCopy;
  char *temp;
  asprintf(&bufferCopy, "%s", buffer);
  temp = strtok(bufferCopy, " ");
  // first space should be GETFILE
  if (strcmp("GETFILE",temp) != 0) {
    free(bufferCopy);
    setStatusAndSize(gfr, GF_INVALID, -1);
    return -3;
  }
  // second space should be OK
  temp = strtok(NULL, " ");
  if (temp == NULL || strlen(temp) < 2) {
    free(bufferCopy);
    setStatusAndSize(gfr, GF_INVALID, -1);
    return -3;
  }
  if (strcmp("OK", temp) == 0) {

  } else if (strcmp("FILE_NOT_FOUND\r\n\r\n", temp) == 0) {
    setStatusAndSize(gfr, GF_FILE_NOT_FOUND, -1);
    free(bufferCopy);
    return 0;

  } else if (strcmp("ERROR\r\n\r\n", temp) == 0) {
    setStatusAndSize(gfr, GF_ERROR, -1);
    free(bufferCopy);
    return 0;

  } else {
    free(bufferCopy);
    setStatusAndSize(gfr, GF_INVALID, -1);
    return -1;
  }

  // final one is the size.
  // make sure we have token first
  if (strstr(buffer, "\r\n\r\n") == NULL) {
    free(bufferCopy);
    setStatusAndSize(gfr, GF_INVALID, -1);
    return -4;
  }
  temp = strtok(NULL, "\r\n\r\n");
  if (temp == NULL) {
    free(bufferCopy);
    setStatusAndSize(gfr, GF_INVALID, -1);
    return -5;
  }

  int size = atoi(temp);
  if (size == 0) {
    free(bufferCopy);
    setStatusAndSize(gfr, GF_INVALID, -1);
    return -6;
  }

  setStatusAndSize(gfr, GF_OK, size);

  temp = strtok(NULL, "\r\n\r\n");

  if (temp != NULL) {
    int len = strlen(temp) + 1;
    gfr->bytes_received = gfr->bytes_received + len;
    gfr->writefunc(temp,len,gfr->writearg);
  }
  free(bufferCopy);
  return 0;
}

void sendFileRequest(gfcrequest_t *gfr) {
  char *header;
  int size = asprintf(&header, "GETFILE GET %s\r\n\r\n", gfr->path);
  send(gfr->socket, header, size, 0);
  free(header);
}

int recvStatusAndSize(gfcrequest_t *gfr) {
  char buffer[BUFSIZE];
  memset(&buffer, 0, sizeof buffer);
  int offset = 0;
  while(1) {
    int received = recv(gfr->socket, buffer+offset, BUFSIZE-offset, 0);
    if (received <= 0) {
      return parseStatusAndSize(gfr, buffer);
    }
    if (parseStatusAndSize(gfr, buffer) == 0) {
      return 0;
    }
    if (strlen(buffer) > 26) {
      return -7;
    }
    if (offset >= BUFSIZE) {
      return -8;
    }
    offset += received;
  }
}

int recvFileData(gfcrequest_t *gfr) {
  char buffer[BUFSIZE];
  memset(&buffer, 0, sizeof buffer);

  while (1) {
    int received;
    if (gfr->bytes_received == gfr->file_size) {
      return 0;
    }
    received = recv(gfr->socket, buffer, BUFSIZE, 0);
    if (received == 0) {
      if (gfr->file_size > gfr->bytes_received) {
        return -9;
      }
      return 0;
    }
    if (received < 0) {
      return 0;
    }
    gfr->writefunc(buffer,received,gfr->writearg);
    gfr->bytes_received +=received;
    memset(&buffer, 0, sizeof buffer);
  }
  if (gfr->file_size > gfr->bytes_received) {
    return -11;
  }
  return 0;
}

int gfc_perform(gfcrequest_t *gfr){
  gfr->socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  struct timeval timeout;
  timeout.tv_sec = 5.0;
  timeout.tv_usec = 0;
  setsockopt(gfr->socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

  (gfr->server_address).sin_family=AF_INET;
  (gfr->server_address).sin_port=htons(gfr->portno);

  struct hostent *host;
  host = gethostbyname(gfr->host);
  memcpy(&(gfr->server_address).sin_addr, host->h_addr_list[0], host->h_length);

  int status = connect(gfr->socket, (struct sockaddr *) &gfr->server_address, sizeof(gfr->server_address));

  if (status != 0) {
    close(gfr->socket);
    return -15;
  }

  sendFileRequest(gfr);

  int header_status = recvStatusAndSize(gfr);
  if (header_status < 0) {
    close(gfr->socket);
    return header_status;
  }

  if(gfr->status != GF_OK) {
    close(gfr->socket);
    return 0;
  }

  int file_trans_status = recvFileData(gfr);
  if (file_trans_status < 0) {
    close(gfr->socket);
    return file_trans_status;
  }
  close(gfr->socket);
  return 0;
}

void gfc_set_headerarg(gfcrequest_t *gfr, void *headerarg){
  gfr->headerarg = headerarg;
}

void gfc_set_headerfunc(gfcrequest_t *gfr, void (*headerfunc)(void*, size_t, void *)){
  gfr->headerfunc = headerfunc;
}

void gfc_set_path(gfcrequest_t *gfr, char* path){
  gfr->path = path;
}

void gfc_set_port(gfcrequest_t *gfr, unsigned short port){
  gfr->portno = port;
}

void gfc_set_server(gfcrequest_t *gfr, char* server){
  gfr->host = server;
}

void gfc_set_writearg(gfcrequest_t *gfr, void *writearg){
  gfr->writearg = writearg;
}

void gfc_set_writefunc(gfcrequest_t *gfr, void (*writefunc)(void*, size_t, void *)){
  gfr->writefunc = writefunc;
}

char* gfc_strstatus(gfstatus_t status){
    switch(status) {
      case GF_OK: return "OK";
      case GF_FILE_NOT_FOUND: return "FILE_NOT_FOUND";
      case GF_ERROR: return "ERROR";
      case GF_INVALID: return "INVALID";
      default: return "unknown";
    }
}
