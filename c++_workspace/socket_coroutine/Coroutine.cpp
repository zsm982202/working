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
#include "Coroutine.h"


Schedule::Schedule() {
	curr_coroutine_ = nullptr;
	efd_ = epoll_create1(0);
	if (efd_ < 0) {
		LOG_ERROR("epoll_create failed, msg=%s", strerror(errno));
		exit(-1);
	}
}

Schedule::~Schedule() {
	close(efd_);
}

ucontext_t *Schedule::SchedCtx() {
	return &sched_ctx_;
}

void Schedule::WakeupCoroutine(Coroutine *coroutine) {
	// 加入就绪队列
	coroutine->setStatus(CoroutineStatus::COROUTINE_READY);
	ready_coroutines_.push_back(coroutine);

	// 从等待队列中删除
	Coroutine::WaitingEvents& waiting_events = coroutine->GetWaitingEvents();
	for (size_t i = 0; i < waiting_events.waiting_fds_r_.size(); i++) {
		// 从等待队列中删除
		int fd = waiting_events.waiting_fds_r_[i].fd_;
		auto iter = io_waiting_coroutines_.find(fd);
		if (iter != io_waiting_coroutines_.end()) {
			io_waiting_coroutines_.erase(iter);
		}
		// 从等待队列中删除超时的事件
		int64_t expired_at = waiting_events.waiting_fds_r_[i].expired_at_;
		if(expired_at > 0) {
			auto expired_iter = expired_events_.find(expired_at);
			if(expired_iter->second.find(coroutine) == expired_iter->second.end()) {
				LOG_ERROR("not coroutine [%lu] in expired events", coroutine->Seq());
			} else {
				expired_iter->second.erase(coroutine);
			}
		}
	}
	for (size_t i = 0; i < waiting_events.waiting_fds_w_.size(); i++) {
		// 从等待队列中删除
		int fd = waiting_events.waiting_fds_w_[i].fd_;
		auto iter = io_waiting_coroutines_.find(fd);
		if (iter != io_waiting_coroutines_.end()) {
			io_waiting_coroutines_.erase(iter);
		}
		// 从等待队列中删除超时的事件
		int64_t expired_at = waiting_events.waiting_fds_w_[i].expired_at_;
		if(expired_at > 0) {
			auto expired_iter = expired_events_.find(expired_at);
			if(expired_iter->second.find(coroutine) == expired_iter->second.end()) {
				LOG_ERROR("not coroutine [%lu] in expired events", coroutine->Seq());
			} else {
				expired_iter->second.erase(coroutine);
			}
		}
	}
}

//创建协程并加入ready_coroutines_
void Schedule::CreateCoroutine(std::function<void ()> run, size_t stack_size, std::string coroutine_name) {
	if (stack_size == 0) {
		stack_size = 1024 * 1024;
	}
	Coroutine *coroutine = new Coroutine(run, this, stack_size, coroutine_name);
	ready_coroutines_.push_back(coroutine);
	LOG_DEBUG("create a new coroutine with id[%lu]", coroutine->Seq());
}

