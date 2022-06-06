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
#include "util.h"

typedef enum {
	COROUTINE_DEAD = 0,
	COROUTINE_READY = 1,
	COROUTINE_RUNNING = 2,
	COROUTINE_SUSPEND = 3
} CoroutineStatus;

class Coroutine;

class Schedule {
public:
	Schedule(); //创建efd_
	~Schedule(); //关闭efd_
	void WakeupCoroutine(Coroutine* coroutine); //唤醒协程coroutine
	void CreateCoroutine(std::function<void()> run, size_t stack_size = 0, std::string coroutine_name = ""); //创建协程
	void Dispatch(); //调度协程
	void Yield(); //当前协程主动让出cpu并加入就绪队列ready_coroutines_
	void SwitchToSched(); //切到调度器
	void TakeOver(int fd); //将fd的边沿模式的读写事件添加到efd_中
	bool RegisterFdWithCurrCoroutine(int fd, int64_t expired_at, bool is_write); //注册fd
	bool LogoutFd(int fd); //注销fd
	ucontext_t* SchedCtx(); //调度器上下文

	static Schedule* coroutineManager() { //调度器(协程管理器)
		static thread_local Schedule xf;
		return &xf;
	}

private:
	int efd_; //epoll文件描述符
	std::deque<Coroutine*> ready_coroutines_; //协程就绪队列
	std::deque<Coroutine*> running_coroutines_; //协程运行队列
	ucontext_t sched_ctx_; //调度器的上下文
	Coroutine* curr_coroutine_; //正在运行的协程

	struct WaitingCoroutine {
		Coroutine* r_, * w_;
		WaitingCoroutine() {
			r_ = nullptr;
			w_ = nullptr;
		}
	};

	std::map<int, WaitingCoroutine> io_waiting_coroutines_; //一个fd由一个协程进行监听，但一个协程可能监听多个fd
	std::map<int64_t, std::set<Coroutine*>> expired_events_; //key是协程过期的时间 value是key时间过期的协程集合
};


class Coroutine {
public:
	Coroutine(std::function<void()> run, Schedule* coroutineManager, size_t stack_size, std::string coroutine_name);
	~Coroutine();
	ucontext_t* Ctx(); //协程上下文
	std::string Name(); //协程名字
	void setStatus(CoroutineStatus status); //设置协程状态
	bool IsFinished(); //协程是否DEAD
	uint64_t Seq(); //协程序列号
	static void Start(Coroutine* coroutine); //协程开始运行

	struct FdEvent {
		int fd_;
		int64_t expired_at_;

		FdEvent(int fd = -1, int64_t expired_at = -1) {
			if(expired_at <= 0) {
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

	WaitingEvents& GetWaitingEvents() { //该协程监听的读写事件
		return waiting_events_;
	}

	void SetReadEvent(const FdEvent& fe); //将fe添加到waiting_events_.waiting_fds_r_中，如果存在则修改
	void SetWriteEvent(const FdEvent& fe); //将fe添加到waiting_events_.waiting_fds_w_中，如果存在则修改
private:
	uint64_t seq_; //协程序列号
	Schedule* coroutineManager_; //管理该协程的Schedule
	std::string coroutine_name_; //协程名字
	CoroutineStatus status_; //协程状态
	ucontext_t ctx_; //协程上下文
	uint8_t* stack_ptr_; //协程栈指针
	size_t stack_size_; //协程栈大小
	std::function<void()> run_; //协程运行的函数
	WaitingEvents waiting_events_; //该协程监听的读写事件
};
