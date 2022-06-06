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
	static uint32_t next_seq_; //��һ�����к�
	int RawFd(); //fd_
	void RegisterFdToSched(); //�½�һ��Э�̵���������fdע���ȥ
	bool IsValid(); //fd�Ƿ�Ϸ�
protected:
	int fd_; //�ļ�������
	int seq_; //���к�
};


class Connection;

class Server : public Fd {
public:
	Server();
	~Server();
	Connection* Accept(); //���������������ؼ������Ŀͻ���
	void SetFd(int fd); //���ü����׽���
	static Server ListenTCP(uint16_t port); //��������ķ�����
private:
	uint16_t port_; //�˿�
};


class Connection : public Fd {
public:
	Connection();
	Connection(int fd); //���ÿͻ����׽���
	~Connection();
	Connection* ConnectTCP(const char* ipv4, uint16_t port); //���ӷ�����
	ssize_t Write(const char* buf, size_t sz, int timeout_ms = -1) const; //���������ͻ���д��
	ssize_t Read(char* buf, size_t sz, int timeout_ms = -1) const; //�������ӿͻ��˶���
};

