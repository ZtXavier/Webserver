#include"timer.hpp"
#include"server.hpp"
#include"http_conn.hpp"

sort_timer_lst::sort_timer_lst()
{
    head = NULL;
    tail = NULL;
}

sort_timer_lst::~sort_timer_lst()
{
    util_timer *tmp = head;
    while(tmp)
    {
        head = tmp->next;
        delete tmp;
        tmp = head;
    }
}

void sort_timer_lst::add_timer(util_timer *timer)
{
    if(!timer)
    {
        return;
    }
    if(!head)
    {
        head = tail = timer;
        return;
    }
    //如果该时间小于头节点的时间直接变成头节点
    if(timer->expire < head->expire)
    {
        timer->next = head;
        head->prev = timer;
        head = timer;
        return;
    }
    add_timer(timer,head);
}

// 调整时间链表
void sort_timer_lst::adjust_timer(util_timer *timer)
{
    //如果timer为空则退出
    if(!timer)
    {
        return;
    }
    // 判断timer是否是单个头节点
    util_timer *tmp = timer->next;
    if(!tmp || (timer->expire < tmp->expire))
    {
        return;
    }
    if(timer == head)
    {
        head = head->next;
        head->prev = NULL;
        timer->next = NULL;
        add_timer(head,timer->next);
    }
    else
    {
        timer->prev->next = timer->next;
        timer->next->prev = timer->prev;
        add_timer(timer,timer->next);
    }
}
void sort_timer_lst::del_timer(util_timer *timer)
{
    if(!timer)
    {
        return;
    }
    if((timer == head) && (timer == tail))
    {
        delete timer;
        head = NULL;
        tail = NULL;
        return;
    }
    if(timer == head)
    {
        head = head->next;
        head->prev = NULL;
        delete timer;
        return;
    }
    if(timer == tail)
    {
        tail = tail->prev;
        tail->next = NULL;
        delete timer;
        return;
    }
    timer->prev->next = timer->next;
    timer->next->prev = timer->prev;
    delete timer;
}

void sort_timer_lst::tick()
{
    if(!head)
    {
        return;
    }
    time_t cur = time(NULL);
    util_timer *tmp = head;
    while(tmp)
    {
        if(cur < tmp->expire) //如果当前的时间小于超时时间
        {
            break;
        }
        tmp->cb_func(tmp->user_data);
        head = tmp->next;
        if(head)
        {
            head->prev = NULL;
        }
        delete tmp;
        tmp = head;
    }
}

void sort_timer_lst::add_timer(util_timer *timer,util_timer*lst_head)
{
    util_timer *prev = lst_head;
    util_timer *tmp = prev->next;
    while(tmp)
    {
        if(timer->expire < tmp->expire)
        {
            prev->next = timer;
            timer->next = tmp;
            tmp->prev = timer;
            timer->prev = prev;
            break;
        }
        prev = tmp;
        tmp = tmp->next;
    }
    if(!tmp)
    {
        prev->next = timer;
        timer->prev = prev;
        timer->next = NULL;
        tail = timer;
    }
}

// 初始化超时
void utils::init(int timeslot)
{
    m_TIMESLOT = timeslot;
}


// 设置非阻塞
int utils::setnonblocking(int fd)
{
    int old_option = fcntl(fd,F_GETFL);    //获得fd的属性
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd,F_SETFL,new_option);        // 以new_option来设置文件描述符
    return old_option;
}

// 将内核事件注册读模式，ET模式，开启EPOLLONESHOT
void utils::addfd(int epollfd,int fd,bool one_shot,int TRIGMode)
{
    epoll_event event;
    event.data.fd = fd;
    
    if(1 == TRIGMode)
    {
        event.events = EPOLLIN | EPOLLRDHUP | EPOLLET;
    }
    else
    {
        event.events = EPOLLIN | EPOLLRDHUP;
    }
    if(one_shot)
    {
        event.events |= EPOLLONESHOT;
    }
    epoll_ctl(epollfd,EPOLL_CTL_ADD,fd,&event);
    setnonblocking(fd);
}

// 信号处理函数
void utils::sig_handler(int sig)
{
    int save_errno = errno; //保证可重入性
    int msg = sig;
    // 我们将通过管道与工作线程进行交互
    send(u_pipefd[1],(char *)&msg,1,0); // 通过管道来将信息处理发送给客户端
    errno = save_errno;
}

// 注册函数
void utils::addsig(int sig,void (handler)(int),bool restart)
{
    struct sigaction sa;
    bzero(&sa,sizeof(sa));
    sa.sa_handler = handler;
    if(restart)
    {
        sa.sa_flags |= SA_RESTART;
    }
        sigfillset(&sa.sa_mask);
        assert(sigaction(sig,&sa,NULL) != -1);
}

// 定时处理任务，重新定时以不断触发sigalarm信号
void utils::timer_handler()
{
    m_timer_lst.tick();
    alarm(m_TIMESLOT);
}

// 显示错误
void utils::show_error(int connfd,const char *info)
{
    send(connfd,info,strlen(info),0);
    close(connfd);
}

int *utils::u_pipefd = 0;
int utils::u_epollfd = 0;

class utils;

void cb_func(client_data *user_data)
{
    epoll_ctl(utils::u_epollfd,EPOLL_CTL_DEL,user_data->sockfd,0);
    assert(user_data);
    close(user_data->sockfd);
    http_conn::m_user_count--;
}