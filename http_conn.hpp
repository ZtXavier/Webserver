#ifndef _HTTP_CONN_H_
#define _HTTP_CONN_H_

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
#include"mute.hpp"
#include"log.hpp"
#include"sql_conn.hpp"

class http_conn
{
    public:
            // 文件名最大长度
            static const int FILENAME_LEN = 200;
            // 读缓冲区的大小
            static const int READ_BUFFER_SIZE = 2048;
            // 写缓冲区的大小
            static const int WRITE_BUFFER_SIZE = 1024;
            // GET请求做法
            enum METHOD
            {
                GET = 0,
                POST,
                HEAD,
                PUT,
                DELETE,
                TRACE,
                OPTIONS,
                CONNECT,
                PATH
            };
            // 主状态机
            enum CHECK_STATE
            {
                CHECK_STATE_REQUESTLINE = 0, // 检测状态
                CHECK_STATE_HEADER,               // 头状态
                CHECK_STATE_CONTENT
            };
            // 从状态机
            enum LINE_STATUS
            {
                // 完整行
                LINE_OK = 0,
                // 行出错
                LINE_BAD,
                // 行信息不完整
                LINE_OPEN
            };
            // 结果集
            enum HTTP_CODE
            {
                // 请求不完整,需要继续读取客户数据
                NO_REQUEST,
                // 获得完整的客户请求
                GET_REQUEST,
                // 客户请求有错误语法错误
                BAD_REQUEST,
                // 没有获得数据资源 
                NO_RESOURCE,
                // 没有访问权限
                FORBIDOEN_REQUEST,
                // 请求文件
                FILE_REQUEST,
                // 内部错误
                INTERNAL_ERROR,
                // 客户端已经关闭链接
                CLOSED_CONNECTION
            };

            public:
                    http_conn(){}
                    ~http_conn(){}

                    void init(int sockfd,const sockaddr_in &addr,char *,int ,int,std::string user,std::string passwd,std::string sqlname);
                    void close_conn(bool real_close = true);
                    void process();
                    bool read_once();
                    bool write();
                    // 返回初始化的地址
                    sockaddr_in *get_address()
                    {
                        return &m_addr;
                    }
                    // 初始化数据库
                    void initsql_result(mysql_conn *sql_pool);
                    int timer_flag;
                    int improv;

                    static int m_epollfd;
                    static int m_user_count;
                    MYSQL *sql;
                    int m_state;           // 读写状态 0读 1写






            private:
                    void init();
                    void unmap();
                    bool process_write(HTTP_CODE ret);
                    bool add_response(const char *format, ...);
                    bool add_content(const char *content);
                    bool add_status_line(int status,const char *title);
                    bool add_headers(int content_length);
                    bool add_content_type();
                    bool add_content_length(int content_length);
                    bool add_linger();
                    bool add_blank_line();

                    // 用来分析请求
                    HTTP_CODE process_read();
                    HTTP_CODE parse_request_lines(char *text);
                    HTTP_CODE parse_headers(char *text);
                    HTTP_CODE parse_content(char *text);
                    HTTP_CODE do_request();
                    LINE_STATUS parse_line();
                    char *get_line() { return m_read_buffer + m_start_line;}









                    //客户端地址
                    sockaddr_in m_addr;
                    // http地址
                    int m_sockfd;
                    // 标识读缓冲区中已经读入的客户端数据的最后一个字节的下一个位置
                    int m_read_idx;
                    // 写缓冲区中待发送的字节数
                    int m_write_idx;
                    // 读写缓冲区
                    char m_read_buffer[READ_BUFFER_SIZE];
                    char m_write_buffer[WRITE_BUFFER_SIZE];
                    // 客户请求的目标文件的完整路径，内容为doc_root+m_url
                    char m_name_file[FILENAME_LEN];
                    // 分析的字符在缓冲区的位置
                    int m_checked_idx;     
                    // 开始的行数
                    int m_start_line;     
                     // 包含的长度
                    int m_content_length;    
                    // 客户请求的目标文件名
                    char *m_url;    
                    // http协议版本号,这里只支持http/1.1                
                    char *m_version;     
                    // 主机名        
                    char *m_host;
                    char *m_file_address;

                    char *m_string;             //存储头部数据
                    //网站根目录
                    char *doc_root;
                    //数据库的信息
                    char sql_name[128];
                    char sql_passwd[128];
                    char sql_user[128];
                    //et lt 模式的转变
                    int m_TRIGMode;  

                    //写日志
                    int m_close_log;

                    int cgi;                          
                    int bytes_to_send;
                    int bytes_have_send;
                    bool m_linger;
                    // 状态机
                    CHECK_STATE m_check_state;
                    // 请求方法
                    METHOD m_method;
                    // 通过map来存储用户的信息
                    std::map<std::string,std::string> m_user;
                    // 判断目标文件的状态
                    struct stat m_file_stat;
                    // 定义一个向量结构体，通过writev来向内存中写数据
                    struct iovec m_iv[2];
                    int m_iv_count;
                    
};




#endif