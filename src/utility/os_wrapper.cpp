#include "os_wrapper.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"
// #include <esp_private/freertos_idf_additions_priv.h>
#include <esp_log.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <utility>

extern "C" __attribute__((weak))
void vApplicationMallocFailedHook() {
	assert(0);
}

extern "C" __attribute__((weak))
void vApplicationStackOverflowHook(TaskHandle_t xTask, char * pcTaskName ) {
	assert(0);
}

namespace Wrapper {

namespace OS {

#define CALL_FROM_ISR(func, ...) do {			\
	BaseType_t switch_flag = 0;					\
	func(_hd, ##__VA_ARGS__, &switch_flag);		\
	portYIELD_FROM_ISR(switch_flag);			\
} while(0)

constexpr static const char TAG[] = "Wrapper::OS";

static uint32_t ms2ticks(uint32_t ms) {
	return ms/(1000/configTICK_RATE_HZ);
}

// freertos definition: 1-Good, 0-Bad
// mine:0-Good, -1 bad
static int ret(BaseType_t ret_value) {
	return ret_value == 1 ? 0 : -1;
}

uint32_t to(uint32_t timeout) {
	return timeout == -1 ? portMAX_DELAY : ms2ticks(timeout);
}

void start() {
	vTaskStartScheduler();
}

void delay(uint32_t ms) {
	vTaskDelay(ms2ticks(ms));
}

void yield() {
	taskYIELD();
}

uint32_t tick_rate() {
	return configTICK_RATE_HZ;
}

uint32_t tick() {
	return xTaskGetTickCount();
}
uint32_t tick_ms() {
	return tick() * portTICK_PERIOD_MS;
}

uint32_t tick_s() {
	return tick_ms() / 1000;
}

void scheduler_suspend() {
	vTaskSuspendAll();
}

void scheduler_resume() {
	xTaskResumeAll();
}

Scheduler get_scheduler() {
	return (Scheduler)xTaskGetSchedulerState();
}

OBuf get_task_list() {
	OBuf list(2048, '\0');
	vTaskList((char*)list.data());
	list.resize(strlen((char*)list.data()));
	return list;
}


void disable_all_interrupts() {
    portDISABLE_INTERRUPTS();
}

void enabled_all_interrupts() {
    portENABLE_INTERRUPTS();
}

bool in_isr_context() {
#ifdef ESP_PLATFORM
	return xPortInIsrContext();
#elif defined __arm__
	uint32_t result;
	__asm__ volatile ("MRS %0, ipsr" : "=r" (result) );
	return(result);
#elif defined __linux__
	return false;
#endif
}

/* ----------------------------------- Task ----------------------------------- */

#define _hd ((TaskHandle_t)_handle)

void Task::create(TaskFunc func, void* arg, const char name[], uint32_t stack_size, uint32_t priority, int core_id) {
	xTaskCreatePinnedToCore(func, name, stack_size, arg, priority, (TaskHandle_t*)&_handle, core_id < 0 ? tskNO_AFFINITY : core_id);
	assert(_hd);
	ESP_LOGI(TAG, "task %s created", name);
}

void Task::create(const Cfg& cfg) {
	create(cfg.func, cfg.arg, cfg.name, cfg.stack_size, cfg.priority, cfg.core_id);
}

void Task::del() {
	ESP_LOGI(TAG, "task %s deleted", get_name().data());
	assert(_hd);
	vTaskDelete(_hd);
	_handle = nullptr;
}

void Task::suspend() {
	vTaskSuspend(_hd);
}

void Task::resume() {
	if(!in_isr_context())
		vTaskResume(_hd);
	else {
		portYIELD_FROM_ISR(xTaskResumeFromISR(_hd));
	}
}

void Task::set_priority(uint32_t priority) {
	vTaskPrioritySet(_hd, priority);
}

uint32_t Task::get_priority() const {
	return uxTaskPriorityGet(_hd);
}

uint32_t Task::get_min_stack_remaining() const {
	return uxTaskGetStackHighWaterMark(_hd);
}

Task::State Task::state() const {
	return (State)eTaskGetState(_hd);
}

std::string_view Task::get_name() {
	return pcTaskGetName((TaskHandle_t )_handle);
}

void Task::notify(uint32_t events, NotifyAction act) {
	if(!in_isr_context())
		xTaskNotify(_hd, events, eNoAction);
	else
		CALL_FROM_ISR(xTaskNotifyFromISR, events, eNoAction);
}

int Task::notifyWait(uint32_t clear_bit, uint32_t timeout) {
	uint32_t events = 0;
	xTaskNotifyWait(0, clear_bit, &events, to(timeout));
	return events;
}

#undef _hd

/* ----------------------------------- Mutex ----------------------------------- */

#define _hd ((SemaphoreHandle_t)_handle)

Mutex::Mutex() :
_handle((void*)xSemaphoreCreateMutex()) {
	assert(_handle);
}

Mutex::~Mutex() {
	vSemaphoreDelete(_hd);
}

int Mutex::lock(uint32_t timeout) {
	return ret(xSemaphoreTake(_hd, to(timeout)));
}

int Mutex::unlock() {
	if(!in_isr_context())
		return ret(xSemaphoreGive(_hd));
	else {
		CALL_FROM_ISR(xSemaphoreGiveFromISR);
		return 0;
	}
}

RecursiveMutex::RecursiveMutex() :
_handle((void*)xSemaphoreCreateRecursiveMutex()) {
	assert(_handle);
}

RecursiveMutex::~RecursiveMutex() {
	vSemaphoreDelete(_hd);
}

int RecursiveMutex::lock(uint32_t timeout) {
	return ret(xSemaphoreTakeRecursive(_hd, to(timeout)));
}

int RecursiveMutex::unlock() {
	return ret(xSemaphoreGiveRecursive(_hd));
}

/* ----------------------------------- Semaphore ----------------------------------- */

Semaphore::Semaphore() {
	_handle = (void*)xSemaphoreCreateBinary();
	assert(_handle);
}

Semaphore::~Semaphore() {
	vSemaphoreDelete(_hd);
}

int Semaphore::take(uint32_t timeout) {
	return ret(xSemaphoreTake(_hd, to(timeout)));
}

int Semaphore::give() {
	if(!in_isr_context())
		return ret(xSemaphoreGive(_hd));
	else {
		CALL_FROM_ISR(xSemaphoreGiveFromISR);
		return 0;
	}
}

#undef _hd

/* ----------------------------------- Queue ----------------------------------- */

#define _hd	((QueueHandle_t)_handle)

Queue::Queue(uint32_t length, uint32_t item_size) {
	_handle = (void*)xQueueCreate(length, item_size);
	assert(_handle);
}
Queue::~Queue() {
	vQueueDelete(_hd);
}

int Queue::send(const void* item, uint32_t timeout_ms) {
	if(!in_isr_context())
		return ret(xQueueSend(_hd, item, to(timeout_ms)));
	else {
		CALL_FROM_ISR(xQueueSendFromISR, item);
		return 0;
	}
}

int Queue::receive(void* buf, uint32_t timeout_ms) {
	return ret(xQueueReceive(_hd, buf, to(timeout_ms)));
}

int Queue::get_msg_num() {
	return uxQueueMessagesWaiting(_hd);
}
#undef _hd

/* ----------------------------------- EventGroup ----------------------------------- */

#define _hd	((EventGroupHandle_t)_handle)

EventGroup::EventGroup() {
	_handle = (void*)xEventGroupCreate();
	assert(_handle);
}

EventGroup::~EventGroup() {
	vEventGroupDelete(_hd);
}

void EventGroup::set(uint32_t events) {
	if(!in_isr_context())
		xEventGroupSetBits(_hd, events);
	else {
		CALL_FROM_ISR(xEventGroupSetBitsFromISR, events);
	}
}

void EventGroup::clear(uint32_t event_id) {
	xEventGroupClearBits(_hd, event_id);
}

uint32_t EventGroup::get() {
	return xEventGroupGetBits(_hd);
}

int EventGroup::wait_any(uint32_t events, bool auto_clear, uint32_t timeout) {
	uint32_t received_events = xEventGroupWaitBits(_hd, events, auto_clear, pdFALSE, to(timeout));
	//Received events not matching the unblock condition means timeout. set received events to negative
	if(!(received_events & events))
		received_events |= 0x80000000u;
	return received_events;
}

int EventGroup::wait_all(uint32_t events, bool auto_clear, uint32_t timeout) {
	uint32_t received_events = xEventGroupWaitBits(_hd, events, auto_clear, pdTRUE, to(timeout));
	//cannot use == to judge unblock condition. the number of received events might be more than the events you wait for
	if((received_events & events) != events)
		received_events |= 0x80000000u;
	return received_events;
}

#undef _hd


/* ----------------------------------- Timer ----------------------------------- */

#define _hd ((TimerHandle_t)_handle)

Timer::Timer(Callback cb, uint32_t period_ms, bool auto_reload, const char name[], void* arg) :
arg(arg), _cb(cb) {
	_handle = (void *)xTimerCreate(name, ms2ticks(period_ms), auto_reload, 0, (TimerCallbackFunction_t)timer_cb_adapter);
	assert(_handle);
	vTimerSetTimerID(_hd, this);
}

Timer::~Timer() {
	xTimerDelete(_hd, portMAX_DELAY);
}

void Timer::start() {
	// timeout specifies the waiting time for the command queue of daemon task to be available
	// command queue wont be full under most circumstance
	if(!in_isr_context())
		xTimerStart(_hd, portMAX_DELAY);
	else
		CALL_FROM_ISR(xTimerStartFromISR);
}

void Timer::stop() {
	if(!in_isr_context())
		xTimerStop(_hd, portMAX_DELAY);
	else
		CALL_FROM_ISR(xTimerStopFromISR);
}

void Timer::restart() {
	if(!in_isr_context())
		xTimerReset(_hd, portMAX_DELAY);
	else
		CALL_FROM_ISR(xTimerResetFromISR);
}

// xTimerChangePeriod will start a timer when it's not started
void Timer::restart(uint32_t period_ms) {
	if(period_ms > 0){
		if(!in_isr_context())
			xTimerChangePeriod(_hd, ms2ticks(period_ms), portMAX_DELAY);
		else
			CALL_FROM_ISR(xTimerChangePeriodFromISR, ms2ticks(period_ms));
	}
}

bool Timer::is_running() {
	return xTimerIsTimerActive(_hd);
}

uint32_t Timer::get_period() {
	return pdTICKS_TO_MS(xTimerGetPeriod(_hd));
}

const char* Timer::get_name() {
	return pcTimerGetName(_hd);
}

void Timer::timer_cb_adapter(void* _handle) {
	Timer* pthis = reinterpret_cast<Timer*>(pvTimerGetTimerID(_hd));
	pthis->_cb(*pthis);
}

#undef _hd




} // namespace OS

} // namespace Wrapper
