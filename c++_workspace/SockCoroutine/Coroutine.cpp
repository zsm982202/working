#include <cstdio>
#include <errno.h>
#include <error.h>
#include <cstring>
#include <iostream>
#include <sys/epoll.h>
#include <sys/types.h>
#include <signal.h>
#include <unistd.h>
#include <assert.h>
#include "Corountine.h"


Schedule::Schedule() {
    curr_coroutinue_ = nullptr;
    efd_ = epoll_create1(0);
    if (efd_ < 0) {
        LOG_ERROR("epoll_create failed, msg=%s", strerror(errno));
        exit(-1);
    }
}

Schedule::~Schedule() {
    close(efd_);
}

ScheduleCtx *Schedule::SchedCtx() {
    return &sched_ctx_;
}

void Schedule::WakeupCoroutinue(Coroutinue *coroutinue) {
    // 1. 加入就绪队列
    ready_coroutinues_.push_back(coroutinue);

    // 2. 从等待队列中删除
    std::set<int> waiting_fds;
    auto waiting_events = coroutinue->GetWaitingEvents();
    for (size_t i = 0; i < waiting_events.waiting_fds_r_.size(); i++) {
        int fd = waiting_events.waiting_fds_r_[i].fd_;
        auto iter = io_waiting_coroutinues_.find(fd);
        if (iter != io_waiting_coroutinues_.end()) {
            io_waiting_coroutinues_.erase(iter);
            waiting_fds.insert(fd);
        }
    }
    for (size_t i = 0; i < waiting_events.waiting_fds_w_.size(); i++) {
        int fd = waiting_events.waiting_fds_w_[i].fd_;
        auto iter = io_waiting_coroutinues_.find(fd);
        if (iter != io_waiting_coroutinues_.end()) {
            io_waiting_coroutinues_.erase(iter);
            waiting_fds.insert(fd);
        }
    }

    // 3. 从超时队列中删除
    Coroutinue::WaitingEvents &evs = coroutinue->GetWaitingEvents();
    for (size_t i = 0; i < evs.waiting_fds_r_.size(); i++) {
        int64_t expired_at = evs.waiting_fds_r_[i].expired_at_;
        if (expired_at > 0) {
            auto expired_iter = expired_events_.find(expired_at);
            if (expired_iter->second.find(coroutinue) == expired_iter->second.end()) {
                LOG_ERROR("not coroutinue [%lu] in expired events", coroutinue->Seq());
            }
            else {
                expired_iter->second.erase(coroutinue);
            }
        }
    }

    for (size_t i = 0; i < evs.waiting_fds_w_.size(); i++) {
        int64_t expired_at = evs.waiting_fds_w_[i].expired_at_;
        if (expired_at > 0) {
            auto expired_iter = expired_events_.find(expired_at);
            if (expired_iter->second.find(coroutinue) == expired_iter->second.end()) {
                LOG_ERROR("not coroutinue [%lu] in expired events", coroutinue->Seq());
            }
            else {
                expired_iter->second.erase(coroutinue);
            }
        }
    }
}

//创建协程并加入ready_coroutinues_
void Schedule::CreateCoroutinue(std::function<void ()> run, size_t stack_size, std::string coroutinue_name) {
    if (stack_size == 0) {
        stack_size = 1024 * 1024;
    }
    Coroutinue *coroutinue = new Coroutinue(run, this, stack_size, coroutinue_name);
    ready_coroutinues_.push_back(coroutinue);
    LOG_DEBUG("create a new coroutinue with id[%lu]", coroutinue->Seq());
}

