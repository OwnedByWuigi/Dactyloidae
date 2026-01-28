/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */	

#include "SlowAsynchronousTaskScheduler.h"
#include "mozilla/TimeStamp.h"
#include "nsComponentManagerUtils.h"

//#define DEBUG_SATS 1
using namespace mozilla;

// If we're expecting a tick to happen soon, don't use timer based tick.
static const uint32_t kExpectedTimeOverlapLimit = 10;

SlowAsynchronousTaskScheduler::SlowAsynchronousTaskScheduler()

: mShuttingDown(false)
{
}

SlowAsynchronousTaskScheduler::~SlowAsynchronousTaskScheduler()
{
  MOZ_ASSERT(!mTimer);
  while (ScheduledSlowTask* task = mScheduledSlowTasks.popFirst()) {
    delete task;
  }
}

void
SlowAsynchronousTaskScheduler::ExpectedTick(uint32_t aMillisecondsFromNow)
{
  TimeStamp newExpectedTick = FromNow(aMillisecondsFromNow);
  if (mExpectedNonTimerTick.IsNull() ||
      newExpectedTick < mExpectedNonTimerTick) {
    mExpectedNonTimerTick = newExpectedTick;
  }
}

void
SlowAsynchronousTaskScheduler::Tick(bool aFromTimer)
{
  CancelTimer();	
  if (mScheduledSlowTasks.isEmpty()) {
    if (!aFromTimer) {
      mExpectedNonTimerTick = TimeStamp();
    }
    return;
  }
  TimeStamp now = TimeStamp::Now();
  TimeDuration limit =
    TimeDuration::FromMilliseconds(
      static_cast<double>(kExpectedTimeOverlapLimit));

  // We have a tick from timer, but we're possibly waiting for a non-timer
  // tick happening real soon. If so, don't handle this tick.

  if (aFromTimer && !mExpectedNonTimerTick.IsNull() &&
      now < mExpectedNonTimerTick &&
      now + limit > mExpectedNonTimerTick) {

    // Try to run the timer right after the expected tick, since
    // the expected tick might get lost.
    uint32_t newTimerValue =
      static_cast<uint32_t>((mExpectedNonTimerTick - now).ToMilliseconds()) + 1;
    EnsureTimer(newTimerValue);
    return;
  }

  mExpectedNonTimerTick = TimeStamp();
  ScheduledSlowTask* current = mScheduledSlowTasks.popFirst();

  if (!current) {
    return;
  }

  if (current->mRunnable) {
#ifdef DEBUG_SATS
    printf("SlowAsynchronousTaskScheduler::Tick[timer=%s], runnable \n",
           aFromTimer ? "true" : "false");
#endif
    nsCOMPtr<nsIRunnable> runnable;
    runnable.swap(current->mRunnable);
    delete current;
    current = nullptr;
    runnable->Run();
  } else {
#ifdef DEBUG_SATS
    printf("SlowAsynchronousTaskScheduler::Tick[timer=%s] task=%p ",
           aFromTimer ? "true" : "false", current->mSlowTasks);
    for (uint32_t i = 0; current->mSlowTasks[i].mCallback; ++i) {
      printf("[%u]", current->mSlowTasks[i].mID);
    }
#endif
    DependentSlowTask& task =
      current->mSlowTasks[current->mIndexOfNextTask];
#ifdef DEBUG_SATS
    printf(", will run id=%u\n", task.mID);
#endif
    // Run the currently scheduled task, and store state of the next task.
    void* data = current->mDataForNextTask;
    current->mDataForNextTask = nullptr;

    SATSState nextState = task.mCallback(task.mID, data);
    uint32_t nextID = nextState.mState;
    uint32_t i = 0;
    for (; current->mSlowTasks[i].mCallback; ++i) {
      if (current->mSlowTasks[i].mID == nextID) {
        break;
      }
    }

    // The nextID did point to another task which has a callback to run.
    // Schedule that.
    if (current->mSlowTasks[i].mCallback) {
      current->mIndexOfNextTask = i;
      current->mDataForNextTask = nextState.mData;
      // Reschedule, since it now has new mIndexOfNextTask.
      AddScheduledSlowTask(current, current->mSlowTasks[i].mDelayMillis);
    } else {
      // Couldn't find anything to schedule.	
      delete current;
    }
  }

  // Make sure we have a timer running if we have something scheduled.
  ScheduledSlowTask* first = mScheduledSlowTasks.getFirst();

  if (first) {
#ifdef DEBUG_SATS
    printf("Ensuring timer for the next task\n");
#endif
    uint32_t millis = first->mExpectedTimeToRun > now ?
      static_cast<uint32_t>(
        (first->mExpectedTimeToRun - now).ToMilliseconds()) :
      0;
    EnsureTimer(millis);
  }
}

