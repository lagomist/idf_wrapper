#pragma once

#include "bufdef.h"
#include <optional>
#include <stdint.h>
#include <string>

namespace Wrapper {

namespace OS {

enum class Scheduler : uint8_t {
	SUSPENDED   = 0,
	NOT_STARTED = 1,
	RUNNING     = 2,
};

void start();
void delay(uint32_t ms);
void yield();
uint32_t tick_rate();
uint32_t tick();
uint32_t tick_ms();
uint32_t tick_s();
void scheduler_suspend();
void scheduler_resume();
bool in_isr_context();
Scheduler get_scheduler();

OBuf get_task_list();

void disable_all_interrupts();
void enabled_all_interrupts();


class Task {
public:
	using TaskFunc = void (*)(void*);

	enum Core: int8_t {
		CORE_0 = 0,
		CORE_1 = 1,
		CORE_AUTO = -1
	};

	struct Cfg {
		const char* name = nullptr;
		uint32_t stack_size = 4096;
		uint32_t priority = 5;
		Core core_id = Core::CORE_AUTO;
		TaskFunc func = nullptr;
		void* arg = nullptr;
	};

	enum class State : uint8_t {
		Running = 0,     /* A task is querying the state of itself, so must be running. */
		Ready,           /* The task being queried is in a read or pending ready list. */
		Blocked,         /* The task being queried is in the Blocked state. */
		Suspended,       /* The task being queried is in the Suspended state, or is in the Blocked state with an infinite time out. */
		Deleted,         /* The task being queried has been deleted, but its TCB has not yet been freed. */
		Invalid          /* Used as an 'invalid state' value. */
	};

	enum class NotifyAction : uint8_t {
		eNoAction = 0,         
		eSetBits,              
		eIncrement,            
		eSetValueWithOverwrite,
		eSetValueWithoutOverwrite,
	};

	static void selfDelete();

	void create(TaskFunc func, void* arg, const char name[], uint32_t stack_size, uint32_t priority, int core_id = -1);
	void create(const Cfg& cfg);
	void del();
	void suspend();
	void resume();
	void set_priority(uint32_t priority);
	uint32_t get_priority() const;
	uint32_t get_min_stack_remaining() const;
	State state() const;

	std::string_view get_name();

	void notify(uint32_t events = 0, NotifyAction act = NotifyAction::eNoAction);
	int notifyWait(uint32_t clear_bit = 0, uint32_t timeout = -1);

	void* get_handle() const { return _handle; }

	bool is_inited() const { return get_handle(); }

private:
	void* _handle = nullptr;
};


class MutexBase {
public:
	virtual int lock(uint32_t timeout_ms = -1) { return 0; }
	virtual int unlock() { return 0; }
};

class Mutex : public MutexBase {
public:
	Mutex();
	~Mutex();
	int lock(uint32_t timeout_ms = -1) override;
	int unlock() override;
private:
	void* _handle;
};

class RecursiveMutex : public MutexBase {
public:
	RecursiveMutex();
	~RecursiveMutex();
	int lock(uint32_t timeout_ms = -1) override;
	int unlock() override;
private:
	void* _handle;
};

class LockGuard {
public:
	LockGuard(MutexBase& mutex):_mutex(mutex) {mutex.lock();}
	~LockGuard() {_mutex.unlock();}
private:
	MutexBase& _mutex;
};

class Semaphore {
public:
	Semaphore();
	~Semaphore();
	int take(uint32_t timeout_ms = -1);
	int give();
private:
	void* _handle;
};

class Queue {
public:
	Queue(uint32_t length, uint32_t item_size);
	~Queue();
	// 可以在ISR中调用，但此时timeout无效
	int send(const void* item, uint32_t timeout_ms = -1);
	// 不能在ISR中调用
	int receive(void* buf, uint32_t timeout_ms = -1);

	int get_msg_num();
private:
	void* _handle;
};

template <typename T>
class QueueT : protected Queue {
public:
	QueueT(uint32_t length) : Queue(length, sizeof(T)) {}
	// 可以在ISR中调用，但此时timeout无效
	int send(const T& item, uint32_t timeout_ms = -1) {
		return Queue::send(&item, timeout_ms);
	}
	// 不能在ISR中调用
	int receive(T& item, uint32_t timeout_ms = -1) {
		return Queue::receive(&item, timeout_ms);
	}

	std::optional<T> receive(uint32_t timeout_ms = -1) {
		T item;
		if(receive(item, timeout_ms) == 0)
			return item;
		else
			return std::nullopt;
	}

	using Queue::get_msg_num;
};

class EventGroup {
public:
	EventGroup();
	~EventGroup();
	void set(uint32_t events);
	uint32_t get();
	void clear(uint32_t events);
	// return: negative means timeout; positive means succeed. will not return 0
	// no matter which case,  the lower 24bits contains the received events before clear
	int wait_all(uint32_t events, bool auto_clear = true, uint32_t timeout_ms = -1);
	int wait_any(uint32_t events, bool auto_clear = true, uint32_t timeout_ms = -1);
private:
	void* _handle;
};


// 为计算timer运行时间，进入、退出timer时会调用如下weak函数，可覆盖。
class Timer {
public:
	using Callback = void(*)();
	Timer(Callback cb, uint32_t period_ms, bool auto_reload = true, const char name[] = nullptr, void* arg = nullptr);
	~Timer();
	void start();
	void restart();
	void restart(uint32_t period_ms);
	void stop();
	bool is_running();
	uint32_t get_period();
	const char* get_name();
	uint32_t get_number();
	void* arg;
private:
	void* _handle;
	static uint32_t _timer_number; 
	Callback _cb;
	static void timer_cb_adapter(void* timer_handle);
};


} // namespace OS

} // namespace Wrapper
