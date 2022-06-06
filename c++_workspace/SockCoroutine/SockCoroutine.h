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


class Fd {
public:
    Fd();

    ~Fd();

    static uint32_t next_seq_;

    int RawFd();

    void RegisterFdToSched();

    bool Available();

protected:
    int fd_;

    int seq_;
};


class Client;

class Server : public Fd {
public:

    Server();

    ~Server();

    std::shared_ptr<Client> Accept();

    void SetFd(int fd);

    static Server ListenTCP(uint16_t port);

private:
    uint16_t port_;
};


class Client : public Fd {
public:
    Client();

    Client(int fd);

    ~Client();

    static std::shared_ptr<Client> ConnectTCP(const char *ipv4, uint16_t port);

    ssize_t Write(const char *buf, size_t sz, int timeout_ms=-1) const;

    ssize_t Read(char *buf, size_t sz, int timeout_ms=-1) const;
};
