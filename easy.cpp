#include<iostream>
#include<stdlib.h>
#include<stdio.h>
#include<sys/socket.h>
#include<arpa/inet.h>
#include<unistd.h>
#include<errno.h>
#include<fcntl.h>
#include<cassert>
#include<sys/epoll.h>

const int Max_fd = 66535;            //最大文件描述符
const int Max_evenums = 10000;  //最大事件数
const int TIMEOUT = 6;               //超时单位

