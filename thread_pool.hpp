#ifndef _THREAD_POOL_
#define _THREAD_POOL_
#include<list>
#include<pthread.h>
#include<cstdio>
#include"sql_conn.hpp"
#include"mute.hpp"
#include"http_conn.hpp"
#include"server.hpp"

template<class T>
class thread_pool
{
public:
        thread_pool(int actor_model,mysql_conn *connpool,int thread_number = 8,int max_num = 10000);
        ~thread_pool();
        bool append(T * request,int state);
        bool append_(T * request);

private:
        static void *worker(void *arg);
        void run();

private:
        int thread_number;                // 线程池中的数量
        int max_num;                        // 请求队列中最大值
        int m_actor_model;                // 模型切换
        lock_mutex locker;                 // 定义一个锁对象
        mysql_conn *sql_pool;                   // 定义一个数据库对象
        sem_signal m_statequeue;           // 定义一个信号量，判断是否有任务需要处理
        std::list<T *> work_queue;      // 创建一个队列
        pthread_t *m_thread;            // 定义一个指针，为描述线程的数组

};


template <class T>
thread_pool<T>::thread_pool(int actor_model,mysql_conn *connpool,int thread_number,int max_num)
{
    m_actor_model = actor_model;
    sql_pool = connpool;

    if(thread_number <= 0 || max_num <= 0)
        throw std::exception();
    m_thread = new pthread_t[thread_number];
    if(m_thread == NULL)
        throw std::exception();
    for(int i = 0;i < thread_number;i++)
    {
        if(pthread_create(m_thread+i,NULL,worker,this) != 0)      //这里将this作为指针传给线程参数
        {
            delete[] m_thread;               //创建失败就删除线程池
            throw std::exception();
        }
        if(pthread_detach(m_thread[i]) != 0)
        {
            delete[] m_thread;
            throw std::exception();
        }
    }
}


template <class T>
thread_pool<T>::~thread_pool()
{
    delete[] m_thread;
}

template <class T>
bool thread_pool<T>::append(T *request ,int state)
{
    locker.lock();
    if(work_queue.size() >= max_num)  //如果追加的任务数量超过了最大值，返回false
    {
        locker.unlock();
        return false;
    }
    request->m_state;                                 //将状态赋值
    work_queue.push_back(request);
    locker.unlock();
    m_statequeue.post();                                   //增加一个信号量
}

template <class T>
bool thread_pool<T>::append_(T *request)
{
    locker.lock();
    if(work_queue.size() >= max_num)  //如果追加的任务数量超过了最大值，返回false
    {
        locker.unlock();
        return false;
    }
    work_queue.push_back(request);
    locker.unlock();
    m_statequeue.post();   
}


template <class T>
void * thread_pool<T>::worker(void *arg)
{
    thread_pool *pool = (thread_pool*)arg;
    pool->run();
    return pool;
}


template<class T>
void thread_pool<T>::run ()
{
    while(true)
    {
        m_statequeue.wait();       //等待唤醒
        locker.lock();
        if(work_queue.empty())
        {
            locker.unlock();
            continue;
        }
        T * request = work_queue.front();
        work_queue.pop_front();
        locker.unlock();
        if(!request)
        {
            continue;
        }
        if(1 == m_actor_model)    //
        {
            if(0 == request->m_state)
            {
                if(0 == request->read_once())
                {
                    request->improv = 1;
                    mysql_conn_control mysql_control(&request->sql,sql_pool);
                    request->process();
                }
                else
                {
                    request->improv = 1;
                    request->timer_flag = 1;
                }
            }
            else
            {
                if(request->write())
                {
                    request->improv = 1;
                }
                else
                {
                    request->improv = 1;
                    request->timer_flag = 1;
                }
            }
        }
        else
        {
            mysql_conn_control  mqlcon(&request->sql,sql_pool);
            request->process();
        }
    }
} 

#endif