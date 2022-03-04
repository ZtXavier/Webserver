#ifndef _MUTEX_
#define _MUTEX_

#include<iostream>
#include<exception>
#include<semaphore.h>
#include"server.h"


class lock_mutex
{
public:
        lock_mutex()
        {
            if(pthread_mutex_init(&pmutex,NULL) != 0)
            {
                throw std::exception();
            }
        }
        ~lock_mutex()
        {
            pthread_mutex_destroy(&pmutex);
        }
        bool lock()
        {
            return pthread_mutex_lock(&pmutex);
        }
        bool unlock()
        {
            return pthread_mutex_unlock(&pmutex);
        }
        pthread_mutex_t *get()
        {
            return &pmutex;
        }
private:
        pthread_mutex_t pmutex;
};

class condition
{
public:
        condition()
        {
            if(pthread_cond_init(&cond,NULL) != 0)
            {
                throw std::exception();
            }
        }
        ~condition()
        {
            pthread_cond_destroy(&cond);
        }
        
        bool wait(pthread_mutex_t *mutex)
        {
            int ret;
            if((ret = pthread_cond_wait(&cond,mutex)) == 0)
            {
                return true;
            }
            return false;
        }

        //这里使用linux时间系统调用
        bool timewait(pthread_mutex_t *mutex,struct timespec time)
        {
            int ret;
            if((ret = pthread_cond_timedwait(&cond,mutex,&time)) == 0)
            {
                return true;
            }
            return false;
        }

        bool cond_signal()
        {
            return pthread_cond_signal(&cond) == 0;
        }

        bool broadcast()
        {
            return pthread_cond_broadcast(&cond) == 0;
        }
private:
        pthread_cond_t cond;
};

class sem_signal
{
public:
        sem_signal()
        {
            if(sem_init(&sem_s,0,0) != 0)
            {
                throw std::exception();
            }
        }
        sem_signal(int num)
        {
            if(sem_init(&sem_s,0,num))
            {
                throw std::exception();
            }
        }
        ~sem_signal()
        {
            sem_destroy(&sem_s);
        }
        bool wait()
        {
            return sem_wait(&sem_s) == 0;   //等待信号量
        }
        bool post()
        {
            return sem_post(&sem_s) == 0;   //增加信号量
        }
private:
        sem_t sem_s;
};


#endif