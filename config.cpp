#include"server.h"
#include"config.h"

server_config::server_config()
{
    // 端口号
    PORT = 8000;

    // 日志写入方式
    WRITE_LOG = 0;

    // epoll触发方式 LT默认
    TRIGMode = 0;

    // listenfd LT默认
    LISTENMode = 0;

    //优雅关闭链接
    opt_linger = 0;

    // 数据库连接池数量
    sql_num = 0;

    // 线程池线程数量
    thread_num = 0;

    // 日志开关
    close_log = 0;

    //并发模式 proactor
    actor_model = 0;

}

void server_config::parse_arg(int argc,char *argv[])
{
    int option;
    // 这些命令后必须有参数
    const char *ch = "p:w:l:o:s:t:c:a:";
    // optarg 是文件中接受参数的指针
    while((option = getopt(argc,argv,ch)) != -1)
    {
        switch(option)
        {
            case 'p':
            {
                PORT = atoi(optarg);
                break;
            }
            case 'w':
            {
                WRITE_LOG = atoi(optarg);
                break;
            }
            case 'l':
            {
                LISTENMode = atoi(optarg);
                break;
            }
            case 'o':
            {
                opt_linger = atoi(optarg);
                break;
            }
            case 's':
            {
                sql_num = atoi(optarg);
                break;
            }
            case 't':
            {
                thread_num = atoi(optarg);
                break;
            }
            case 'c':
            {
                close_log = atoi(optarg);
                break;
            }
            case 'a':
            {
                actor_model = atoi(optarg);
                break;
            }
            default:
            {
                break;
            }
        }
    }
}