void
SlowAsynchronousTaskScheduler::Schedule(nsIRunnable* aRunnable,
                                        uint32_t aMillis)
{
#ifdef DEBUG_SATS
  printf("SlowAsynchronousTaskScheduler::Schedule aRunnable\n");
#endif

  AddScheduledSlowTask(new ScheduledSlowTask(aRunnable), aMillis);
  EnsureTimer(aMillis);
}

void
SlowAsynchronousTaskScheduler::Schedule(DependentSlowTask* aTasks,
                                        void* aData)

{
#ifdef DEBUG_SATS
  printf("SlowAsynchronousTaskScheduler::Schedule aTasks ");
  for (uint32_t i = 0; aTasks[i].mCallback; ++i) {
    printf("[%u]", aTasks[i].mID);
  }
  printf("\n");
#endif

  MOZ_ASSERT(aTasks && aTasks[0].mCallback);
  AddScheduledSlowTask(new ScheduledSlowTask(aTasks, 0, aData),
                       aTasks[0].mDelayMillis);
  EnsureTimer(aTasks[0].mDelayMillis);
}

void
SlowAsynchronousTaskScheduler::Schedule(DependentSlowTask* aTasks,
                                        uint32_t aID, void* aData)
{
#ifdef DEBUG_SATS
  printf("SlowAsynchronousTaskScheduler::Schedule aTask ");
  for (uint32_t i = 0; aTasks[i].mCallback; ++i) {
    printf("[%u]", aTasks[i].mID);
  }
  printf(", scheduling=%u\n", aID);	
#endif

  for (uint32_t i = 0; aTasks[i].mCallback; ++i) {
    if (aTasks[i].mID == aID) {
      MOZ_ASSERT(aTasks[i].mCallback);
      AddScheduledSlowTask(new ScheduledSlowTask(aTasks, i, aData),
                           aTasks[i].mDelayMillis);
      EnsureTimer(aTasks[i].mDelayMillis);
      return;
    }
  }
  MOZ_ASSERT(false, "Unknown ID?");
}

void
SlowAsynchronousTaskScheduler::CancelScheduledTask(nsIRunnable* aRunnable)
{
  if (aRunnable) {
    ScheduledSlowTask* task = mScheduledSlowTasks.getFirst();
    while(task) {
      ScheduledSlowTask* next = task->getNext();
      if (task->mRunnable == aRunnable) {
        task->remove();
        delete task;
      } 	
      task = next;	
    }
  }	
}
	
void	
SlowAsynchronousTaskScheduler::CancelScheduledTask(DependentSlowTask* aTasks)	
{
  if (aTasks) {
    ScheduledSlowTask* task = mScheduledSlowTasks.getFirst();
    while(task) {
      ScheduledSlowTask* next = task->getNext();	
      if (task->mSlowTasks == aTasks) {
        task->remove();
        delete task;
      }
      task = next;
    }
  }
}

