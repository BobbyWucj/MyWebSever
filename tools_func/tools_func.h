#ifndef TOOLS_FUNC_H
#define TOOLS_FUNC_H

#include <unistd.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <signal.h>
#include <string.h>
#include <assert.h>
#include "../config.h"

extern int setNonBlocking(int fd);

extern void addFd(int epollfd, int fd, bool one_shot = false, bool enable_et = true);

extern void removeFd(int epollfd, int fd);

extern void modFd(int epollfd, int fd, int ev);

extern void addSig(int sig, void(*handler)(int), bool restart = true);

#endif // TOOLS_FUNC_H
