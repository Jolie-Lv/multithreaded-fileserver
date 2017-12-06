#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <getopt.h>
#include <stdio.h>
#include <curl/curl.h>
#include "gfserver.h"
#include "shm_channel.h"

/* note that the -n and -z parameters are NOT used for Part 1 */
/* they are only used for Part 2 */
#define USAGE                                                                         \
"usage:\n"                                                                            \
"  webproxy [options]\n"                                                              \
"options:\n"                                                                          \
"  -h                  Show this help message\n"                                      \
"  -n [segment_count]  Number of segments to use in communication with the cache.\n"  \
"  -p [listen_port]    Listen port (Default: 8086)\n"                                 \
"  -t [thread_count]   Num worker threads (Default: 1, Range: 1-1000)\n"              \
"  -s [server]         The server to connect to (Default: Udacity S3 instance)\n"     \
"  -z [segment_size]   The size (in bytes) of the segments.\n"


/* OPTIONS DESCRIPTOR ====================================================== */
static struct option gLongOptions[] = {
  {"help",          no_argument,            NULL,           'h'},
  {"segment-count", required_argument,      NULL,           'n'},
  {"port",          required_argument,      NULL,           'p'},
  {"thread-count",  required_argument,      NULL,           't'},
  {"server",        required_argument,      NULL,           's'},
  {"segment-size",  required_argument,      NULL,           'z'},
  {NULL,            0,                      NULL,            0}
};

extern ssize_t handle_with_file(gfcontext_t *ctx, char *path, void* arg);
extern ssize_t handle_with_cache(gfcontext_t *ctx, char *path, void* arg);
extern ssize_t handle_with_curl(gfcontext_t *ctx, char *path, void* arg);

static gfserver_t gfs;
static int numsegments = 0;
static int sizesegments = 0;

static void _sig_handler(int signo){
  if (signo == SIGINT || signo == SIGTERM){
    gfserver_stop(&gfs);
    remove_shm_segments(numsegments, sizesegments);
    remove_msg_queue();
    exit(signo);
  }
}

/* Main ========================================================= */
int main(int argc, char **argv) {
  int i;
  int option_char = 0;
  unsigned short port = 8086;
  unsigned short nworkerthreads = 1;
  unsigned int nsegments = 3;
  size_t segsize = 512;
  char *server = "s3.amazonaws.com/content.udacity-data.com";

  /* disable buffering on stdout so it prints immediately */
  setbuf(stdout, NULL);

  if (signal(SIGINT, _sig_handler) == SIG_ERR){
    fprintf(stderr,"Can't catch SIGINT...exiting.\n");
    exit(SERVER_FAILURE);
  }

  if (signal(SIGTERM, _sig_handler) == SIG_ERR){
    fprintf(stderr,"Can't catch SIGTERM...exiting.\n");
    exit(SERVER_FAILURE);
  }

  /* Parse and set command line arguments */
  while ((option_char = getopt_long(argc, argv, "hn:p:s:t:z:", gLongOptions, NULL)) != -1) {
    switch (option_char) {
      case 'h': // help
        fprintf(stdout, "%s", USAGE);
        exit(0);
        break;
      case 'n': // segment count
        nsegments = atoi(optarg);
        break;
      case 'p': // listen-port
        port = atoi(optarg);
        break;
      case 's': // file-path
        server = optarg;
        break;
      case 't': // thread-count
        nworkerthreads = atoi(optarg);
        break;
      case 'z': // segment size
        segsize = atoi(optarg);
        break;
      default:
        fprintf(stderr, "%s", USAGE);
        exit(1);
    }
  }

  if (!server) {
    fprintf(stderr, "Invalid (null) server name\n");
    exit(1);
  }

  if (segsize < 128) {
    fprintf(stderr, "Invalid segment size\n");
    exit(1);
  }

  if (nsegments < 1) {
    fprintf(stderr, "Must have a positive number of segments\n");
    exit(1);
  }
  numsegments = nsegments;
  sizesegments = segsize;

  /* This is where you initialize your shared memory */
  // create all shm segments
  create_shm_segments(nsegments, segsize);
  for(int i = 0; i < nsegments; i++) {
    // get the id of each created segment
    int shm_id = id_for_shm_segment(i, segsize);
    // queue that up on the message queue for the workers later
    queue_shm_segment(shm_id);
  }

  /* This is where you initialize the server struct */
  gfserver_init(&gfs, nworkerthreads);

  /* This is where you set the options for the server */
  gfserver_setopt(&gfs, GFS_PORT, port);
  gfserver_setopt(&gfs, GFS_MAXNPENDING, 12);
  gfserver_setopt(&gfs, GFS_WORKER_FUNC, handle_with_cache);
  for(i = 0; i < nworkerthreads; i++) {
    gfserver_setopt(&gfs, GFS_WORKER_ARG, i, server);
  }
  /* This is where you invoke the framework to run the server */
  /* Note that it loops forever */
  gfserver_serve(&gfs);
}
