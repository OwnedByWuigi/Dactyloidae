/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_SlowAsynchronousTaskScheduler_h__
#define mozilla_SlowAsynchronousTaskScheduler_h__

#include "nsThreadUtils.h"
#include "nsITimer.h"	
#include "mozilla/LinkedList.h"
#include "mozilla/TimeStamp.h"

namespace mozilla {

struct SATSState
{
  MOZ_IMPLICIT SATSState(uint32_t aState)
  : mState(aState), mData(nullptr)
  {}

  MOZ_IMPLICIT SATSState(uint32_t aState, void* aData)
  : mState(aState), mData(aData)
  {}

  MOZ_IMPLICIT SATSState(const SATSState& aOther)
  : mState(aOther.mState), mData(aOther.mData)
  {}	

  uint32_t mState;
  void*    mData;

private:
  SATSState() = delete;
};

// Returns the ID of the next DependentSlowTask.
typedef SATSState (*SlowTaskCallback)(uint32_t aCurrentID, void* aData);

struct DependentSlowTask
{
  uint32_t mID;
  uint32_t mDelayMillis;
  SlowTaskCallback mCallback;
};

// If one has several tasks which need to run occasionally and those tasks
// depend on the each others, one can pass pointer to an array of
// DependentSlowTasks objects. Each callback returns the ID of the next state.
//
// enum {
//   eIDOfState1,
//   eIdOfState2,
//   eIdOfFinishState
// };
//
// static DependentSlowTask sMySlowTasks[]
// {
//  { eIDOfState1, 10000, TriggerState1 },
//  { eIdOfState2, 5000, TriggerState2 },
//  { eIdOfFinishState, 0, nullptr }
// };
// 
// scheduler->Schedule(sMySlowTasks);
class SlowAsynchronousTaskScheduler
{
public:
  SlowAsynchronousTaskScheduler();
  ~SlowAsynchronousTaskScheduler();

  // Call ExpectedTick() to give SATS hint when the next non-timer based Tick
  // might be called. For example refresh driver could call this.
  void ExpectedTick(uint32_t aMillisecondsFromNow);

  // Call Tick() to run possible pending scheduled tasks.
  void Tick()

  {
    Tick(false);
  }

  void TickFromTimer()
  {
    Tick(true);
  }

  void Schedule(nsIRunnable* aRunnable, uint32_t aMillis);
  void Schedule(DependentSlowTask* aTasks, void* aData = nullptr);
  void Schedule(DependentSlowTask* aTasks, uint32_t aID, void* aData = nullptr);

  void CancelScheduledTask(nsIRunnable* aRunnable);
  void CancelScheduledTask(DependentSlowTask* aTasks);
  void CancelScheduledTask(DependentSlowTask* aTasks, uint32_t aID);

  bool IsScheduled(nsIRunnable* aRunnable);
  bool IsScheduled(DependentSlowTask* aTasks);
  bool IsScheduled(DependentSlowTask* aTasks, uint32_t aID);
  void Shutdown()
  {
    CancelTimer();
    mTimer = nullptr;
    mShuttingDown = true;
  }
private:
  class ScheduledSlowTask : public LinkedListElement<ScheduledSlowTask>
  {
    public:
    ScheduledSlowTask(DependentSlowTask* aSlowTasks, uint32_t aIndex = 0,
                      void* aData = nullptr)
    : mSlowTasks(aSlowTasks)
    , mIndexOfNextTask(aIndex)
    , mDataForNextTask(aData)
    {
      MOZ_COUNT_CTOR(ScheduledSlowTask);
    }

    ScheduledSlowTask(nsIRunnable* aSlowTask)
    : mRunnable(aSlowTask)
    , mSlowTasks(nullptr)
    , mIndexOfNextTask(0)
    , mDataForNextTask(nullptr)
    {
      MOZ_COUNT_CTOR(ScheduledSlowTask);
    }

    ~ScheduledSlowTask()
    {
      MOZ_COUNT_DTOR(ScheduledSlowTask);
    }	

    mozilla::TimeStamp mExpectedTimeToRun;
    nsCOMPtr<nsIRunnable> mRunnable;
    DependentSlowTask* mSlowTasks;
    uint32_t mIndexOfNextTask;
    void* mDataForNextTask;
  };

  void Tick(bool aFromTimer);
  void AddScheduledSlowTask(ScheduledSlowTask* aTask, uint32_t aMillis);
  void EnsureTimer(uint32_t aMillis);
  void CancelTimer();
  static TimeStamp FromNow(uint32_t aMillis)
  {
    return TimeStamp::Now() +
           TimeDuration::FromMilliseconds(static_cast<double>(aMillis));
  }	

  LinkedList<ScheduledSlowTask> mScheduledSlowTasks;
  nsCOMPtr<nsITimer> mTimer;
  mozilla::TimeStamp mTimerTime;
  nsCOMPtr<nsICancelableRunnable> mTimerCallback;
  mozilla::TimeStamp mExpectedNonTimerTick;

  bool mShuttingDown;
};
}
#endif