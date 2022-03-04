#ifndef _BLOCK_QUEUE_
#define _BLOCK_QUEUE_

#include"server.h"
#include"mute.h"
template<class T>
class block_queue
{
public:
        block_queue(int max = 1000)
        {
            if(max_size <= 0)
            {
                exit(-1);
                printf("errno in block");
            }
            max_size = max;
            m_array = new T[max_size];
            m_size = 0;
            m_front = -1;
            m_back = -1;
        }
        ~block_queue() 
        {
            locker.lock();
            if(m_array)
            delete[] m_array;
            locker.unlock();
        }

        void clear()
        {
             locker.lock();
            m_size = 0;
            m_front = 0;
            m_back = 0;
            locker.unlock();
        }
        //判断是否满
        bool full()
        {
            locker.lock();
            if(m_size >= max_size)
            {
                locker.unlock();
                return true;
            }
            locker.unlock();
            return false;
        }
        //判断是否为空
        bool empty()
        {
            locker.lock();
            if(m_size == 0)
            {
                locker.unlock();
                return true;
            }
            locker.unlock();
            return false;
        }

        //返回队首的元素
        bool front(T &value)
        {
            locker.lock();
            if(m_size == 0)
            {
                locker.unlock();
                return false;
            }
            value = m_array[m_front];
            locker.unlock();
            return true;
        }

        //返回队尾的元素
        bool back(T &value)
        {
             locker.lock();
            if(m_size == 0)
            {
                locker.unlock();
                return false;
            }
            value = m_array[m_back];
            locker.unlock();
            return true;
        }

        int size()
        {
            int ret = 0;
            locker.lock();
            ret = m_size;
            locker.unlock();
            return ret;
        }

        int max_size()
        {
            int ret = 0;
            locker.lock();
            ret = max_size;
            locker.unlock();
            return ret;
        }

        //当添加一个元素时需要唤醒在等待的线程
        bool push(const T &item)
        {
            locker.lock();
            if(m_size >= max_size)
            {
                cond.broadcast();
                locker.unlock();
                return false;
            }
            m_back = (m_back+1)%max_size;
            m_array[m_back] = item;
            ++m_size;
            cond.broadcast();
            locker.unlock();
            return true;
        }

        bool pop(const T &item)
        {
            locker.lock();
            while(m_size <= 0)
            {
                if(!cond.wait(locker.get()))       //pthread_cond_wait在之前需要加锁
                {
                    locker.unlock();
                    return false;
                }
            }
            m_front = (m_front + 1) % max_size;
            item = m_array[m_front];
            --m_size;
            locker.unlock();
            return true;
        }

        bool pop(T &item,int ms_timeout)
        {
            struct timespec tm = {0 , 0};
            struct timeval now = {0 , 0};
            gettimeofday(&now,NULL);
            locker.lock();
            if(m_size <= 0)
            {
                tm.tv_sec = now.tv_sec + ms_timeout / 1000;
                tm.tv_usec = (ms_timeout % 1000) * 1000;
                if(!cond.timewait(locker.get(),tm))
                {
                    locker.unlock();
                    return false;
                }
            }
            if(m_size <= 0)
            {
                locker.unlock();
                return false;
            }
            m_front = (m_front + 1) %max_size;
            item = m_array[m_front];
            --m_size;
            return true;
        }
        
private:
        int max_size;
        int m_size;
        int m_front;
        int m_back;
        T * m_array;
        lock_mutex locker;
        condition cond;

};


#endif
