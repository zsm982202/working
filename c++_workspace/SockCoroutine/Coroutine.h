#pragma once

#include <set>
#include <map>
#include <list>
#include <queue>
#include <vector>
#include <string>
#include <functional>
#include <ucontext.h>

#include "log.h"

typedef enum {
    COROUTINE_INIT = 0,
    COROUTINE_READY = 1,
    COROUTINE_WAITING = 2,
    COROUTINE_DEAD = 3
} CoroutinueStatus;


class Coroutinue;

class Schedule {
public:
    Schedule(); //创建efd_

    ~Schedule();//关闭efd_

    void WakeupCoroutinue(Coroutinue *coroutinue); //唤醒协程coroutinue

    //创建协程
    void CreateCoroutinue(std::function<void()> run, size_t stack_size = 0, std::string coroutinue_name="");

    void Dispatch(); //调度协程

    void Yield(); //当前协程主动让出cpu并加入就绪队列ready_coroutinues_

    void SwitchToSched();  //切到调度器

    void TakeOver(int fd); //将fd的边沿模式的读写事件添加到efd_中

    bool RegisterFdWithCurrCoroutinue(int fd, int64_t expired_at, bool is_write); //注册fd

    bool LogoutFd(int fd); //注销fd

    ucontext_t *SchedCtx(); //调度器上下文

    static Schedule *coroutinueManager() {
        static thread_local Schedule xf;
        return &xf;
    }

private:
    int efd_; //epoll文件描述符
    
    std::deque<Coroutinue *> ready_coroutinues_;

    std::deque<Coroutinue *> running_coroutinues_;

    ucontext_t sched_ctx_;

    Coroutinue *curr_coroutinue_; //正在运行的协程

    struct WaitingCoroutinue {
        Coroutinue *r_, *w_;
        WaitingCoroutinue() {
            r_ = nullptr;
            w_ = nullptr;
        }
    };

    std::map<int, WaitingCoroutinue> io_waiting_coroutinues_;
    // 会不会出现一个fd的读/写被多个协程监听？？不会！
    // 但是一个coroutinue可能会监听多个fd，实际也不存在，一个连接由一个协程处理

    std::map<int64_t, std::set<Coroutinue *>> expired_events_;

    std::vector<Coroutinue *> finished_coroutinues_;
};


class Coroutinue
{
public:
    Coroutinue(std::function<void ()> run, Schedule *coroutinueManager, size_t stack_size, std::string coroutinue_name);

    ~Coroutinue();

    ucontext_t *Ctx();

    std::string Name();

    bool IsFinished();
    
    uint64_t Seq();

    static void Start(Coroutinue *coroutinue);

    struct FdEvent {
        int fd_;
        int64_t expired_at_;

        FdEvent(int fd =-1, int64_t expired_at=-1) {
            if (expired_at <= 0) {
                expired_at = -1;
            }
            fd_ = fd;
            expired_at_ = expired_at;
        }
    };

    struct WaitingEvents {
        // 一个协程中监听的fd不会太多，所以直接用数组
        std::vector<FdEvent> waiting_fds_r_;
        std::vector<FdEvent> waiting_fds_w_;
    };

    WaitingEvents &GetWaitingEvents() {
        return waiting_events_;
    }

    //将fe添加到waiting_events_.waiting_fds_r_中
    void SetReadEvent(const FdEvent &fe) {
        for (size_t i = 0; i < waiting_events_.waiting_fds_r_.size(); ++i) {
            if (waiting_events_.waiting_fds_r_[i].fd_ == fe.fd_) {
                waiting_events_.waiting_fds_r_[i].expired_at_ = fe.expired_at_;
                return;
            }
        }
        waiting_events_.waiting_fds_r_.push_back(fe);
    }

    //将fe添加到waiting_events_.waiting_fds_w_中
    void SetWriteEvent(const FdEvent &fe) {
        for (size_t i = 0; i < waiting_events_.waiting_fds_w_.size(); ++i) {
            if (waiting_events_.waiting_fds_w_[i].fd_ == fe.fd_) {
                waiting_events_.waiting_fds_w_[i].expired_at_ = fe.expired_at_;
                return ;
            }
        }
        waiting_events_.waiting_fds_w_.push_back(fe);
    }



private:
    uint64_t seq_;

    Schedule *coroutinueManager_;

    std::string coroutinue_name_;

    CoroutinueStatus status_;

    ucontext_t ctx_;

    uint8_t *stack_ptr_;

    size_t stack_size_;
    
    std::function<void ()> run_;

    WaitingEvents waiting_events_;

};

