#ifndef _SERVER_H_
#define _SERVER_H_

#include<stdio.h>
#include<stdlib.h>
#include<error.h>
#include<mysql/mysql.h>
#include<string.h>
#include<pthread.h>
#include<string.h>
#include<unistd.h>
#include<signal.h>
#include<sys/types.h>
#include<sys/epoll.h>
#include<errno.h>
#include<fcntl.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<assert.h>
#include<sys/stat.h>
#include<sys/mman.h>
#include<stdarg.h>
#include<sys/wait.h>
#include<sys/uio.h>
#include<sys/time.h>
#include<sys/mman.h>
#include<sys/wait.h>
#include<map>
#include<vector>
#include"http_conn.hpp"
#include"timer.hpp"
#include"thread_pool.hpp"
#include"sql_conn.hpp"
#include"log.hpp"


// void my_error(const char * err_string,int line)
// {
//     fprintf(stderr,"line:%d",line);
//     perror(err_string);
//     exit(1);
// }

const int MAXNUM_FD = 65536;                //最大文件描述符
const int MAX_EVENT_NUMBER = 10000;    //最大事件数
const int TIMESLOT = 5;                            // 最小时间单位

class my_Webserver
{
public:
    my_Webserver();
    ~my_Webserver();




    // 初始化服务器参数
    void server_init(int port,std::string user,std::string passwd,std::string dbname,
                          int log_write,int opt_linger,int trigmode,int sql_num,int thread_num,int close_log,
                          int actor_model);

    // 工作模式
    void trig_mode();
    //线程池;
    void threads_pool();      
     //数据库链接池;
    void mysql_pool();   
    //写入日志
    void log_write();
    // 监听可读事件
    void eventlisten();       
    // 事件循环
    void eventloop();
    // 定时器
    void timer(int connfd,struct sockaddr_in client_address);
    // 调整定时器
    void adjust_timer(util_timer *timer);
    // 解决超时
    void deal_timer(util_timer *timer,int sockfd);
    
    bool deal_clientdata();

    bool deal_signal(bool &timeout,bool& stop_server);

    void deal_read(int sockfd);

    void deal_write(int sockfd);

    int m_port;

    char *m_root;

    int m_log_write;

    int m_close_log;

    int m_actor_model;

    int m_pipefd[2];
    int m_epollfd;
    http_conn *users;

    thread_pool<http_conn> *m_pool;
    int m_thread_num;

    epoll_event events[MAX_EVENT_NUMBER];
    int m_listenfd;
    int m_opt_linger;
    int m_TRIGMode;
    // 监听模式
    int m_LISTENMode;
    // 链接模式
    int m_CONNMode;
    // 用户信息
    client_data *users_timer;
    utils uts;

    mysql_conn *m_sql_pool;
    std::string m_user;
    std::string m_passwd;
    std::string m_dbname;
    int m_sql_num;
};




#endif