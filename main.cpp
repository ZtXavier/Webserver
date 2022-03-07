#include<iostream>
#include"config.hpp"
#include"server.hpp"
#include"mute.hpp"
#include"log.hpp"
#include"thread_pool.hpp"


int main(int argc,char *argv[])
{

    std::string user = "root";
    std::string passwd = "123456";
    std::string dbname = "webserver";

    server_config cfg;
    cfg.parse_arg(argc,argv);

    my_Webserver webserver;
    webserver.server_init(cfg.PORT,user,passwd,dbname,cfg.WRITE_LOG,cfg.opt_linger,cfg.TRIGMode,cfg.sql_num,cfg.thread_num,cfg.close_log,cfg.actor_model);
    
    webserver.log_write();
    
    webserver.mysql_pool();

    webserver.threads_pool();

    webserver.trig_mode();

    webserver.eventlisten();

    webserver.eventloop();

    return 0;

}