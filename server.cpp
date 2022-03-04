#include"server.h"

my_Webserver::my_Webserver()
{
    // http_conn类对象
        users = new http_conn[MAXNUM_FD];
    // 根目录路径
        char server_path[256];
        getcwd(server_path,256);
        char root[6] = "/root";
        m_root = (char *)malloc(strlen(server_path) + strlen(root) + 1);
        strcpy(m_root,server_path);
        strcat(m_root,root);

        //定时器
        users_timer = new client_data[MAXNUM_FD];
}

my_Webserver::~my_Webserver()
{
    close(m_epollfd);
    close(m_listenfd);
    close(m_pipefd[0]);
    close(m_pipefd[1]);
    delete [] users;
    delete [] users_timer;
    delete [] m_sql_pool; 
}

void my_Webserver::server_init(int port,std::string user,std::string passwd,std::string dbname,
                                            int log_write,int opt_linger,int trig_mode,int sql_num,
                                            int thread_num,int close_log,int actor_model)
{
    m_port = port;
    m_user = user;
    m_passwd = passwd;
    m_dbname = dbname;
    m_sql_num = sql_num;
    m_thread_num = thread_num;
    m_log_write = log_write;
    m_opt_linger = opt_linger;
    m_TRIGMode = trig_mode;
    m_close_log = close_log;
    m_actor_model = actor_model;
}

void my_Webserver::trig_mode()
{
    // LT + LT
    if(0 == m_TRIGMode)
    {
        m_LISTENMode = 0;
        m_CONNMode = 0;
    }
    // LT + ET
    else if(1 == m_TRIGMode)
    {
        m_LISTENMode = 0;
        m_CONNMode = 1;
    }
    // ET + LT
    else if(2 == m_TRIGMode)
    {
        m_LISTENMode = 1;
        m_CONNMode = 0;
    }
    // ET + ET
    else if(3 == m_TRIGMode)
    {
        m_LISTENMode = 1;
        m_CONNMode = 1;
    }
}

void my_Webserver::log_write()
{
    if(0 == m_close_log)
    {
        if(1 == m_log_write)
        {
            Log::get_instance()->init("./WebserverLog",m_close_log,2000,200000,200);
        }
        else
        {
            Log::get_instance()->init("./WebserverLog",m_close_log,2000,200000,0);
        }
    }
}

void my_Webserver::mysql_pool()
{
    m_sql_pool = mysql_conn::GetInstance();
    m_sql_pool->init("localhost",3306,m_user,m_passwd,m_dbname,m_sql_num,m_close_log);

    // 初始化数据库表
    users->initsql_result(m_sql_pool);
}

void my_Webserver::threads_pool()
{
    m_pool = new thread_pool<http_conn>(m_actor_model,m_sql_pool,m_sql_num);
}

