#include<mysql/mysql.h>
#include<fstream>
#include"http_conn.h"
#include "log.h"


// http响应信息

const char *ok_200_title = "OK";
const char *error_400_title = "Bad Request";
const char *error_400_form = "Your request has bad syntax or is inherently impossible to staisfy./n";
const char *error_403_title = "Forbidden";
const char *error_403_form = "You do not have permission to get file form this server.\n";
const char *error_404_title = "Not Found";
const char *error_404_form = "The requested file was not found on this server.\n";
const char *error_500_title = "Internal Error";
const char *error_500_form = "There was an unusual problem serving the request file.\n";

lock_mutex locker;
std::map<std::string,std::string> users;

void http_conn::initsql_result(mysql_conn *sql_pool)
{
    // 获取池中的链接
    MYSQL *sql = NULL;
    mysql_conn_control mysqlcon(&sql,sql_pool);

    // 检索数据库表中username，passwd数据，浏览器输入
    if(mysql_query(sql,"SELECT username,passwd FROM user"))
    {   
        LOG_ERROR("SELECT error :%s\n",mysql_error(sql));
    }

    // 从表中检索完整的结果集
    MYSQL_RES *result = mysql_store_result(sql);

    // 返回结果集中的列数
    int num_fields = mysql_num_fields(result);

    // 返回所有字段结构的数据
    MYSQL_FIELD  *fields = mysql_fetch_fields(result);

    // 通过获取结果集来存user和passwd
    while(MYSQL_ROW row = mysql_fetch_row(result))
    {
        std::string ur(row[0]);
        std::string pd(row[1]);
        users[ur] = pd;
    }
}


// 对文件描述符设置非阻塞
int setnonblocking(int fd)
{
    int old_option = fcntl(fd,F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd,F_SETFL,new_option);
    return old_option;
}

// 注册内核事件ET以及开启epolloneshot
void addfd(int epollfd,int fd,bool one_shot,int TRIGMode)
{
    epoll_event event;
    event.data.fd = fd;
    
    if(1 == TRIGMode)
    {
        event.events = EPOLLET | EPOLLIN | EPOLLRDHUP;
    }
    else
    {
        event.events = EPOLLRDHUP | EPOLLIN;
    }
    if(one_shot)
    {
        event.events |= EPOLLONESHOT;
    }
    epoll_ctl(epollfd,EPOLL_CTL_ADD,fd,&event);
    setnonblocking(fd);
}

// 删除描述符
void removefd(int epollfd,int fd)
{
    epoll_ctl(epollfd,EPOLL_CTL_DEL,fd,0);
    close(fd);
}

// 将事件重置为EPOLLONESHOT
void modfd(int epollfd,int fd,int ev,int TRIGMode)
{
    epoll_event event;
    event.data.fd = fd;
    if(1 == TRIGMode)
    {
        event.events = ev | EPOLLET | EPOLLONESHOT | EPOLLRDHUP;
    } 
    else
    {
        event.events = ev | EPOLLONESHOT | EPOLLRDHUP;
    }
    epoll_ctl(epollfd,EPOLL_CTL_MOD,fd,&event);
}

int http_conn::m_user_count = 0;
int http_conn::m_epollfd = -1;

// 初始化链接
void http_conn::init(int sockfd,const sockaddr_in &addr,char *root,int TRIGMode,int close_log,std::string user,std::string passwd,std::string sqlname)
{
    m_sockfd = sockfd;
    m_addr = addr;
    addfd(m_epollfd,sockfd,true,m_TRIGMode);
    m_user_count++;
    // 浏览器退出或者出现链接重置
    doc_root = root;
    m_TRIGMode = TRIGMode;
    m_close_log = close_log;

    strcpy(sql_user,user.c_str());
    strcpy(sql_passwd,user.c_str());
    strcpy(sql_name,sqlname.c_str());
    init();
}

// 初始默认的参数

