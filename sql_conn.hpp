#ifndef _MYSQL_CONN
#define _MYSQL_CONN
#include<iostream>
#include<list>
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
#include<mysql/mysql.h>
#include<string.h>
#include<pthread.h>
#include"mute.hpp"


class mysql_conn
{
public:
        mysql_conn();
        ~mysql_conn();
        MYSQL *ret_conn();
        int numoffree();
        void destroy_pool();
        void init(std::string my_address,int my_port,std::string user_name,std::string user_passwd,std::string datadb_name,int max_conn,int close_log);
        bool deleteconn(MYSQL *conn);

        std::string my_address;
        std::string user_name;
        std::string user_passwd;
        std::string datadb_name;
        int my_port;
        lock_mutex locker;

        static mysql_conn *GetInstance();


private:
        int max_conn;
        int have_conn;
        int free_conn;
        int m_close_log;
        std::list<MYSQL*> connlist;


    
};

class mysql_conn_control
{
public:
        mysql_conn_control(MYSQL **sql,mysql_conn *pool);
        ~mysql_conn_control();
private:
        MYSQL *mql;
        mysql_conn * mql_pool;
};
#endif