void	
SlowAsynchronousTaskScheduler::CancelScheduledTask(DependentSlowTask* aTasks,
                                                   uint32_t aID)
{
  if (aTasks) {
    ScheduledSlowTask* task = mScheduledSlowTasks.getFirst();
    while(task) {
      ScheduledSlowTask* next = task->getNext();
      if (task->mSlowTasks == aTasks &&
          task->mSlowTasks[task->mIndexOfNextTask].mID == aID) {
        task->remove();
        delete task;	
      }	
      task = next;	
    }
  }	
}

bool
SlowAsynchronousTaskScheduler::IsScheduled(nsIRunnable* aRunnable)
{
  if (aRunnable) {
    ScheduledSlowTask* task = mScheduledSlowTasks.getFirst();
    while(task) {
      if (task->mRunnable == aRunnable) {
        return true;
      }
      task = task->getNext();
    }
  }
  return false;	
}	

bool
SlowAsynchronousTaskScheduler::IsScheduled(DependentSlowTask* aTasks)
{
  if (aTasks) {
    ScheduledSlowTask* task = mScheduledSlowTasks.getFirst();
    while(task) {
      if (task->mSlowTasks == aTasks) {
        return true;
      }
      task = task->getNext();
    }
  }
  return false;	
}

bool
SlowAsynchronousTaskScheduler::IsScheduled(DependentSlowTask* aTasks,
                                           uint32_t aID)
{
  if (aTasks) {
    ScheduledSlowTask* task = mScheduledSlowTasks.getFirst();
    while(task) {
      if (task->mSlowTasks == aTasks &&
          task->mSlowTasks[task->mIndexOfNextTask].mID == aID) {
        return true;
      }
      task = task->getNext();
    }
  }
  return false;
}

class SATSTimerTick : public nsCancelableRunnable
{
public:
  NS_IMETHOD Run()
  {
    if (!mCancelled) {
      SlowAsynchronousTaskScheduler* sats =
        CycleCollectedJSContext::GetScheduler();
      if (sats) {
        sats->TickFromTimer();
      }
    }
    return NS_OK;
  }
  NS_IMETHOD Cancel()
  {
    mCancelled = true;
    return NS_OK;
  }
  bool mCancelled = false;
};

void
SlowAsynchronousTaskScheduler::EnsureTimer(uint32_t aMillis)
{
#ifdef DEBUG_SATS
  printf("SlowAsynchronousTaskScheduler::EnsureTimer %ums\n", aMillis);
#endif
  TimeStamp fromNow = FromNow(aMillis);
  if (mTimer) {
    if (!mTimerTime.IsNull() && mTimerTime < fromNow) {
      // We will run the timer sooner than aMillis from now.	
      return;
    }
    mTimer->Cancel();
  } else {
    if (mShuttingDown) {
      return;
    }
    mTimer = do_CreateInstance("@mozilla.org/timer;1");
    if (!mTimer) {
      NS_WARNING("No timer!");
      return;
    }
    mTimerCallback = new SATSTimerTick();
  }
  mTimerTime = fromNow;
  CycleCollectedJSContext::ScheduleTimer(mTimer, mTimerCallback, aMillis);
}

void
SlowAsynchronousTaskScheduler::AddScheduledSlowTask(ScheduledSlowTask* aTask,
                                                    uint32_t aMillis)
{
  TimeStamp expectedTime = FromNow(aMillis);
  ScheduledSlowTask* task = mScheduledSlowTasks.getFirst();
  while(task) {
    if (task->mExpectedTimeToRun > expectedTime) {
      task->setPrevious(aTask);
      break;	

    }
    task = task->getNext();	

  }
  if (!task) {
    mScheduledSlowTasks.insertBack(aTask);
  }
  aTask->mExpectedTimeToRun = expectedTime;
}

void
SlowAsynchronousTaskScheduler::CancelTimer()
{
  if (mTimer) {
    mTimer->Cancel();
    mTimerTime = TimeStamp();
  }
}