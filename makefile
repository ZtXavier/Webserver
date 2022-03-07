server: main.cpp  timer.cpp http_conn.cpp log.cpp sql_conn.cpp server.cpp config.cpp
	g++ -o server -g -lpthread -lmysqlclient

clean:
	rm  -r server