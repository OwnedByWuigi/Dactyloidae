#include <Mw/Milsko.h>

#if defined(_WIN32)
long MwTimeGetTick(void) {
	return timeGetTime();
}

void MwTimeSleep(int ms) {
	Sleep(ms);
}
#elif defined(__APPLE__)
long MwTimeGetTick(void) {
	struct timespec ts;
	long		n = 0;
	clock_serv_t	cclock;
	mach_timespec_t mts;

	host_get_clock_service(mach_host_self(), CALENDAR_CLOCK, &cclock);
	clock_get_time(cclock, &mts);
	mach_port_deallocate(mach_task_self(), cclock);

	n += ts.tv_nsec / 1000 / 1000;
	n += ts.tv_sec * 1000;

	return n;
}
void MwTimeSleep(int ms) {
	struct timespec ts;

	ts.tv_sec  = ms / 1000;
	ts.tv_nsec = (ms % 1000) * 1000 * 1000;

	nanosleep(&ts, NULL);
}
#elif defined(CLASSIC_MAC_OS)
// static TMTask tm;
// static int    timer;
// void	      timerUpdateFunc(void) {
// 	 timer += 1;
// 	 PrimeTime((QElemPtr)&tm, 1);
// };

long MwTimeGetTick(void) {
	UnsignedWide h;
	Microseconds(&h);
	return h.lo;
}
void MwTimeSleep(int ms) {
	// tm.tmAddr     = NewTimerProc(timerUpdateFunc);
	// tm.tmWakeUp   = 0;
	// tm.tmReserved = 0;

	// InsTime((QElemPtr)&tm);
	// PrimeTime((QElemPtr)&tm, 1);

	// DisposeTimerUPP(tm.tmAddr);
}
#elif defined(__unix__) || defined(__HAIKU__)
long MwTimeGetTick(void) {
	struct timespec ts;
	long		n = 0;

	clock_gettime(CLOCK_MONOTONIC, &ts);

	n += ts.tv_nsec / 1000 / 1000;
	n += ts.tv_sec * 1000;

	return n;
}

void MwTimeSleep(int ms) {
	struct timespec ts;

	ts.tv_sec  = ms / 1000;
	ts.tv_nsec = (ms % 1000) * 1000000;

	nanosleep(&ts, NULL);
}
#endif
