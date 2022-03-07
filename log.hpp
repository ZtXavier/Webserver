#ifndef _LOG_
#define _LOG_

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
#include<stdarg.h>
#include"sql_conn.hpp"
#include"block_queue.hpp"
#include"mute.hpp"


class Log
{
public:
    static Log *get_instance()
    {
        static Log instance;
        return &instance;
    }
    
    static void *flush_log_thread(void * arg)
    {
        Log::get_instance()->async_write_log();
    }

    bool init(const char *file_name,int close_log,int log_buf_size=8192,int split_lines = 5000000,int max_queue_size = 0);
    void write_log(int level,const char *format, ...);
    void log_flush();

private:
    Log();
    virtual ~Log();
    void *async_write_log()
    {
        std::string single_log;
        while(m_log_queue->pop(single_log))
        {
            locker.lock();
            fputs(single_log.c_str(),m_fp);
            locker.unlock();
        }
    }

    char dir_name[128];   //路径名
    char log_name[128];  //log文件名
    int   m_split_lines;     //日志最大行数
    int   m_log_buf_size;  //日志缓冲区大小
    long long  m_count;   //日志行数记录
    int m_today;             //按天分类，记录时间是那一天
    FILE *m_fp;              //文件指针
    char *m_buf;            
    block_queue<std::string> *m_log_queue;           //阻塞队列
    bool m_is_async;       //同步异步标志 false为同步，true为异步
    lock_mutex locker;
    int m_close_log;        //关闭日志

};

#define LOG_ERROR(format, ...) if(0 == m_close_log) {Log::get_instance()->write_log(3,format,##__VA_ARGS__);Log::get_instance()->log_flush();}
#define LOG_INFO(format, ...) if(0 == m_close_log) {Log::get_instance()->write_log(1,format,##__VA_ARGS__);Log::get_instance()->log_flush();}
#define LOG_WARN(format, ...) if(0 == m_close_log) {Log::get_instance()->write_log(2,format,##__VA_ARGS__);Log::get_instance()->log_flush();}
#define LOG_DEBUG(format, ...) if(0 == m_close_log) {Log::get_instance()->write_log(0,format,##__VA_ARGS__);Log::get_instance()->log_flush();}

#endif