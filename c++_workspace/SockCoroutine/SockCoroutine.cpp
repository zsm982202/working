#include "SockCoroutine.h"
#include "Coroutinue.h"


uint32_t Fd::next_seq_ = 0;


Fd::Fd() {
    fd_ = -1;
    seq_ = Fd::next_seq_++;
}

Fd::~Fd() {

}

bool Fd::Available() {
    return fd_ > 0;
}

int Fd::RawFd() {
    return fd_;
}

void Fd::RegisterFdToSched() {
    CoroutineManager *coroutinueManager = CoroutineManager::coroutinueManager();
    coroutinueManager->TakeOver(fd_);
}

Server::Server() {

}

Server::~Server() {
    close(fd_);
}

Server Server::ListenTCP(uint16_t port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        exit(-1);
    }

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    int flag = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag)) < 0) {
        LOG_ERROR("try set SO_REUSEADDR failed, msg=%s", strerror(errno));
        exit(-1);
    }

    if (fcntl(fd, F_SETFL, O_NONBLOCK) < 0) {
        LOG_ERROR("set set listen fd O_NONBLOCK failed, msg=%s", strerror(errno));
        exit(-1);
    }

    //bind
    if (bind(fd, (sockaddr *)&addr, sizeof(sockaddr_in)) < 0) {
        LOG_ERROR("try bind port [%d] failed, msg=%s", port, strerror(errno));
        exit(-1);
    }

    //listen
    if (listen(fd, 10) < 0) {
        LOG_ERROR("try listen port[%d] failed, msg=%s", port, strerror(errno));
        exit(-1);
    }

    Server server;
    server.SetFd(fd); //设置fd_为监听套接字

    LOG_INFO("listen %d success...", port);
    CoroutineManager::coroutinueManager()->TakeOver(fd); //将fd的边沿模式的读写事件添加到efd_中

    return server;
}

void Server::SetFd(int fd) {
    fd_ = fd;
}

std::shared_ptr<Client> Server::Accept() {
    CoroutineManager *coroutinueManager = CoroutineManager::coroutinueManager();

    while (true) {
        int client_fd = accept(fd_, nullptr, nullptr);
        if (client_fd > 0) {
            //设置client_fd为非阻塞
            if (fcntl(client_fd, F_SETFL, O_NONBLOCK) != 0) {
                perror("fcntl");
                exit(-1);
            }
            int nodelay = 1;
            //启动TCP_NODELAY，就意味着禁用了Nagle算法，允许小包的发送。
            //对于延时敏感型，同时数据传输量比较小的应用，开启TCP_NODELAY选项无疑是一个正确的选择。
            if (setsockopt(client_fd, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof(nodelay)) < 0) {
                LOG_ERROR("try set TCP_NODELAY failed, msg=%s", strerror(errno));
                close(client_fd);
                client_fd = -1;
            }
            coroutinueManager->TakeOver(client_fd); //将client_fd的边沿模式的读写事件添加到efd_中
            return std::shared_ptr<Client>(new Client(client_fd));
        }
        else {
            if (errno == EAGAIN) {
                // accept失败，协程切出
                coroutinueManager->RegisterFdWithCurrCoroutinue(fd_, -1, false); //永久注册读监听fd_（不会过期）
                coroutinueManager->SwitchToSched(); //切到调度器
            }
            else if (errno == EINTR) {
                LOG_INFO("accept client connect return interrupt error, ignore and conitnue...");
            }
            else {
                perror("accept");
            }
        }
    }
    return std::shared_ptr<Client>(new Client(-1));
}


Client::Client() {
}

Client::Client(int fd) {
    fd_ = fd;
}

Client::~Client() {
    CoroutineManager::coroutinueManager()->LogoutFd(fd_);
    LOG_INFO("close fd[%d]", fd_);
    close(fd_);
    fd_ = -1;
}


