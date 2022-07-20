#include"log.hpp"
#include"server.hpp"
#include<sys/time.h>

Log::Log()
{
    m_count = 0;
    m_is_async = false;
}

Log::~Log ()
{
    if(m_fp != NULL)
    {
        fclose(m_fp);
    }
}

bool Log::init(const char *file_name,int close_log,int log_buf_size,int split_lines,int max_queue)
{
    if(max_queue >= 1)
    {
        m_is_async = true;    //如果队列长度大于等于1,则设置为异步
        m_log_queue = new block_queue<std::string>(max_queue);
        pthread_t tid;
        //异步创建一个线程
        pthread_create(&tid,NULL,flush_log_thread,NULL);
    }
    m_close_log = close_log;
    m_log_buf_size = log_buf_size;
    m_buf = new char[m_log_buf_size];
    bzero(m_buf,log_buf_size);
    m_split_lines = split_lines;

    time_t tm = time(NULL);
    struct tm *sys_tm = localtime(&tm);
    //对于文件名的处理
    const char *p = strrchr(file_name,'/');
    char log_full_name[256] = {0};
    //下面对于log_full_name 的处理
    if(p == NULL)
    {
        snprintf(log_full_name,255,"%dyear %02dmon %02dday %dhour %dmin",sys_tm->tm_year+1900,sys_tm->tm_mon+1,sys_tm->tm_mday,sys_tm->tm_hour,sys_tm->tm_min);
    }
    else
    {
        strcpy(log_name,p+1);
        strncpy(dir_name,file_name,p - file_name + 1);
        snprintf(log_full_name,255,"%s:%dyear %02dmon %02dday %dhour %dmin_%s",dir_name,sys_tm->tm_year+1900,sys_tm->tm_mon+1,sys_tm->tm_mday,sys_tm->tm_hour,sys_tm->tm_min,log_name);
    }

    m_today = sys_tm->tm_mday;
    if((m_fp = fopen(log_full_name,"a")) == NULL)
    {
        // my_error("error",__LINE__);
        return false;
    }
    return true;
}


void Log::write_log(int level,const char *format,...)
{
    struct timeval now{0,0};
    gettimeofday(&now,NULL);
    time_t t = now.tv_sec;
    struct tm *sys_tm = localtime(&t);
    char s[16] = {0};
    switch(level)
    {
        case 0:
                strcpy(s,"[debug]:");
                break;
        case 1:
                strcpy(s,"[info]:");
                break;
        case 2:
                strcpy(s,"[warn]:");
                break;
        case 3:
                strcpy(s,"[erro]");
                break;
        default:
                strcpy(s,"[info]:");
                break;
    }

    locker.lock();
    m_count++;

    if(m_today != sys_tm->tm_mday || m_count % m_split_lines == 0)
    {
        char new_log[256] = {0};
        fflush(m_fp);
        fclose(m_fp);
        char tail[16] = {0};
        //tail 是加上时间
        snprintf(tail,16,"%dyear%02dmon%02day",sys_tm->tm_year+1900,sys_tm->tm_mon+1,sys_tm->tm_mday);

        if(m_today != sys_tm->tm_mday)
        {
            snprintf(new_log, 255,"%s%s%s",dir_name,tail,log_name);
            m_today = sys_tm->tm_mday;
            m_count = 0;
        }
        else
        {
            snprintf(new_log,255,"%s%s%s.%lld",dir_name,tail,log_name,m_count / m_split_lines);
        }
        m_fp  = fopen(new_log,"a");
    }
    locker.unlock();
    va_list list;
    va_start(list,format);

    std::string log_str;
    locker.lock();

    int n = snprintf(m_buf,48,"%d-%02d-%02d-%02d:%02d:%02d.%06ld %s",sys_tm->tm_year+1900,sys_tm->tm_mon+1,sys_tm->tm_mday,sys_tm->tm_hour,sys_tm->tm_min,sys_tm->tm_sec,now.tv_usec,s);
    int m = vsnprintf(m_buf+n,m_log_buf_size-1,format,list);

    m_buf[n + m] = '\n';
    m_buf[n+m+1] = '\0';
    log_str = m_buf;

    locker.unlock();

    if(m_is_async && !m_log_queue->full())
    {
        m_log_queue->push(log_str);
    }
    else
    {
        locker.lock();
        fputs(log_str.c_str(),m_fp);
        locker.unlock();
    }
    va_end(list);
}

void Log::log_flush()
{
    locker.lock();
    fflush(m_fp);
    locker.unlock();
}