void my_Webserver::eventlisten()
{
    m_listenfd = socket(PF_INET,SOCK_STREAM,0);
    assert(m_listenfd >= 0);

    // 优雅关闭
    if(0 == m_opt_linger)
    {
        struct linger tmp = {0,1};
        setsockopt(m_listenfd,SOL_SOCKET,SO_LINGER,&tmp,sizeof(tmp));
    }
    // close 暂缓关闭
    else if(1 == m_opt_linger)
    {
        struct linger tmp = {1,1};
        setsockopt(m_listenfd,SOL_SOCKET,SO_LINGER,&tmp,sizeof(tmp));
    }

    int ret = 0;
    struct sockaddr_in address;
    bzero(&address,sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(m_port);

    int flag = 1;
    setsockopt(m_listenfd,SOL_SOCKET,SO_REUSEADDR,&flag,sizeof(flag));
    ret = bind(m_listenfd,(struct sockaddr *)&address,sizeof(address));
    assert(ret >= 0);
    ret = listen(m_listenfd,5);
    assert(ret >= 0);

    uts.init(TIMESLOT);
    
    epoll_event events[MAX_EVENT_NUMBER];
    m_epollfd = epoll_create(5);
    assert(m_epollfd != -1);

    uts.addfd(m_epollfd,m_listenfd,false,m_LISTENMode);
    http_conn::m_epollfd = m_epollfd;
    // 双通信模式来实现进程之间的通信
    ret = socketpair(PF_UNIX,SOCK_STREAM,0,m_pipefd);
    assert(ret != -1);
    uts.setnonblocking(m_pipefd[1]);
    uts.addfd(m_epollfd,m_pipefd[0],false,0);

    uts.addsig(SIGPIPE,SIG_IGN);
    uts.addsig(SIGALRM,uts.sig_handler,false);
    uts.addsig(SIGTERM,uts.sig_handler,false);

    alarm(TIMESLOT);

    utils::u_epollfd = m_epollfd;
    utils::u_pipefd = m_pipefd;
} 

// 初始化
void my_Webserver::timer(int connfd,struct sockaddr_in client_address)
{
    users[connfd].init(connfd,client_address,m_root,m_CONNMode,m_close_log,m_user,m_passwd,m_dbname);

    // 初始化client_data数据
    // 创建定时器，设置超时时间，绑定用户数据，将定时器添加到链表中
    users_timer[connfd].address = client_address;
    users_timer[connfd].sockfd = connfd;
    util_timer *timer = new util_timer;
    timer->user_data = &users_timer[connfd];
    timer->cb_func = cb_func;
    time_t cur = time(NULL);
    timer->expire = cur + 3 * TIMESLOT;
    users_timer[connfd].timer = timer;
    uts.m_timer_lst.add_timer(timer);
}

// 若有数据传输，则将定时器延后三个单位
// 并对新的定时器在链表上的位置进行调整
void my_Webserver::adjust_timer(util_timer *timer)
{
    time_t cur = time(NULL);
    timer->expire = cur + 3 * TIMESLOT;
    uts.m_timer_lst.adjust_timer(timer);

    LOG_INFO("%s","adjust timer once");
}

void my_Webserver::deal_timer(util_timer *timer,int sockfd)
{
    timer->cb_func(&users_timer[sockfd]);
    if(timer)
    {
        uts.m_timer_lst.del_timer(timer);
    }
    LOG_INFO("Close fd : %d",users_timer[sockfd].sockfd);
}

// 接受客户端的请求
bool my_Webserver::deal_clientdata()
{
    struct sockaddr_in client_address;
    socklen_t client_addrlength = sizeof(client_address);
    // LT
    if(0 == m_LISTENMode)
    {
        int connfd = accept(m_listenfd,(struct sockaddr *)&client_address,&client_addrlength);
        if(connfd < 0)
        {
            LOG_ERROR("%s:errno is:%d","accept error",errno);
            return false;
        }
        if(http_conn::m_user_count >= MAXNUM_FD)
        {
            uts.show_error(connfd,"WebServer is busy");
            LOG_ERROR("%s","WebServer is busy");
            return false;
        }
        // 初始化保存到timer_lst中
        timer(connfd,client_address);
    }
    // ET模式
    else
    {
        while(1)
        {
            int connfd = accept(m_listenfd,(struct sockaddr *)&client_address,&client_addrlength);
            if(connfd < 0)
            {
                LOG_ERROR("%s:errno is %d","accept error",errno);
                break;
            } 
            if(http_conn::m_user_count >= MAXNUM_FD)
            {
                uts.show_error(connfd,"WebServer is busy");
                LOG_ERROR("%s","WebServer is busy");
                break;
            }
            // 初始化客户端链接
            timer(connfd,client_address);
        }
        return false;
    }
    return true;
}

bool my_Webserver::deal_signal(bool &timeout,bool &stop_server)
{
    int ret = 0;
    int sig;
    char signal_buf[1024];
    bzero(signal_buf,sizeof(signal_buf));
    ret = recv(m_pipefd[0],signal_buf,sizeof(signal_buf),0);
    if(ret == -1)
    {
        return false;
    }
    else if(ret == 0)
    {
        return false;
    }
    else
    {
        for(int i = 0;i < ret;i++)
        {
            switch(signal_buf[i])
            {
                case SIGALRM:
                {
                    timeout = true;
                    break;
                }
                case SIGTERM:
                {
                    stop_server = true;
                    break;
                }
            }
        }
    }
    return true;
}

void my_Webserver::deal_thread(int sockfd)
{
    util_timer *timer = users_timer[sockfd].timer;

    // reactor 模式
    if(1 == m_actor_model)
    {
        if(timer != NULL)
        {
            adjust_timer(timer);
        }
        m_pool->append(users + sockfd,0);

        while(true)
        {
            if(1 == users[sockfd].improv)
            {
                if(1 == users[sockfd].timer_flag)
                {
                    deal_timer(timer,sockfd);
                    users[sockfd].improv = 0;
                    break;
                }
            }
        }
    }
    // proactor 模式
    else
    {
        if(users[sockfd].read_once())
        {
            LOG_INFO("deal with the client(%s)",inet_ntoa(users[sockfd].get_address()->sin_addr));

            // 若检测到读事件，将该事件放入到请求队列
            m_pool->append_(users + sockfd);

            if(timer != NULL)
            {
                adjust_timer(timer);
            }
        }
        else
        {
            deal_timer(timer,sockfd);
        }
    }
}

void my_Webserver::deal_write(int sockfd)
{
    util_timer *timer = users_timer[sockfd].timer;
    // reactor
    if(1 == m_actor_model)
    {
        if(timer != NULL)
        {
            adjust_timer(timer);
        }
        m_pool->append(users + sockfd,1);

        while(true)
        {
            if(1 == users[sockfd].improv)
            {
                if(1 == users[sockfd].timer_flag)
                {
                    deal_timer(timer,sockfd);
                    users[sockfd].timer_flag = 0;
                }
                users[sockfd].improv = 0;
                break;
            }
        }
    }
    else
    {
        //proactor
        if(users[sockfd].write())
        {
            LOG_INFO("send data to client(%s)",inet_ntoa(users[sockfd].get_address()->sin_addr));

            if(timer != NULL)
            {
                adjust_timer(timer);
            }
            else
            {
                deal_timer(timer,sockfd);
            }
        }
    }
}

void my_Webserver::eventloop()
{
    bool timeout = false;
    bool stop_server = false;

    while(!stop_server)
    {
        int number = epoll_wait(m_epollfd,events,MAX_EVENT_NUMBER,-1);
        if(number < 0 && errno != EINTR)
        {
            LOG_ERROR("%s","epoll failure");
            break;
        }
        for(int i = 0;i < number;i++)
        {
            int sockfd = events[i].data.fd;

            //处理新到的客户链接
            if(sockfd == m_listenfd)
            {
                bool flag = deal_clientdata();
                if(false == flag)
                {
                    continue;
                }
            }
            else if(events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR))
            {
                util_timer *timer = users_timer[sockfd].timer;
                deal_timer(timer,sockfd);
            }
            // 处理信号
            else if((sockfd == m_pipefd[0]) && (events[i].events & EPOLLIN))
            {
                bool flag = deal_signal(timeout,stop_server);
                if(false == flag)
                {
                    LOG_ERROR("%s","deal_clientdata failture");
                }
            }
            else if(events[i].events & EPOLLIN)
            {
                deal_read(sockfd);
            }
            else if(events[i].events & EPOLLOUT)
            {
                deal_write(sockfd);
            }
        }
        if(timeout)
        {
            uts.timer_handler();
            LOG_INFO("%s","timer tick");
            timeout = false;
        }
    }
}