std::shared_ptr<Client> Client::ConnectTCP(const char *ipv4, uint16_t port) {
    int fd = socket(AF_INET,SOCK_STREAM, 0);
 
    struct sockaddr_in svr_addr;
    memset(&svr_addr, 0, sizeof(svr_addr));
    svr_addr.sin_family = AF_INET;
    svr_addr.sin_port = htons(port);  
    svr_addr.sin_addr.s_addr = inet_addr(ipv4);  
 
    //连接服务器，成功返回0，错误返回-1
    if (connect(fd, (struct sockaddr *)&svr_addr, sizeof(svr_addr)) < 0)
    {
        LOG_ERROR("try connect %s:%d failed, msg=%s", ipv4, port, strerror(errno));
        return std::shared_ptr<Client>(new Client(-1));
    }

    int nodelay = 1;
    if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof(nodelay)) < 0) {
        LOG_ERROR("try set TCP_NODELAY failed, msg=%s", strerror(errno));
        close(fd);
        return std::shared_ptr<Client>(new Client(-1));
    }

    if (fcntl(fd, F_SETFL, O_NONBLOCK) < 0) {
        LOG_ERROR("set set fd[%d] O_NONBLOCK failed, msg=%s", fd, strerror(errno));
        close(fd);
        return std::shared_ptr<Client>(new Client(-1));
    }
    LOG_DEBUG("connect %s:%d success with fd[%d]", ipv4, port, fd);
    CoroutineManager::coroutinueManager()->TakeOver(fd);

    return std::shared_ptr<Client>(new Client(fd));
}

ssize_t Client::Write(const char *buf, size_t sz, int timeout_ms) const {
    size_t write_bytes = 0;
    CoroutineManager *coroutinueManager = CoroutineManager::coroutinueManager();
    int64_t expired_at = timeout_ms > 0 ? util::NowMs() + timeout_ms : -1;

    while (write_bytes < sz) {
        int n = write(fd_, buf + write_bytes, sz - write_bytes);
        if (n > 0) {
            write_bytes += n;
            LOG_DEBUG("write to fd[%d] return %d, total send %ld bytes", fd_, n, write_bytes);
        }
        else if (n == 0) {
            LOG_INFO("write to fd[%d] return 0 byte, peer has closed", fd_);
            return 0;
        }
        else {
            if (expired_at > 0 && util::NowMs() >= expired_at) {
                LOG_WARNING("write to fd[%d] timeout after wait %dms", fd_, timeout_ms);
                return 0;
            }
            if (errno != EAGAIN && errno != EINTR) {
                LOG_DEBUG("write to fd[%d] failed, msg=%s", fd_, strerror(errno));
                return -1;
            }
            else if (errno == EAGAIN) {
                //buf已经写完
                LOG_DEBUG("write to fd[%d] return EAGIN, add fd into IO waiting events and switch to sched", fd_);
                coroutinueManager->RegisterFdWithCurrCoroutinue(fd_, expired_at, true); //注册客户端fd，为了接下来继续监听客户端读事件
                coroutinueManager->SwitchToSched(); //切到调度器
            }
            else {
                //pass
            }
        }
    }
    LOG_DEBUG("write to fd[%d] for %ld byte(s) success", fd_, sz);
    return sz;
}

ssize_t Client::Read(char *buf, size_t sz, int timeout_ms) const {
    CoroutineManager *coroutinueManager = CoroutineManager::coroutinueManager();
    int64_t expired_at = timeout_ms > 0 ? util::NowMs() + timeout_ms : -1;

    while (true) {
        int n = read(fd_, buf, sz);
        LOG_DEBUG("read from fd[%d] reutrn %d bytes", fd_, n);
        if (n > 0) {
            return n;
        }
        else if (n == 0) {
            LOG_DEBUG("read from fd[%d] return 0 byte, peer has closed", fd_);
            return 0;
        }
        else {
            if (expired_at > 0 && util::NowMs() >= expired_at) {
                LOG_WARNING("read from fd[%d] timeout after wait %dms", fd_, timeout_ms);
                return 0;
            }
            if (errno != EAGAIN && errno != EINTR) {
                LOG_DEBUG("read from fd[%d] failed, msg=%s", fd_, strerror(errno))
                return -1;
            }
            else if (errno == EAGAIN) {
                LOG_DEBUG("read from fd[%d] return EAGIN, add into waiting/expired events with expired at %ld  and switch to sched", fd_, expired_at);
                coroutinueManager->RegisterFdWithCurrCoroutinue(fd_, expired_at, false);
                coroutinueManager->SwitchToSched();
            }
            else if (errno == EINTR) {
                //pass
            }
        }
    }
    return -1;
}
