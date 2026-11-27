#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include "stats.h"

Stats *stats_create(void){
  int fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0600);
  if (fd < 0) { perror("shm_open"); exit(1); }
  if (ftruncate(fd, sizeof(Stats)) < 0) { perror("ftruncate"); exit(1); }

  Stats *s = mmap(NULL, sizeof(Stats), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (s == MAP_FAILED) { perror("mmap"); exit(1); }
  close(fd);

  memset(s, 0, sizeof(Stats));
  pthread_mutexattr_t a;
  pthread_mutexattr_init(&a);
  pthread_mutexattr_setpshared(&a, PTHREAD_PROCESS_SHARED);  /* mutex vale entre processos */
  pthread_mutex_init(&s->lock, &a);
  pthread_mutexattr_destroy(&a);
  return s;
}


void stats_destroy(Stats *s){
  pthread_mutex_destroy(&s->lock);
  munmap(s, sizeof(Stats));
  shm_unlink(SHM_NAME);
}