void Schedule::Dispatch() {
    //由于epoll_wait(efd_, evs, MAX_EVENT_COUNT, 2)最多2ms进入下一次循环
    while (true) {
        if (ready_coroutinues_.size() > 0) {
            running_coroutinues_ = std::move(ready_coroutinues_); //?
            ready_coroutinues_.clear();
            LOG_DEBUG("there are %ld coroutinue(s) in ready list, ready to run...", running_coroutinues_.size());

            for (auto iter = running_coroutinues_.begin(); iter != running_coroutinues_.end(); iter++) {
                Coroutinue *coroutinue = *iter;
                curr_coroutinue_ = coroutinue;
                LOG_DEBUG("switch from sched to coroutinue[%lu]", coroutinue->Seq());
                assert(swapcontext(SchedCtx(), coroutinue->Ctx()) == 0);
                curr_coroutinue_ = nullptr;

                if (coroutinue->IsFinished()) {
                    LOG_INFO("coroutinue[%lu] finished, free it!", coroutinue->Seq());
                    delete coroutinue;
                }
            }
            running_coroutinues_.clear();
        }

        int64_t now_ms = util::NowMs(); 
        //唤醒超时的协程Coroutinue
        while (!expired_events_.empty() && expired_events_.begin()->first <= now_ms) {
            std::set<Coroutinue *> &expired_coroutinues = expired_events_.begin()->second;
            while (!expired_coroutinues.empty()) {
                std::set<Coroutinue *>::iterator expired_coroutinue = expired_coroutinues.begin();
                WakeupCoroutinue(*expired_coroutinue);
            }
            expired_events_.erase(expired_events_.begin());
        }

        #define MAX_EVENT_COUNT 512
        struct epoll_event evs[MAX_EVENT_COUNT];
        int n = epoll_wait(efd_, evs, MAX_EVENT_COUNT, 2); //阻塞2ms
        if (n < 0) {
            LOG_ERROR("epoll_wait error, msg=%s", strerror(errno));
            continue;
        }

        
        for (int i = 0; i < n; i++) {
            struct epoll_event &ev = evs[i];
            int fd = ev.data.fd;
            
            auto coroutinue_iter = io_waiting_coroutinues_.find(fd);
            if (coroutinue_iter != io_waiting_coroutinues_.end()) { //fd在等待队列中
                WaitingCoroutinue &waiting_coroutinue = coroutinue_iter->second;
                //唤醒读事件对应的协程io_waiting_coroutinues_[fd]
                if (ev.events & EPOLLIN) {
                    LOG_DEBUG("waiting fd[%d] has fired IN event, wake up pending coroutinue[%lu]", fd, waiting_coroutinue.r_->Seq());
                    WakeupCoroutinue(waiting_coroutinue.r_);
                }
                else if (ev.events & EPOLLOUT) {
                    if (waiting_coroutinue.w_ == nullptr) {
                        LOG_WARNING("fd[%d] has been fired OUT event, but not found any coroutinue to handle!", fd);
                    }
                    else {//唤醒写事件对应的协程io_waiting_coroutinues_[fd]
                        LOG_DEBUG("waiting fd[%d] has fired OUT event, wake up pending coroutinue[%lu]", fd, waiting_coroutinue.w_->Seq());
                        WakeupCoroutinue(waiting_coroutinue.w_);
                    }
                }
            }
        }
    }
}

void Schedule::Yield() {
    assert(curr_coroutinue_ != nullptr);
    // 主动切出的后仍然是ready状态，等待下次调度
    ready_coroutinues_.push_back(curr_coroutinue_);
    SwitchToSched();
}

//切换到schedule
void Schedule::SwitchToSched() {
    assert(curr_coroutinue_ != nullptr);
    LOG_DEBUG("switch to sched");
    assert(swapcontext(curr_coroutinue_->Ctx(), SchedCtx()) == 0);
}

//将fd的边沿模式的读写事件添加到efd_中
void Schedule::TakeOver(int fd) {
    struct epoll_event ev;
    ev.events = EPOLLIN | EPOLLOUT | EPOLLET;
    ev.data.fd = fd;

    if (epoll_ctl(efd_, EPOLL_CTL_ADD, fd, &ev) < 0) {
        LOG_ERROR("add fd [%d] into epoll failed, msg=%s", fd, strerror(errno));
        exit(-1);
    }
    LOG_DEBUG("add fd[%d] into epoll event success", fd);
}

bool Schedule::RegisterFdWithCurrCoroutinue(int fd, int64_t expired_at, bool is_write) {
    /*
        op = 0 读
        op = 1 写
    */

    assert(curr_coroutinue_ != nullptr);
    if (expired_at > 0) {
        expired_events_[expired_at].insert(curr_coroutinue_);
    }

    auto iter = io_waiting_coroutinues_.find(fd);
    if (iter == io_waiting_coroutinues_.end()) {
        WaitingCoroutinue wf;
        if (!is_write) { // 读
            wf.r_ = curr_coroutinue_;
            io_waiting_coroutinues_.insert(std::make_pair(fd, wf));
            curr_coroutinue_->SetReadEvent(Coroutinue::FdEvent(fd, expired_at));
        }
        else {
            wf.w_ = curr_coroutinue_;
            io_waiting_coroutinues_.insert(std::make_pair(fd, wf));
            curr_coroutinue_->SetWriteEvent(Coroutinue::FdEvent(fd, expired_at));
        }
    }
    else {
        if (!is_write) {
            iter->second.r_ = curr_coroutinue_;
            curr_coroutinue_->SetReadEvent(Coroutinue::FdEvent(fd, expired_at));
        }
        else {
            iter->second.w_ = curr_coroutinue_;
            curr_coroutinue_->SetWriteEvent(Coroutinue::FdEvent(fd, expired_at));
        }
    }
    return true;
}

