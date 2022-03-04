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
#include"mute.h"
#include"server.h"

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
        int switch_log;
        std::list<MYSQL*> connlist;


    
};

// 构造函数
mysql_conn::mysql_conn()
{
    have_conn = 0;
    free_conn = 0;
}


// 初始化数据库的链接
void mysql_conn::init(std::string address,int port,std::string user_name,std::string user_passwd,std::string datadb_name,int max_conn,int log)
{
    this->max_conn = max_conn;
    this->my_address = address;
    this->my_port = port;
    this->user_passwd = user_passwd;
    this->user_name = user_name;
    this->datadb_name = datadb_name;
    this->switch_log = log;

    for(int i = 0; i < max_conn;i++)
    {
        MYSQL *conn = NULL;

        if((conn = mysql_init(conn)) == NULL)
        {
            exit(1);
        }

        if((conn = mysql_real_connect(conn,address.c_str(),user_name.c_str(),user_passwd.c_str(),datadb_name.c_str(),port,NULL,0)) == NULL)
        {
            exit(1);
        }
        connlist.push_back(conn);               //如果链接成功则将其放入连接队列中
        ++free_conn;
    }
    max_conn = free_conn;
} 

mysql_conn *mysql_conn::GetInstance()
{
    static mysql_conn conn_pool;
    return &conn_pool;
}


MYSQL *mysql_conn::ret_conn()
{
    MYSQL * connected = NULL;
    if(connlist.size() == 0)
    {
        return NULL;
    } 
    locker.lock();

    connected = connlist.front();
    connlist.pop_front();

    free_conn--;
    have_conn++;

    locker.unlock();
    return connected;
}

int mysql_conn::numoffree()
{
    return this->free_conn;
}

void mysql_conn::destroy_pool()
{
    MYSQL *conn = NULL;
    if(connlist.size() > 0)
    {
        std::list<MYSQL*>::iterator it = connlist.begin();
        for(;it != connlist.end();it++)
        {
            conn = (*it);
            mysql_close(conn);
        }
        have_conn = 0;
        free_conn = 0;
        connlist.clear();         // 释放list类型的链表
    }

}


// 删除链接的句柄
bool mysql_conn::deleteconn(MYSQL *conn)
{
    if(conn != NULL)
    {
        connlist.push_back(conn);
        free_conn++;
        have_conn--;

        return true;
    }
    return false; 
}


mysql_conn::~mysql_conn()
{
    destroy_pool();
}


class mysql_conn_control
{
public:
        mysql_conn_control(MYSQL **sql,mysql_conn *pool);
        ~mysql_conn_control();
private:
        MYSQL *mql;
        mysql_conn * mql_pool;
};


mysql_conn_control::mysql_conn_control(MYSQL **sql,mysql_conn *pool)
{
    mql = *sql;
    mql_pool = pool;
}

mysql_conn_control::~mysql_conn_control()
{
    mql_pool->deleteconn(mql);
}


#endif