void http_conn::init()
{
    sql = NULL;
    bytes_to_send = 0;
    bytes_have_send = 0;
    m_check_state = CHECK_STATE_REQUESTLINE;
    m_linger = false;
    m_method = GET;
    m_url = 0;
    m_version = 0;
    m_content_length = 0;
    m_host = 0;
    m_start_line = 0;
    m_checked_idx = 0;
    m_read_idx = 0;
    m_write_idx = 0;
    cgi = 0;
    m_state = 0;
    timer_flag = 0;
    improv = 0;

    bzero(m_read_buffer,sizeof(m_read_buffer));
    bzero(m_name_file,sizeof(m_name_file));
    bzero(m_write_buffer,sizeof(m_write_buffer));
}

// 关闭链接
void http_conn::close_conn(bool real_close)
{
    if(real_close && (m_sockfd != -1))
    {
        printf("close %d\n",m_sockfd);
        removefd(m_epollfd,m_sockfd);
        m_sockfd = -1;
        m_user_count--;
    }
}

http_conn::LINE_STATUS http_conn::parse_line()
{
    char temp;
    for(;m_checked_idx < m_read_idx;m_checked_idx++)
    {
        temp = m_read_buffer[m_checked_idx];
        if(temp == '\r')
        {
            // 判断是否这一行全部填充满
            if((m_checked_idx + 1) == m_read_idx)
            {
                return LINE_OPEN;
            }
            else if(m_read_buffer[m_checked_idx + 1] == '\n')
            {
                m_read_buffer[m_checked_idx++] = '\0';
                m_read_buffer[m_checked_idx++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
        else if(temp == '\n')
        {
            if(m_checked_idx > 1 && m_read_buffer[m_checked_idx - 1] == '\r')
            {
                m_read_buffer[m_checked_idx - 1] = '\0';
                m_read_buffer[m_checked_idx++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
    }
    return LINE_OPEN;
}

// 循环读取客户数据，直到无数据可读
// 使用ET非阻塞模式

bool http_conn::read_once()
{
    if(m_read_idx >= READ_BUFFER_SIZE)
    {
        return false;
    }
    int bytes_read = 0;
    // LT模式
    if(0 == m_TRIGMode)
    {
        if((bytes_read = recv(m_sockfd,m_read_buffer+m_read_idx,READ_BUFFER_SIZE - m_read_idx,0)) <= 0)
        {
            return false;
        }
        m_read_idx += bytes_read;
        return true;
    }
    // ET模式
    else
    {
        while(true)
        {
            bytes_read = recv(m_sockfd,m_read_buffer+m_read_idx,READ_BUFFER_SIZE - m_read_idx,0);
            if(bytes_read == 0)
            {
                return false;
            }
            else if(bytes_read == -1)
            {
                // 对于非阻塞而言需要判断错误类型，否则将会使程序处于饥渴状态
                if(errno == EAGAIN || errno == EWOULDBLOCK)
                {
                    break;
                    return false;
                }
            }
            m_read_idx += bytes_read;
        }
        return true;
    }
}
    // http解析请求行，获得请求方法，目标为url 和 http版本号
    http_conn::HTTP_CODE http_conn::parse_request_lines(char *text)
    {
        // 返回有相同格式的字符串之后的部分
        m_url = strpbrk(text," \t");
        if( m_url == NULL)
        {
            return BAD_REQUEST;
        }
        *m_url++ = '\0';
        char *method = text;
        if(strcasecmp(method,"GET") == 0)
        {
            m_method = GET;
        }
        else if(strcasecmp(method,"POST") == 0)
        {
            m_method = POST;
            cgi = 1;
        }
        else return BAD_REQUEST;
        // 查找在m_url中连续的tab键的个数
        m_url += strspn(m_url," \t"); 
        m_version = strpbrk(m_url," \t");
        if(m_version == NULL)
        {
            return BAD_REQUEST;
        }
        *m_version++ = '\0';
        m_version += strspn(m_version," \t");
        if(strcasecmp(m_version,"HTTP/1.1") != 0)
        {
            return BAD_REQUEST;
        }
        if(strncasecmp(m_url,"http://",7) == 0)
        {
            m_url += 7;
            m_url = strchr(m_url,'/');
        }
        if(strncasecmp(m_url,"https://",8) == 0)
        {
            m_url += 8;
            m_url = strchr(m_url,'/');
        }
        if(!m_url || m_url[0] != '/')
        {
            return BAD_REQUEST;
        }
        // 当url为/时，显示判断界面
        if(strlen(m_url) == 1)
        {
            strcat(m_url,"judge.html");
        }
        m_check_state = CHECK_STATE_HEADER;
        return NO_REQUEST;
    }

    // 解析http请求的一个头部信息
    http_conn::HTTP_CODE http_conn::parse_headers(char *text)
    {
        if(text[0] == '\0')
        {
            if(m_content_length != 0)
            {
                m_check_state = CHECK_STATE_CONTENT;
                return NO_REQUEST;
            }
            return GET_REQUEST;
        }
        else if(strncasecmp(text,"Connection:",11) == 0)
        {
            text += 11;
            text += strspn(text," \t");
            if(strcasecmp(text,"keep-alive") == 0)
            {
                m_linger = true;
            }
        }
        else if(strncasecmp(text,"Content-length:",15) == 0)
        {
            text += 15;
            text += strspn(text," \t");
            // 将字符串转化为int类型
            m_content_length = atol(text);
        }
        else if(strncasecmp(text,"Host:",5) == 0)
        {
            text += 5;
            text += strspn(text," \t");
            m_host = text;
        }
        else
        {
            LOG_INFO("UNKNOW HEADER: %s",text);
        }
            return NO_REQUEST;
 }


 // 判断http是否完整读入
 http_conn::HTTP_CODE http_conn::parse_content(char *text)
 {
     if(m_read_idx >= (m_content_length + m_checked_idx))
     {
         text[m_content_length] = '\0';
         // POST 请求中最后是数入的用户名和密码
         m_string = text;
         return GET_REQUEST;
     }
     return NO_REQUEST;
 }

 http_conn::HTTP_CODE http_conn::process_read()
 {
     LINE_STATUS line_status = LINE_OK;
     HTTP_CODE ret = NO_REQUEST;
     char *text = 0;

     while((m_check_state == CHECK_STATE_CONTENT && line_status == LINE_OK) || ((line_status = parse_line()) == LINE_OK))
     {
         text = get_line();
         m_start_line = m_checked_idx;
         LOG_INFO("%s",text);
         switch(m_check_state)
         {
             case CHECK_STATE_REQUESTLINE:
             {  
                ret = parse_request_lines(text);
                if(ret == BAD_REQUEST)
                {
                    return BAD_REQUEST;
                }
                break;
             }
             case CHECK_STATE_HEADER:
             {
                 ret = parse_headers(text);
                 if(ret == BAD_REQUEST)
                 {
                     return BAD_REQUEST;
                 }
                 else if(ret == GET_REQUEST)
                 {
                     return do_request();
                 }
                 break;
             }
             case CHECK_STATE_CONTENT:
             {
                 ret = parse_content(text);
                 if(ret == GET_REQUEST)
                 {
                     return do_request();
                 }
                 line_status = LINE_OPEN;
                 break;
             }
             default:
                return INTERNAL_ERROR;
         }
     }
        return NO_REQUEST;
 }



 http_conn::HTTP_CODE http_conn::do_request()
 {
     //复制根目录
     strcpy(m_name_file,doc_root);
     int len = strlen(doc_root);
     const char *p = strrchr(m_url,'/');

     // 处理cgi
     if(cgi == 1 && (*(p+1) == '2' || *(p + 1) == '3'))
     {
         // 判断登录或注册
         char flag = m_url[1];

         char *m_url_name = (char*)malloc(sizeof(char) *200);
         strcpy(m_url_name,"/");
         strcat(m_url_name,(m_url+2));
         strncpy(m_name_file + len,m_url_name,FILENAME_LEN - len - 1);
         free(m_url_name);

         // 提出用户名和密码
         char name[64];
         char passwd[64];
         int i ;
         for(int i = 5;m_string[i] != '&';i++)
         {
             name[i - 5] = m_string[i];
         }
         name[i - 5] = '\0';

         int j = 0;
         for(i += 10;m_string[i] != '\0';i++,j++)
         {
             passwd[j] = m_string[i];
         }
         passwd[j] = '\0';

         if(*(p+1) == '3')
         {
             // 注册检测重名
             char *sql_insert = (char*)malloc(sizeof(char) * 200);
             strcpy(sql_insert,"INSERT INTO user(username,passwd) VALUES(");
             strcat(sql_insert,"'");
             strcat(sql_insert,name);
             strcat(sql_insert,"', '");
             strcat(sql_insert,passwd);
             strcat(sql_insert,"')");
            // 没有重名添加数据
             if(users.find(name) == users.end())
            {
                locker.lock();
                int res = mysql_query(sql,sql_insert);
                users.insert(std::pair<std::string,std::string>(name,passwd));
                locker.unlock();

                if(!res)
                {
                    strcpy(m_url,"/log.html");
                } 
                else
                {
                    strcpy(m_url,"/registerError.html");
                } 
            }
            else 
            {
                strcpy(m_url,"/registerError.html");
            }
         }
         // 登录判断
         else if(*(p+1) == '2')
         {
             if(users.find(name) != users.end() && users[name] == passwd)
             {
                 strcpy(m_url,"/welcome.html");
             }
             else
             {
                 strcpy(m_url,"/logError.html");
             }
         }
     }

         // 注册
         if(*(p+1) == '0')
         {
             char *m_url_name = (char *)malloc(sizeof(char) *200);
             strcpy(m_url_name,"/register.html");
             strncpy(m_name_file+len,m_url_name,strlen(m_url_name));
             free(m_url_name);
         }
         // 日志
         else if(*(p + 1) == '1')
         {
             char *m_url_name = (char *)malloc(sizeof(char) *200);
             strcpy(m_url_name,"/log.html");
             strncpy(m_name_file+len,m_url_name,strlen(m_url_name));
             free(m_url_name);
         }
          else if(*(p + 1) == '5')
         {
             char *m_url_name = (char *)malloc(sizeof(char) *200);
             strcpy(m_url_name,"/picture.html");
             strncpy(m_name_file+len,m_url_name,strlen(m_url_name));
             free(m_url_name);
         }
         else if(*(p + 1) == '6')
         {
             char *m_url_name = (char *)malloc(sizeof(char) *200);
             strcpy(m_url_name,"/video.html");
             strncpy(m_name_file+len,m_url_name,strlen(m_url_name));
             free(m_url_name);
         }
         else if(*(p + 1) == '7')
         {
             char *m_url_name = (char *)malloc(sizeof(char) *200);
             strcpy(m_url_name,"/fans.html");
             strncpy(m_name_file+len,m_url_name,strlen(m_url_name));
             free(m_url_name);
         }
         else
         {
             strncpy(m_name_file + len,m_url,FILENAME_LEN - len - 1);
         }
         if(stat(m_name_file,&m_file_stat) < 0)
         {
            return NO_RESOURCE;
         }
        if(!(m_file_stat.st_mode & S_IROTH))
        {
            return FORBIDOEN_REQUEST;
        }
        if(S_ISDIR(m_file_stat.st_mode))
        {
            return BAD_REQUEST;
        }

        int fd = open(m_name_file,O_RDONLY);
        m_file_address = (char *) mmap(0,m_file_stat.st_size,PROT_READ,MAP_PRIVATE,fd,0);
        close(fd);
        return FILE_REQUEST;
 }

// 取消mmap中stat文件的映射地址
 void http_conn::unmap()
 {
     // 如果地址存在，取消映射地址
     if(m_file_address)
     {
         munmap(m_file_address,m_file_stat.st_size);
         m_file_address = 0;  //将地址大小记为零
     }
 }
// 写http响应
 bool http_conn::write()
 {
     int temp = 0;
     if(bytes_to_send == 0)
     {
         modfd(m_epollfd,m_sockfd,EPOLLIN,m_TRIGMode);
         init();
         return true;
     }
     while(1)
     {
         temp = writev(m_sockfd,m_iv,m_iv_count);
         // 这里是为了防止tcp缓冲区数据写满，保持链接状态
         if(temp < 0)
         {
             if(errno == EAGAIN)
             {
                 modfd(m_epollfd,m_sockfd,EPOLLOUT,m_TRIGMode);
                 return true;
             }
             unmap();
             return false;
         }
         // 已经发送的数据大小
         bytes_have_send += temp;
         // 准备发送的数据大小
         bytes_to_send -= temp;

         // 如果发送的数据大于m_iv结构体的大小，调整重新发送
         if(bytes_have_send >= m_iv[0].iov_len)
         {
             m_iv[0].iov_len = 0;
             m_iv[1].iov_base = m_file_address + (bytes_have_send - m_write_idx);
             m_iv[1].iov_len = bytes_to_send;
         }
         else
         {
            m_iv[0].iov_base = m_write_buffer + bytes_have_send;
            m_iv[0].iov_len = m_iv[0].iov_len - bytes_have_send;
         }
        // 如果发送的数据小于等于零，则将重新设置事件属性
         if(bytes_to_send <= 0)
         {
             unmap();
             modfd(m_epollfd,m_sockfd,EPOLLIN,m_TRIGMode);
             if(m_linger)
             {
                 init();
                 return true;
             }
             else
             {
                 return false;
             }
         }
     }
 }
    // 添加http请求
    bool http_conn::add_response(const char *format,...)
    {
        if(m_write_idx >= WRITE_BUFFER_SIZE)
        {
            return false;
        }
        va_list ag_list;
        va_start(ag_list,format);
        int len = vsnprintf(m_write_buffer + m_write_idx,WRITE_BUFFER_SIZE - m_write_idx - 1,format,ag_list);
        if(len >= (WRITE_BUFFER_SIZE - 1 - m_write_idx))
        {
            va_end(ag_list);
            return false;
        }
        m_write_idx += len;
        va_end(ag_list);

        LOG_INFO("request:%s",m_write_buffer);
        return true;
    }

    bool http_conn::add_status_line (int status,const char *title)
    {
        return add_response("%s %d %s\r\n","HTTP/1.1",status,title);
    }

    bool http_conn::add_headers(int content_len)
    {
        return add_content_length(content_len) && add_linger() && add_blank_line();
    }

    bool http_conn::add_linger()
    {
        return add_response("Connection:%s\r\n",(m_linger == true) ? "keep-alive" : "close");
    }

    bool http_conn::add_blank_line ()
    {
        return add_response("%s","\r\n");
    }

    bool http_conn::add_content_length(int content_len)
    {
        return add_response("Connect-Length:%d\r\n",content_len);
    }

    bool http_conn::add_content_type()
    {
        return add_response("Connection-Type:%s\r\n","text/html");
    }

    bool http_conn::process_write(HTTP_CODE ret)
    {
        switch(ret)
        {
            case INTERNAL_ERROR:
            {
                add_status_line(500,error_500_title);
                add_headers(strlen(error_500_form));
                if(!add_content(error_500_form))
                {
                    return false;
                }
                break;
            }
            case BAD_REQUEST:
            {
                add_status_line(404,error_404_title);
                add_headers(strlen(error_404_form));
                if(!add_content(error_404_form))
                {
                    return false;
                }
                break;
            }
            case FORBIDOEN_REQUEST:
            {
                add_status_line(403,error_403_title);
                add_headers(strlen(error_403_form));
                if(!add_content(error_403_form))
                {
                    return false;
                }
                break;
            }
            case FILE_REQUEST:
            {
                add_status_line(200,ok_200_title);
                if(m_file_stat.st_size != 0)
                {
                    m_iv[0].iov_base = m_write_buffer;
                    m_iv[0].iov_len = m_write_idx;
                    m_iv[1].iov_base = m_file_address;
                    m_iv[1].iov_len = m_file_stat.st_size;
                    m_iv_count = 2;
                    bytes_to_send = m_write_idx + m_file_stat.st_size;
                    return true;
                }
                else
                {
                    const char * ok_string = "<html><body><body></html>";
                    add_headers(strlen(ok_string));
                    if(!add_content(ok_string))
                    {
                        return false;
                    }
                }
            }
            default:
            {
                return false;
            }
        }
        m_iv[0].iov_base = m_write_buffer;
        m_iv[0].iov_len = m_write_idx;
        m_iv_count = 1;
        bytes_to_send = m_write_idx;
        return true;
    }

    void http_conn::process()
    {
        HTTP_CODE read_ret = process_read();
        if(read_ret == NO_REQUEST)
        {
            modfd(m_epollfd,m_sockfd,EPOLLIN,m_TRIGMode);
            return;
        }
        bool write_ret = process_write(read_ret);
        if(!write_ret)
        {
            close_conn();
        }
        modfd(m_epollfd,m_sockfd,EPOLLOUT,m_TRIGMode);
    }