void Schedule::Dispatch() {
	while (true) {
		if (ready_coroutines_.size() > 0) {
			running_coroutines_ = std::move(ready_coroutines_); //左值转右值，省去拷贝的时间
			ready_coroutines_.clear();
			LOG_DEBUG("there are %ld coroutine(s) in ready list, ready to run...", running_coroutines_.size());

			for (auto iter = running_coroutines_.begin(); iter != running_coroutines_.end(); iter++) {
				Coroutine *coroutine = *iter;
				curr_coroutine_ = coroutine;
				LOG_DEBUG("switch from sched to coroutine[%lu]", coroutine->Seq());
				assert(swapcontext(SchedCtx(), coroutine->Ctx()) == 0);
				curr_coroutine_ = nullptr;

				if (coroutine->IsFinished()) {
					LOG_INFO("coroutine[%lu] finished, free it!", coroutine->Seq());
					delete coroutine;
				}
			}
			running_coroutines_.clear();
		}

		int64_t now_ms = util::NowMs(); 
		//唤醒超时的协程Coroutine
		while (!expired_events_.empty() && expired_events_.begin()->first <= now_ms) {
			std::set<Coroutine *> &expired_coroutines = expired_events_.begin()->second;
			while (!expired_coroutines.empty()) {
				std::set<Coroutine *>::iterator expired_coroutine = expired_coroutines.begin();
				WakeupCoroutine(*expired_coroutine);
			}
			expired_events_.erase(expired_events_.begin());
		}

#define MAX_EVENT_COUNT 512
		struct epoll_event evs[MAX_EVENT_COUNT];
		int n = epoll_wait(efd_, evs, MAX_EVENT_COUNT, 0); //不阻塞立刻返回
		if (n < 0) {
			LOG_ERROR("epoll_wait error, msg=%s", strerror(errno));
			continue;
		}

		for (int i = 0; i < n; i++) {
			struct epoll_event &ev = evs[i];
			int fd = ev.data.fd;

			auto coroutine_iter = io_waiting_coroutines_.find(fd);
			if (coroutine_iter != io_waiting_coroutines_.end()) { //fd在等待队列中
				WaitingCoroutine &waiting_coroutine = coroutine_iter->second;
				//唤醒读事件对应的协程io_waiting_coroutines_[fd]
				if (ev.events & EPOLLIN) {
					LOG_DEBUG("waiting fd[%d] has fired IN event, wake up pending coroutine[%lu]", fd, waiting_coroutine.r_->Seq());
					WakeupCoroutine(waiting_coroutine.r_);
				}
				else if (ev.events & EPOLLOUT) {
					if (waiting_coroutine.w_ == nullptr) {
						LOG_WARNING("fd[%d] has been fired OUT event, but not found any coroutine to handle!", fd);
					}
					else {//唤醒写事件对应的协程io_waiting_coroutines_[fd]
						LOG_DEBUG("waiting fd[%d] has fired OUT event, wake up pending coroutine[%lu]", fd, waiting_coroutine.w_->Seq());
						WakeupCoroutine(waiting_coroutine.w_);
					}
				}
			}
		}
	}
}

void Schedule::Yield() {
	assert(curr_coroutine_ != nullptr);
	//主动切出的后仍然是ready状态，等待下次调度
	curr_coroutine_->setStatus(CoroutineStatus::COROUTINE_READY);
	ready_coroutines_.push_back(curr_coroutine_);
	SwitchToSched();
}