//注销fd
bool Schedule::LogoutFd(int fd) {
    auto iter = io_waiting_coroutinues_.find(fd);
    if (iter != io_waiting_coroutinues_.end()) {
        WaitingCoroutinue &waiting_coroutinues = iter->second;
        Coroutinue *coroutinue_r = waiting_coroutinues.r_;
        Coroutinue *coroutinue_w = waiting_coroutinues.w_;

        //将读协程从expired_events_中删除
        if (coroutinue_r != nullptr) {
            Coroutinue::WaitingEvents &evs_r = coroutinue_r->GetWaitingEvents();
            for (size_t i = 0; i < evs_r.waiting_fds_r_.size(); i++) {
                if (evs_r.waiting_fds_r_[i].fd_ == fd) {
                    int64_t expired_at = evs_r.waiting_fds_r_[i].expired_at_;
                    if (expired_at > 0) {
                        auto expired_iter = expired_events_.find(expired_at);
                        if (expired_iter->second.find(coroutinue_r) == expired_iter->second.end()) {
                            LOG_ERROR("not coroutinue [%lu] in expired events", coroutinue_r->Seq());
                        }
                        else {
                            expired_iter->second.erase(coroutinue_r);
                        }
                    }
                }
            }
        }
        //将写协程从expired_events_中删除
        if (coroutinue_w != nullptr) {
            Coroutinue::WaitingEvents &evs_w = coroutinue_w->GetWaitingEvents();
            for (size_t i = 0; i < evs_w.waiting_fds_w_.size(); i++) {
                if (evs_w.waiting_fds_w_[i].fd_ == fd) {
                    int64_t expired_at = evs_w.waiting_fds_w_[i].expired_at_;
                    if (expired_at > 0) {
                        auto expired_iter = expired_events_.find(expired_at);
                        if (expired_iter->second.find(coroutinue_w) == expired_iter->second.end()) {
                            LOG_ERROR("not coroutinue [%lu] in expired events", coroutinue_w->Seq());
                        }
                        else {
                            expired_iter->second.erase(coroutinue_r);
                        }
                    }
                }
            }
        }
        io_waiting_coroutinues_.erase(iter);
    }
    else {
        LOG_INFO("fd[%d] not register into sched", fd);
    }

    struct epoll_event ev;
    ev.events = EPOLLIN | EPOLLOUT | EPOLLET;
    ev.data.fd = fd;
    //将fd下树
    if (epoll_ctl(efd_, EPOLL_CTL_DEL, fd, &ev) < 0) {
        LOG_ERROR("unregister fd[%d] from epoll efd[%d] failed, msg=%s", fd, efd_, strerror(errno));
    }
    else {
        LOG_INFO("unregister fd[%d] from epoll efd[%d] success!", fd, efd_);
    }
    return true;
}


thread_local uint64_t coroutinue_seq = 0;

Coroutinue::Coroutinue(std::function<void ()> run, Schedule *coroutinueManager, size_t stack_size, std::string coroutinue_name) {
    run_ = run;
    coroutinueManager_ = coroutinueManager;
    coroutinue_name_ = coroutinue_name;
    stack_size_ = stack_size;
    stack_ptr_ = new uint8_t[stack_size_];
    
    getcontext(&ctx_);
    ctx_.uc_stack.ss_sp = stack_ptr_;
    ctx_.uc_stack.ss_size = stack_size_;
    ctx_.uc_link = coroutinueManager->SchedCtx();
    makecontext(&ctx_, (void (*)())Coroutinue::Start, 1, this);

    seq_ = coroutinue_seq++;
    status_ = CoroutinueStatus::COROUTINE_INIT;
}

Coroutinue::~Coroutinue() {
    delete []stack_ptr_;
    stack_ptr_ = nullptr;
    stack_size_ = 0;
}
    
uint64_t Coroutinue::Seq() {
    return seq_;
}

ScheduleCtx *Coroutinue::Ctx() {
    return &ctx_;
}

void Coroutinue::Start(Coroutinue *coroutinue) {
    coroutinue->run_();
    coroutinue->status_ = CoroutinueStatus::COROUTINE_DEAD;
    LOG_DEBUG("coroutinue[%lu] finished...", coroutinue->Seq());
}

std::string Coroutinue::Name() {
    return coroutinue_name_;
}

bool Coroutinue::IsFinished() {
    return status_ == CoroutinueStatus::COROUTINE_DEAD;
}


