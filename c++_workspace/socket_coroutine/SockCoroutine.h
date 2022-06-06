#pragma once

#include <memory>
#include <unistd.h>
#include <inttypes.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include "util.h"


class Fd {
public:
	Fd();
	~Fd();
	static uint32_t next_seq_; //下一个序列号
	int RawFd(); //fd_
	void RegisterFdToSched(); //新建一个协程调度器并将fd注册进去
	bool IsValid(); //fd是否合法
protected:
	int fd_; //文件描述符
	int seq_; //序列号
};


class Connection;

class Server : public Fd {
public:
	Server();
	~Server();
	Connection* Accept(); //服务器监听，返回监听到的客户端
	void SetFd(int fd); //设置监听套接字
	static Server ListenTCP(uint16_t port); //负责监听的服务器
private:
	uint16_t port_; //端口
};


class Connection : public Fd {
public:
	Connection();
	Connection(int fd); //设置客户端套接字
	~Connection();
	Connection* ConnectTCP(const char* ipv4, uint16_t port); //连接服务器
	ssize_t Write(const char* buf, size_t sz, int timeout_ms = -1) const; //服务器往客户端写入
	ssize_t Read(char* buf, size_t sz, int timeout_ms = -1) const; //服务器从客户端读出
};