//切换到schedule
void Schedule::SwitchToSched() {
	assert(curr_coroutine_ != nullptr);
	LOG_DEBUG("switch to sched");
	assert(swapcontext(curr_coroutine_->Ctx(), SchedCtx()) == 0);
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

bool Schedule::RegisterFdWithCurrCoroutine(int fd, int64_t expired_at, bool is_write) {
	/*
	   op = 0 读
	   op = 1 写
	 */

	assert(curr_coroutine_ != nullptr);
	if(expired_at > 0) {
		expired_events_[expired_at].insert(curr_coroutine_);
	}

	auto iter = io_waiting_coroutines_.find(fd);
	if(iter == io_waiting_coroutines_.end()) {
		WaitingCoroutine wf;
		if(!is_write) { // 读
			wf.r_ = curr_coroutine_;
			io_waiting_coroutines_.insert(std::make_pair(fd, wf));
			curr_coroutine_->SetReadEvent(Coroutine::FdEvent(fd, expired_at));
		} else { // 写
			wf.w_ = curr_coroutine_;
			io_waiting_coroutines_.insert(std::make_pair(fd, wf));
			curr_coroutine_->SetWriteEvent(Coroutine::FdEvent(fd, expired_at));
		}
	} else {
		if(!is_write) {
			iter->second.r_ = curr_coroutine_;
			curr_coroutine_->SetReadEvent(Coroutine::FdEvent(fd, expired_at));
		} else {
			iter->second.w_ = curr_coroutine_;
			curr_coroutine_->SetWriteEvent(Coroutine::FdEvent(fd, expired_at));
		}
	}
	return true;
}

//注销fd
bool Schedule::LogoutFd(int fd) {
	auto iter = io_waiting_coroutines_.find(fd);
	if(iter != io_waiting_coroutines_.end()) {
		WaitingCoroutine& waiting_coroutines = iter->second;
		Coroutine* coroutine_r = waiting_coroutines.r_;
		Coroutine* coroutine_w = waiting_coroutines.w_;

		//将读协程从expired_events_中删除
		if(coroutine_r != nullptr) {
			Coroutine::WaitingEvents& evs_r = coroutine_r->GetWaitingEvents();
			for(size_t i = 0; i < evs_r.waiting_fds_r_.size(); i++) {
				if(evs_r.waiting_fds_r_[i].fd_ == fd) {
					int64_t expired_at = evs_r.waiting_fds_r_[i].expired_at_;
					if(expired_at > 0) {
						auto expired_iter = expired_events_.find(expired_at);
						if(expired_iter->second.find(coroutine_r) == expired_iter->second.end()) {
							LOG_ERROR("not coroutine [%lu] in expired events", coroutine_r->Seq());
						} else {
							expired_iter->second.erase(coroutine_r);
						}
					}
				}
			}
		}
		//将写协程从expired_events_中删除
		if(coroutine_w != nullptr) {
			Coroutine::WaitingEvents& evs_w = coroutine_w->GetWaitingEvents();
			for(size_t i = 0; i < evs_w.waiting_fds_w_.size(); i++) {
				if(evs_w.waiting_fds_w_[i].fd_ == fd) {
					int64_t expired_at = evs_w.waiting_fds_w_[i].expired_at_;
					if(expired_at > 0) {
						auto expired_iter = expired_events_.find(expired_at);
						if(expired_iter->second.find(coroutine_w) == expired_iter->second.end()) {
							LOG_ERROR("not coroutine [%lu] in expired events", coroutine_w->Seq());
						} else {
							expired_iter->second.erase(coroutine_r);
						}
					}
				}
			}
		}
		io_waiting_coroutines_.erase(iter);
	} else {
		LOG_INFO("fd[%d] not register into sched", fd);
	}

	struct epoll_event ev;
	ev.events = EPOLLIN | EPOLLOUT | EPOLLET;
	ev.data.fd = fd;
	//将fd下树
	if(epoll_ctl(efd_, EPOLL_CTL_DEL, fd, &ev) < 0) {
		LOG_ERROR("unregister fd[%d] from epoll efd[%d] failed, msg=%s", fd, efd_, strerror(errno));
	} else {
		LOG_INFO("unregister fd[%d] from epoll efd[%d] success!", fd, efd_);
	}
	return true;
}


thread_local uint64_t coroutine_seq = 0;

Coroutine::Coroutine(std::function<void()> run, Schedule* coroutineManager, size_t stack_size, std::string coroutine_name) {
	run_ = run;
	coroutineManager_ = coroutineManager;
	coroutine_name_ = coroutine_name;
	stack_size_ = stack_size;
	stack_ptr_ = new uint8_t[stack_size_];

	getcontext(&ctx_);
	ctx_.uc_stack.ss_sp = stack_ptr_;
	ctx_.uc_stack.ss_size = stack_size_;
	ctx_.uc_link = coroutineManager->SchedCtx();
	makecontext(&ctx_, (void (*)())Coroutine::Start, 1, this);

	seq_ = coroutine_seq++;
	status_ = CoroutineStatus::COROUTINE_SUSPEND;
}

Coroutine::~Coroutine() {
	delete[]stack_ptr_;
	stack_ptr_ = nullptr;
	stack_size_ = 0;
}

uint64_t Coroutine::Seq() {
	return seq_;
}

ucontext_t* Coroutine::Ctx() {
	return &ctx_;
}

void Coroutine::Start(Coroutine* coroutine) {
	coroutine->status_ = CoroutineStatus::COROUTINE_RUNNING;
	coroutine->run_();
	coroutine->status_ = CoroutineStatus::COROUTINE_DEAD;
	LOG_DEBUG("coroutine[%lu] finished...", coroutine->Seq());
}

std::string Coroutine::Name() {
	return coroutine_name_;
}

void Coroutine::setStatus(CoroutineStatus status) {
	status_ = status;
}

bool Coroutine::IsFinished() {
	return status_ == CoroutineStatus::COROUTINE_DEAD;
}

void Coroutine::SetReadEvent(const FdEvent& fe) {
	for(size_t i = 0; i < waiting_events_.waiting_fds_r_.size(); ++i) {
		if(waiting_events_.waiting_fds_r_[i].fd_ == fe.fd_) {
			waiting_events_.waiting_fds_r_[i].expired_at_ = fe.expired_at_;
			return;
		}
	}
	waiting_events_.waiting_fds_r_.push_back(fe);
}

void Coroutine::SetWriteEvent(const FdEvent& fe) {
	for(size_t i = 0; i < waiting_events_.waiting_fds_w_.size(); ++i) {
		if(waiting_events_.waiting_fds_w_[i].fd_ == fe.fd_) {
			waiting_events_.waiting_fds_w_[i].expired_at_ = fe.expired_at_;
			return;
		}
	}
	waiting_events_.waiting_fds_w_.push_back(fe);
}
