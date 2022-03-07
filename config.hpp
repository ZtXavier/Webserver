#ifndef _CONFIG_H_
#define _CONFIG_H_

#include"server.hpp"

class server_config
{
    public:
    server_config();
    ~server_config(){};

    void parse_arg(int argc,char* argv[]);
    // 端口号
    int PORT;

    // 日志写入方式
    int WRITE_LOG;

    // 触发组合模式
    int TRIGMode;

    //listenfd触发模式
    int LISTENMode;

    // 优雅关闭链接
    int opt_linger;

    // 数据库链接数量
    int sql_num;

    // 线程池内的线程数量
    int thread_num;

    // 日志开关
    int close_log;

    // 并发模式选择
    int actor_model;

};
#endif