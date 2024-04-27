/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsThreadUtils_h__
#define nsThreadUtils_h__

#include "prthread.h"
#include "prinrval.h"
#include "MainThreadUtils.h"
#include "nsIThreadManager.h"
#include "nsITimer.h"
#include "nsIThread.h"
#include "nsIRunnable.h"
#include "nsICancelableRunnable.h"
#include "nsIIdlePeriod.h"
#include "nsIIdleRunnable.h"
#include "nsStringGlue.h"
#include "nsCOMPtr.h"
#include "nsAutoPtr.h"
#include "mozilla/Atomics.h"
#include "mozilla/IndexSequence.h"
#include "mozilla/Likely.h"
#include "mozilla/Move.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/Tuple.h"
#include "mozilla/TypeTraits.h"

//-----------------------------------------------------------------------------
// These methods are alternatives to the methods on nsIThreadManager, provided
// for convenience.

/**
 * Set name of the target thread.  This operation is asynchronous.
 */
extern void NS_SetThreadName(nsIThread* aThread, const nsACString& aName);

/**
 * Static length version of the above function checking length of the
 * name at compile time.
 */
template<size_t LEN>
inline void
NS_SetThreadName(nsIThread* aThread, const char (&aName)[LEN])
{
  static_assert(LEN <= 16,
                "Thread name must be no more than 16 characters");
  NS_SetThreadName(aThread, nsDependentCString(aName));
}

/**
 * Create a new thread, and optionally provide an initial event for the thread.
 *
 * @param aResult
 *   The resulting nsIThread object.
 * @param aInitialEvent
 *   The initial event to run on this thread.  This parameter may be null.
 * @param aStackSize
 *   The size in bytes to reserve for the thread's stack.
 *
 * @returns NS_ERROR_INVALID_ARG
 *   Indicates that the given name is not unique.
 */
extern nsresult
NS_NewThread(nsIThread** aResult,
             nsIRunnable* aInitialEvent = nullptr,
             uint32_t aStackSize = nsIThreadManager::DEFAULT_STACK_SIZE);

/**
 * Creates a named thread, otherwise the same as NS_NewThread
 */
template<size_t LEN>
inline nsresult
NS_NewNamedThread(const char (&aName)[LEN],
                  nsIThread** aResult,
                  nsIRunnable* aInitialEvent = nullptr,
                  uint32_t aStackSize = nsIThreadManager::DEFAULT_STACK_SIZE)
{
  // Hold a ref while dispatching the initial event to match NS_NewThread()
  nsCOMPtr<nsIThread> thread;
  nsresult rv = NS_NewThread(getter_AddRefs(thread), nullptr, aStackSize);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  NS_SetThreadName<LEN>(thread, aName);
  if (aInitialEvent) {
    rv = thread->Dispatch(aInitialEvent, NS_DISPATCH_NORMAL);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "Initial event dispatch failed");
  }

  *aResult = nullptr;
  thread.swap(*aResult);
  return rv;
}

/**
 * Get a reference to the current thread.
 *
 * @param aResult
 *   The resulting nsIThread object.
 */
extern nsresult NS_GetCurrentThread(nsIThread** aResult);

/**
 * Dispatch the given event to the current thread.
 *
 * @param aEvent
 *   The event to dispatch.
 *
 * @returns NS_ERROR_INVALID_ARG
 *   If event is null.
 */
extern nsresult NS_DispatchToCurrentThread(nsIRunnable* aEvent);
extern nsresult
NS_DispatchToCurrentThread(already_AddRefed<nsIRunnable>&& aEvent);

/**
 * Dispatch the given event to the main thread.
 *
 * @param aEvent
 *   The event to dispatch.
 * @param aDispatchFlags
 *   The flags to pass to the main thread's dispatch method.
 *
 * @returns NS_ERROR_INVALID_ARG
 *   If event is null.
 */
extern nsresult
NS_DispatchToMainThread(nsIRunnable* aEvent,
                        uint32_t aDispatchFlags = NS_DISPATCH_NORMAL);
extern nsresult
NS_DispatchToMainThread(already_AddRefed<nsIRunnable>&& aEvent,
                        uint32_t aDispatchFlags = NS_DISPATCH_NORMAL);

extern nsresult
NS_DelayedDispatchToCurrentThread(
  already_AddRefed<nsIRunnable>&& aEvent, uint32_t aDelayMs);

/**
 * Dispatch the given event to the idle queue of the current thread.
 *
 * @param aEvent
 *   The event to dispatch.
 *
 * @returns NS_ERROR_INVALID_ARG
 *   If event is null.
 * @returns NS_ERROR_UNEXPECTED
 *   If the thread is shutting down.
 */

extern nsresult
NS_IdleDispatchToCurrentThread(already_AddRefed<nsIRunnable>&& aEvent);

/**
 * Dispatch the given event to the idle queue of the current thread.
 *
 * @param aEvent The event to dispatch. If the event implements
 *   nsIIdleRunnable, it will receive a call on
 *   nsIIdleRunnable::SetTimer when dispatched, with the value of
 *   aTimeout.
 *
 * @param aTimeout The time in milliseconds until the event should be
 *   moved from the idle queue to the regular queue, if it hasn't been
 *   executed. If aEvent is also an nsIIdleRunnable, it is expected
 *   that it should handle the timeout itself, after a call to
 *   nsIIdleRunnable::SetTimer.
 *
 * @returns NS_ERROR_INVALID_ARG
 *   If event is null.
 * @returns NS_ERROR_UNEXPECTED
 *   If the thread is shutting down.
 */
extern nsresult
NS_IdleDispatchToCurrentThread(already_AddRefed<nsIRunnable>&& aEvent, uint32_t aTimeout);

#ifndef XPCOM_GLUE_AVOID_NSPR
/**
 * Process all pending events for the given thread before returning.  This
 * method simply calls ProcessNextEvent on the thread while HasPendingEvents
 * continues to return true and the time spent in NS_ProcessPendingEvents
 * does not exceed the given timeout value.
 *
 * @param aThread
 *   The thread object for which to process pending events.  If null, then
 *   events will be processed for the current thread.
 * @param aTimeout
 *   The maximum number of milliseconds to spend processing pending events.
 *   Events are not pre-empted to honor this timeout.  Rather, the timeout
 *   value is simply used to determine whether or not to process another event.
 *   Pass PR_INTERVAL_NO_TIMEOUT to specify no timeout.
 */
extern nsresult
NS_ProcessPendingEvents(nsIThread* aThread,
                        PRIntervalTime aTimeout = PR_INTERVAL_NO_TIMEOUT);
#endif

/**
 * Shortcut for nsIThread::HasPendingEvents.
 *
 * It is an error to call this function when the given thread is not the
 * current thread.  This function will return false if called from some
 * other thread.
 *
 * @param aThread
 *   The current thread or null.
 *
 * @returns
 *   A boolean value that if "true" indicates that there are pending events
 *   in the current thread's event queue.
 */
extern bool NS_HasPendingEvents(nsIThread* aThread = nullptr);

/**
 * Shortcut for nsIThread::ProcessNextEvent.
 *
 * It is an error to call this function when the given thread is not the
 * current thread.  This function will simply return false if called
 * from some other thread.
 *
 * @param aThread
 *   The current thread or null.
 * @param aMayWait
 *   A boolean parameter that if "true" indicates that the method may block
 *   the calling thread to wait for a pending event.
 *
 * @returns
 *   A boolean value that if "true" indicates that an event from the current
 *   thread's event queue was processed.
 */
extern bool NS_ProcessNextEvent(nsIThread* aThread = nullptr,
                                bool aMayWait = true);

//-----------------------------------------------------------------------------
// Helpers that work with nsCOMPtr:

inline already_AddRefed<nsIThread>
do_GetCurrentThread()
{
  nsIThread* thread = nullptr;
  NS_GetCurrentThread(&thread);
  return already_AddRefed<nsIThread>(thread);
}

inline already_AddRefed<nsIThread>
do_GetMainThread()
{
  nsIThread* thread = nullptr;
  NS_GetMainThread(&thread);
  return already_AddRefed<nsIThread>(thread);
}

//-----------------------------------------------------------------------------

#ifdef MOZILLA_INTERNAL_API
// Fast access to the current thread.  Do not release the returned pointer!  If
// you want to use this pointer from some other thread, then you will need to
// AddRef it.  Otherwise, you should only consider this pointer valid from code
// running on the current thread.
extern nsIThread* NS_GetCurrentThread();
#endif

//-----------------------------------------------------------------------------

#ifndef XPCOM_GLUE_AVOID_NSPR

namespace mozilla {

// This class is designed to be subclassed.
class IdlePeriod : public nsIIdlePeriod
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIIDLEPERIOD

  IdlePeriod() {}

protected:
  virtual ~IdlePeriod() {}
private:
  IdlePeriod(const IdlePeriod&) = delete;
  IdlePeriod& operator=(const IdlePeriod&) = delete;
  IdlePeriod& operator=(const IdlePeriod&&) = delete;
};

// Cancelable runnable methods implement nsICancelableRunnable, and
// Idle and IdleWithTimer also nsIIdleRunnable.
enum RunnableKind
{
  Standard,
  Cancelable,
  Idle,
  IdleWithTimer
};

// This class is designed to be subclassed.
class Runnable : public nsIRunnable
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIRUNNABLE

  Runnable() {}

protected:
  virtual ~Runnable() {}
private:
  Runnable(const Runnable&) = delete;
  Runnable& operator=(const Runnable&) = delete;
  Runnable& operator=(const Runnable&&) = delete;
};

// This class is designed to be subclassed.
class CancelableRunnable : public Runnable,
                           public nsICancelableRunnable
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  // nsICancelableRunnable
  virtual nsresult Cancel() override;

  CancelableRunnable() {}

protected:
  virtual ~CancelableRunnable() {}
private:
  CancelableRunnable(const CancelableRunnable&) = delete;
  CancelableRunnable& operator=(const CancelableRunnable&) = delete;
  CancelableRunnable& operator=(const CancelableRunnable&&) = delete;
};

// This class is designed to be subclassed.
class IdleRunnable : public CancelableRunnable,
                     public nsIIdleRunnable
{
public:
  NS_DECL_ISUPPORTS_INHERITED

  IdleRunnable() {}
  explicit IdleRunnable(const char* aName) : CancelableRunnable(aName) {}

protected:
  virtual ~IdleRunnable() {}
private:
  IdleRunnable(const IdleRunnable&) = delete;
  IdleRunnable& operator=(const IdleRunnable&) = delete;
  IdleRunnable& operator=(const IdleRunnable&&) = delete;
};

namespace detail {

// An event that can be used to call a C++11 functions or function objects,
// including lambdas. The function must have no required arguments, and must
// return void.
template<typename StoredFunction>
class RunnableFunction : public Runnable
{
public:
  template <typename F>
  explicit RunnableFunction(F&& aFunction)
    : mFunction(Forward<F>(aFunction))
  { }

  NS_IMETHOD Run() override {
    static_assert(IsVoid<decltype(mFunction())>::value,
                  "The lambda must return void!");
    mFunction();
    return NS_OK;
  }
private:
  StoredFunction mFunction;
};

} // namespace detail

} // namespace mozilla

template<typename Function>
already_AddRefed<mozilla::Runnable>
NS_NewRunnableFunction(Function&& aFunction)
{
  return do_AddRef(new mozilla::detail::RunnableFunction
                   // Make sure we store a non-reference in nsRunnableFunction.
                   <typename mozilla::RemoveReference<Function>::Type>
                   // But still forward aFunction to move if possible.
                   (mozilla::Forward<Function>(aFunction)));
}

namespace mozilla {
namespace detail {

already_AddRefed<nsITimer> CreateTimer();

template <RunnableKind Kind>
class TimerBehaviour
{
public:
  nsITimer* GetTimer() { return nullptr; }
  void CancelTimer() {}

protected:
  ~TimerBehaviour() {}
};

template <>
class TimerBehaviour<IdleWithTimer>
{
public:
  nsITimer* GetTimer()
  {
    if (!mTimer) {
      mTimer = CreateTimer();
    }

    return mTimer;
  }

  void CancelTimer()
  {
    if (mTimer) {
      mTimer->Cancel();
    }
  }

protected:
  ~TimerBehaviour()
  {
    CancelTimer();
  }
private:
  nsCOMPtr<nsITimer> mTimer;
};

} // namespace detail
} // namespace mozilla

// An event that can be used to call a method on a class.  The class type must
// support reference counting. This event supports Revoke for use
// with nsRevocableEventPtr.
template<class ClassType,
         typename ReturnType = void,
         bool Owning = true,
         mozilla::RunnableKind Kind = mozilla::Standard>
class nsRunnableMethod
  : public mozilla::Conditional<Kind == mozilla::Standard,
                                mozilla::Runnable,
                                typename mozilla::Conditional<
                                  Kind == mozilla::Cancelable,
                                  mozilla::CancelableRunnable,
                                  mozilla::IdleRunnable>::Type>::Type,
    protected mozilla::detail::TimerBehaviour<Kind>
{
public:
  virtual void Revoke() = 0;

  // These ReturnTypeEnforcer classes set up a blacklist for return types that
  // we know are not safe. The default ReturnTypeEnforcer compiles just fine but
  // already_AddRefed will not.
  template<typename OtherReturnType>
  class ReturnTypeEnforcer
  {
  public:
    typedef int ReturnTypeIsSafe;
  };

  template<class T>
  class ReturnTypeEnforcer<already_AddRefed<T>>
  {
    // No ReturnTypeIsSafe makes this illegal!
  };

  // Make sure this return type is safe.
  typedef typename ReturnTypeEnforcer<ReturnType>::ReturnTypeIsSafe check;
};

template<class ClassType, bool Owning, bool Idle>
struct nsRunnableMethodReceiver
{
  RefPtr<ClassType> mObj;
  explicit nsRunnableMethodReceiver(ClassType* aObj) : mObj(aObj) {}
  ~nsRunnableMethodReceiver() { Revoke(); }
  ClassType* Get() const { return mObj.get(); }
  void Revoke() { mObj = nullptr; }
  void SetDeadline(mozilla::TimeStamp aDeadline) { if (mObj) mObj->SetDeadline(aDeadline); }
};

template<class ClassType>
struct nsRunnableMethodReceiver<ClassType, false, false>
{
  ClassType* MOZ_NON_OWNING_REF mObj;
  explicit nsRunnableMethodReceiver(ClassType* aObj) : mObj(aObj) {}
  ClassType* Get() const { return mObj; }
  void Revoke() { mObj = nullptr; }
  void SetDeadline(mozilla::TimeStamp aDeadline) {}
};

template<class ClassType>
struct nsRunnableMethodReceiver<ClassType, false, true>
{
  ClassType* MOZ_NON_OWNING_REF mObj;
  explicit nsRunnableMethodReceiver(ClassType* aObj) : mObj(aObj) {}
  ClassType* Get() const { return mObj; }
  void Revoke() { mObj = nullptr; }
  void SetDeadline(mozilla::TimeStamp aDeadline) { if (mObj) mObj->SetDeadline(aDeadline); }
};

static inline constexpr bool
IsIdle(mozilla::RunnableKind aKind)
{
  return aKind == mozilla::Idle || aKind == mozilla::IdleWithTimer;
}

template<class ClassType>
struct nsRunnableMethodReceiver<ClassType, true, false>
{
  RefPtr<ClassType> mObj;
  explicit nsRunnableMethodReceiver(ClassType* aObj) : mObj(aObj) {}
  ~nsRunnableMethodReceiver() { Revoke(); }
  ClassType* Get() const { return mObj.get(); }
  void Revoke() { mObj = nullptr; }
  void SetDeadline(mozilla::TimeStamp aDeadline) {}
};

template<typename PtrType, typename Method, bool Owning, mozilla::RunnableKind Kind>
struct nsRunnableMethodTraits;

template<class C, typename R, bool Owning, mozilla::RunnableKind Kind, typename... As>
struct nsRunnableMethodTraits<R(C::*)(As...), Owning, Kind>
{
  typedef C class_type;
  typedef R return_type;
  typedef nsRunnableMethod<C, R, Owning, Kind> base_type;
  static const bool can_cancel = Kind == mozilla::Cancelable;
};

template<class C, typename R, bool Owning, mozilla::RunnableKind Kind, typename... As>
struct nsRunnableMethodTraits<R(C::*)(As...) const, Owning, Kind>
{
  typedef const C class_type;
  typedef R return_type;
  typedef nsRunnableMethod<C, R, Owning, Kind> base_type;
  static const bool can_cancel = Kind == mozilla::Cancelable;
};

#ifdef NS_HAVE_STDCALL
template<class C, typename R, bool Owning, mozilla::RunnableKind Kind, typename... As>
struct nsRunnableMethodTraits<R(__stdcall C::*)(As...), Owning, Kind>
{
  typedef C class_type;
  typedef R return_type;
  typedef nsRunnableMethod<C, R, Owning, Kind> base_type;
  static const bool can_cancel = Kind == mozilla::Cancelable;
};

template<class C, typename R, bool Owning, mozilla::RunnableKind Kind,>
struct nsRunnableMethodTraits<R(NS_STDCALL C::*)(), Owning, Kind>
{
  typedef C class_type;
  typedef R return_type;
  typedef nsRunnableMethod<C, R, Owning, Kind> base_type;
  static const bool can_cancel = Kind == mozilla::Cancelable
};
template<class C, typename R, bool Owning, mozilla::RunnableKind Kind, typename... As>
struct nsRunnableMethodTraits<R(__stdcall C::*)(As...) const, Owning, Kind>
{
  typedef const C class_type;
  typedef R return_type;
  typedef nsRunnableMethod<C, R, Owning, Kind> base_type;
  static const bool can_cancel = Kind == mozilla::Cancelable
};

template<class C, typename R, bool Owning, mozilla::RunnableKind Kind>
struct nsRunnableMethodTraits<R(NS_STDCALL C::*)() const, Owning, Cancelable>
{
  typedef const C class_type;
  typedef R return_type;
  typedef nsRunnableMethod<C, R, Owning, Kind> base_type;
  static const bool can_cancel = Kind == mozilla::Cancelable;
};
#endif


// IsParameterStorageClass<T>::value is true if T is a parameter-storage class
// that will be recognized by NS_New[NonOwning]RunnableMethodWithArg[s] to
// force a specific storage&passing strategy (instead of inferring one,
// see ParameterStorage).
// When creating a new storage class, add a specialization for it to be
// recognized.
template<typename T>
struct IsParameterStorageClass : public mozilla::FalseType {};

// StoreXPassByY structs used to inform nsRunnableMethodArguments how to
// store arguments, and how to pass them to the target method.

template<typename T>
struct StoreCopyPassByValue
{
  typedef T stored_type;
  typedef T passed_type;
  stored_type m;
  template <typename A>
  MOZ_IMPLICIT StoreCopyPassByValue(A&& a) : m(mozilla::Forward<A>(a)) {}
  passed_type PassAsParameter() { return m; }
};
template<typename S>
struct IsParameterStorageClass<StoreCopyPassByValue<S>>
  : public mozilla::TrueType {};

template<typename T>
struct StoreCopyPassByConstLRef
{
  typedef T stored_type;
  typedef const T& passed_type;
  stored_type m;
  template <typename A>
  MOZ_IMPLICIT StoreCopyPassByConstLRef(A&& a) : m(mozilla::Forward<A>(a)) {}
  passed_type PassAsParameter() { return m; }
};
template<typename S>
struct IsParameterStorageClass<StoreCopyPassByConstLRef<S>>
  : public mozilla::TrueType {};

template<typename T>
struct StoreCopyPassByLRef
{
  typedef T stored_type;
  typedef T& passed_type;
  stored_type m;
  template <typename A>
  MOZ_IMPLICIT StoreCopyPassByLRef(A&& a) : m(mozilla::Forward<A>(a)) {}
  passed_type PassAsParameter() { return m; }
};
template<typename S>
struct IsParameterStorageClass<StoreCopyPassByLRef<S>>
  : public mozilla::TrueType {};

template<typename T>
struct StoreCopyPassByRRef
{
  typedef T stored_type;
  typedef T&& passed_type;
  stored_type m;
  template <typename A>
  MOZ_IMPLICIT StoreCopyPassByRRef(A&& a) : m(mozilla::Forward<A>(a)) {}
  passed_type PassAsParameter() { return mozilla::Move(m); }
};
template<typename S>
struct IsParameterStorageClass<StoreCopyPassByRRef<S>>
  : public mozilla::TrueType {};

template<typename T>
struct StoreRefPassByLRef
{
  typedef T& stored_type;
  typedef T& passed_type;
  stored_type m;
  template <typename A>
  MOZ_IMPLICIT StoreRefPassByLRef(A& a) : m(a) {}
  passed_type PassAsParameter() { return m; }
};
template<typename S>
struct IsParameterStorageClass<StoreRefPassByLRef<S>>
  : public mozilla::TrueType {};

template<typename T>
struct StoreConstRefPassByConstLRef
{
  typedef const T& stored_type;
  typedef const T& passed_type;
  stored_type m;
  template <typename A>
  MOZ_IMPLICIT StoreConstRefPassByConstLRef(const A& a) : m(a) {}
  passed_type PassAsParameter() { return m; }
};
template<typename S>
struct IsParameterStorageClass<StoreConstRefPassByConstLRef<S>>
  : public mozilla::TrueType {};

template<typename T>
struct StorensRefPtrPassByPtr
{
  typedef RefPtr<T> stored_type;
  typedef T* passed_type;
  stored_type m;
  template <typename A>
  MOZ_IMPLICIT StorensRefPtrPassByPtr(A&& a) : m(mozilla::Forward<A>(a)) {}
  passed_type PassAsParameter() { return m.get(); }
};
template<typename S>
struct IsParameterStorageClass<StorensRefPtrPassByPtr<S>>
  : public mozilla::TrueType {};

template<typename T>
struct StorePtrPassByPtr
{
  typedef T* stored_type;
  typedef T* passed_type;
  stored_type m;
  template <typename A>
  MOZ_IMPLICIT StorePtrPassByPtr(A a) : m(a) {}
  passed_type PassAsParameter() { return m; }
};
template<typename S>
struct IsParameterStorageClass<StorePtrPassByPtr<S>>
  : public mozilla::TrueType {};

template<typename T>
struct StoreConstPtrPassByConstPtr
{
  typedef const T* stored_type;
  typedef const T* passed_type;
  stored_type m;
  template <typename A>
  MOZ_IMPLICIT StoreConstPtrPassByConstPtr(A a) : m(a) {}
  passed_type PassAsParameter() { return m; }
};
template<typename S>
struct IsParameterStorageClass<StoreConstPtrPassByConstPtr<S>>
  : public mozilla::TrueType {};

template<typename T>
struct StoreCopyPassByConstPtr
{
  typedef T stored_type;
  typedef const T* passed_type;
  stored_type m;
  template <typename A>
  MOZ_IMPLICIT StoreCopyPassByConstPtr(A&& a) : m(mozilla::Forward<A>(a)) {}
  passed_type PassAsParameter() { return &m; }
};
template<typename S>
struct IsParameterStorageClass<StoreCopyPassByConstPtr<S>>
  : public mozilla::TrueType {};

template<typename T>
struct StoreCopyPassByPtr
{
  typedef T stored_type;
  typedef T* passed_type;
  stored_type m;
  template <typename A>
  MOZ_IMPLICIT StoreCopyPassByPtr(A&& a) : m(mozilla::Forward<A>(a)) {}
  passed_type PassAsParameter() { return &m; }
};
template<typename S>
struct IsParameterStorageClass<StoreCopyPassByPtr<S>>
  : public mozilla::TrueType {};

namespace detail {

template<typename TWithoutPointer>
struct NonnsISupportsPointerStorageClass
  : mozilla::Conditional<mozilla::IsConst<TWithoutPointer>::value,
                         StoreConstPtrPassByConstPtr<
                           typename mozilla::RemoveConst<TWithoutPointer>::Type>,
                         StorePtrPassByPtr<TWithoutPointer>>
{};

template<typename>
struct SFINAE1True : mozilla::TrueType
{};

template<class T>
static auto HasRefCountMethodsTest(int)
    -> SFINAE1True<decltype(mozilla::DeclVal<T>().AddRef(),
                            mozilla::DeclVal<T>().Release())>;
template<class>
static auto HasRefCountMethodsTest(long) -> mozilla::FalseType;

template<class T>
struct HasRefCountMethods : decltype(HasRefCountMethodsTest<T>(0))
{};

template<typename T>
struct IsRefcountedSmartPointer : public mozilla::FalseType
{};

template<typename T>
struct IsRefcountedSmartPointer<RefPtr<T>> : public mozilla::TrueType
{};

template<typename T>
struct IsRefcountedSmartPointer<nsCOMPtr<T>> : public mozilla::TrueType
{};

template<typename T>
struct StripSmartPointer
{
  typedef void Type;
};

template<typename T>
struct StripSmartPointer<RefPtr<T>>
{
  typedef T Type;
};

template<typename T>
struct StripSmartPointer<nsCOMPtr<T>>
{
  typedef T Type;
};

template<typename TWithoutPointer>
struct PointerStorageClass
  : mozilla::Conditional<HasRefCountMethods<TWithoutPointer>::value,
                         StorensRefPtrPassByPtr<TWithoutPointer>,
                         typename NonnsISupportsPointerStorageClass<
                           TWithoutPointer
                         >::Type>
{};

template<typename TWithoutRef>
struct LValueReferenceStorageClass
  : mozilla::Conditional<mozilla::IsConst<TWithoutRef>::value,
                         StoreConstRefPassByConstLRef<
                           typename mozilla::RemoveConst<TWithoutRef>::Type>,
                         StoreRefPassByLRef<TWithoutRef>>
{};

template<typename T>
struct SmartPointerStorageClass
  : mozilla::Conditional<IsRefcountedSmartPointer<T>::value,
                         StorensRefPtrPassByPtr<
                           typename StripSmartPointer<T>::Type>,
                         StoreCopyPassByValue<T>>
{};

template<typename T>
struct NonLValueReferenceStorageClass
  : mozilla::Conditional<mozilla::IsRvalueReference<T>::value,
                         StoreCopyPassByRRef<
                           typename mozilla::RemoveReference<T>::Type>,
                         typename SmartPointerStorageClass<T>::Type>
{};

template<typename T>
struct NonPointerStorageClass
  : mozilla::Conditional<mozilla::IsLvalueReference<T>::value,
                         typename LValueReferenceStorageClass<
                           typename mozilla::RemoveReference<T>::Type
                         >::Type,
                         typename NonLValueReferenceStorageClass<T>::Type>
{};

template<typename T>
struct NonParameterStorageClass
  : mozilla::Conditional<mozilla::IsPointer<T>::value,
                         typename PointerStorageClass<
                           typename mozilla::RemovePointer<T>::Type
                         >::Type,
                         typename NonPointerStorageClass<T>::Type>
{};

// Choose storage&passing strategy based on preferred storage type:
// - If IsParameterStorageClass<T>::value is true, use as-is.
// - RC*       -> StorensRefPtrPassByPtr<RC>     : Store RefPtr<RC>, pass RC*
//   ^^ RC quacks like a ref-counted type (i.e., has AddRef and Release methods)
// - const T*  -> StoreConstPtrPassByConstPtr<T> : Store const T*, pass const T*
// - T*        -> StorePtrPassByPtr<T>           : Store T*, pass T*.
// - const T&  -> StoreConstRefPassByConstLRef<T>: Store const T&, pass const T&.
// - T&        -> StoreRefPassByLRef<T>          : Store T&, pass T&.
// - T&&       -> StoreCopyPassByRRef<T>         : Store T, pass Move(T).
// - RefPtr<T>, nsCOMPtr<T>
//             -> StorensRefPtrPassByPtr<T>      : Store RefPtr<T>, pass T*
// - Other T   -> StoreCopyPassByValue<T>        : Store T, pass T.
// Other available explicit options:
// -              StoreCopyPassByConstLRef<T>    : Store T, pass const T&.
// -              StoreCopyPassByLRef<T>         : Store T, pass T& (of copy!)
// -              StoreCopyPassByConstPtr<T>     : Store T, pass const T*
// -              StoreCopyPassByPtr<T>          : Store T, pass T* (of copy!)
// Or create your own class with PassAsParameter() method, optional
// clean-up in destructor, and with associated IsParameterStorageClass<>.
template<typename T>
struct ParameterStorage
  : mozilla::Conditional<IsParameterStorageClass<T>::value,
                         T,
                         typename NonParameterStorageClass<T>::Type>
{};

} /* namespace detail */

namespace mozilla {

namespace detail {

// struct used to store arguments and later apply them to a method.
template <typename... Ts>
struct RunnableMethodArguments final
{
  Tuple<typename ::detail::ParameterStorage<Ts>::Type...> mArguments;
  template <typename... As>
  explicit RunnableMethodArguments(As&&... aArguments)
    : mArguments(Forward<As>(aArguments)...)
  {}
  template<typename C, typename M, typename... Args, size_t... Indices>
  static auto
  applyImpl(C* o, M m, Tuple<Args...>& args, IndexSequence<Indices...>)
      -> decltype(((*o).*m)(Get<Indices>(args).PassAsParameter()...))
  {
    return ((*o).*m)(Get<Indices>(args).PassAsParameter()...);
  }
  template<class C, typename M> auto apply(C* o, M m)
      -> decltype(applyImpl(o, m, mArguments,
                  typename IndexSequenceFor<Ts...>::Type()))
  {
    return applyImpl(o, m, mArguments,
        typename IndexSequenceFor<Ts...>::Type());
  }
};

template<typename Method, bool Owning, RunnableKind Kind, typename... Storages>
class RunnableMethodImpl final
  : public ::nsRunnableMethodTraits<Method, Owning, Kind>::base_type
{
  typedef typename Traits::class_type ClassType;
  typedef typename Traits::base_type BaseType;
  ::nsRunnableMethodReceiver<ClassType, Owning, IsIdle(Kind)> mReceiver;

  Method mMethod;
  RunnableMethodArguments<Storages...> mArgs;
  using BaseType::GetTimer;
  using BaseType::CancelTimer;
private:
  virtual ~RunnableMethodImpl() { Revoke(); };
  static void TimedOut(nsITimer* aTimer, void* aClosure)
  {
    static_assert(IsIdle(Kind), "Don't use me!");
    RefPtr<IdleRunnable> r = static_cast<IdleRunnable*>(aClosure);
    r->SetDeadline(TimeStamp());
    r->Run();
    r->Cancel();
  }
public:
  template<typename... Args>
  explicit RunnableMethodImpl(ClassType* aObj, Method aMethod,
                              Args&&... aArgs)
    : mReceiver(aObj)
    , mMethod(aMethod)
    , mArgs(Forward<Args>(aArgs)...)
  {
    static_assert(sizeof...(Storages) == sizeof...(Args), "Storages and Args should have equal sizes");
  }
  NS_IMETHOD Run()
  {
    CancelTimer();
    if (MOZ_LIKELY(mReceiver.Get())) {
      mArgs.apply(mReceiver.Get(), mMethod);
    }
    return NS_OK;
  }
  nsresult Cancel()
  {
    static_assert(Kind >= Cancelable, "Don't use me!");
    Revoke();
    return NS_OK;
  }
  void Revoke()
  {
    CancelTimer();
    mReceiver.Revoke();
  }

  void SetDeadline(TimeStamp aDeadline)
  {
    mReceiver.SetDeadline(aDeadline);
  }

  void SetTimer(uint32_t aDelay, nsIEventTarget* aTarget)
  {
    MOZ_ASSERT(aTarget);

    if (nsCOMPtr<nsITimer> timer = GetTimer()) {
      timer->Cancel();
      timer->SetTarget(aTarget);
      timer->InitWithFuncCallback(TimedOut, this, aDelay,
                                  nsITimer::TYPE_ONE_SHOT);
    }
  }
};

// Type aliases for NewRunnableMethod.
template<typename PtrType, typename Method>
using OwningRunnableMethod = typename ::nsRunnableMethodTraits<
  typename RemoveReference<PtrType>::Type, Method, true, Standard>::base_type;
template<typename PtrType, typename Method, typename... Storages>
using OwningRunnableMethodImpl = RunnableMethodImpl<
  typename RemoveReference<PtrType>::Type, Method, true, Standard, Storages...>;

// Type aliases for NewCancelableRunnableMethod.
template<typename PtrType, typename Method>
using CancelableRunnableMethod = typename ::nsRunnableMethodTraits<
  typename RemoveReference<PtrType>::Type, Method, true, Cancelable>::base_type;
template<typename PtrType, typename Method, typename... Storages>
using CancelableRunnableMethodImpl = RunnableMethodImpl<
  typename RemoveReference<PtrType>::Type, Method, true, Cancelable, Storages...>;

// Type aliases for NewIdleRunnableMethod.
template<typename PtrType, typename Method>
using IdleRunnableMethod = typename ::nsRunnableMethodTraits<
  typename RemoveReference<PtrType>::Type, Method, true, Idle>::base_type;
template<typename PtrType, typename Method, typename... Storages>
using IdleRunnableMethodImpl = RunnableMethodImpl<
  typename RemoveReference<PtrType>::Type, Method, true, Idle, Storages...>;

// Type aliases for NewIdleRunnableMethodWithTimer.
template<typename PtrType, typename Method>
using IdleRunnableMethodWithTimer = typename ::nsRunnableMethodTraits<
  typename RemoveReference<PtrType>::Type, Method, true, IdleWithTimer>::base_type;
template<typename PtrType, typename Method, typename... Storages>
using IdleRunnableMethodWithTimerImpl = RunnableMethodImpl<
  typename RemoveReference<PtrType>::Type, Method, true, IdleWithTimer, Storages...>;

// Type aliases for NewNonOwningRunnableMethod.
template<typename PtrType, typename Method>
using NonOwningRunnableMethod = typename ::nsRunnableMethodTraits<
  typename RemoveReference<PtrType>::Type, Method, false, Standard>::base_type;
template<typename PtrType, typename Method, typename... Storages>
using NonOwningRunnableMethodImpl = RunnableMethodImpl<
  typename RemoveReference<PtrType>::Type, Method, false, Standard, Storages...>;

// Type aliases for NonOwningCancelableRunnableMethod
template<typename PtrType, typename Method>
using NonOwningCancelableRunnableMethod = typename ::nsRunnableMethodTraits<
  typename RemoveReference<PtrType>::Type, Method, false, Cancelable>::base_type;
template<typename PtrType, typename Method, typename... Storages>
using NonOwningCancelableRunnableMethodImpl = RunnableMethodImpl<
  typename RemoveReference<PtrType>::Type, Method, false, Cancelable, Storages...>;

// Type aliases for NonOwningIdleRunnableMethod
template<typename PtrType, typename Method>
using NonOwningIdleRunnableMethod = typename ::nsRunnableMethodTraits<
  typename RemoveReference<PtrType>::Type, Method, false, Idle>::base_type;
template<typename PtrType, typename Method, typename... Storages>
using NonOwningIdleRunnableMethodImpl = RunnableMethodImpl<
  typename RemoveReference<PtrType>::Type, Method, false, Idle, Storages...>;

// Type aliases for NewIdleRunnableMethodWithTimer.
template<typename PtrType, typename Method>
using NonOwningIdleRunnableMethodWithTimer = typename ::nsRunnableMethodTraits<
  typename RemoveReference<PtrType>::Type, Method, false, IdleWithTimer>::base_type;
template<typename PtrType, typename Method, typename... Storages>
using NonOwningIdleRunnableMethodWithTimerImpl = RunnableMethodImpl<
  typename RemoveReference<PtrType>::Type, Method, false, IdleWithTimer, Storages...>;
} // namespace detail

// Use this template function like so:
//
//   nsCOMPtr<nsIRunnable> event =
//     mozilla::NewRunnableMethod(myObject, &MyClass::HandleEvent);
//   NS_DispatchToCurrentThread(event);
//
// Statically enforced constraints:
//  - myObject must be of (or implicitly convertible to) type MyClass
//  - MyClass must define AddRef and Release methods
//

template<typename PtrType, typename Method>
already_AddRefed<typename ::nsRunnableMethodTraits<Method, true, false>::base_type>
NewRunnableMethod(PtrType aPtr, Method aMethod)
{
  return do_AddRef(new detail::RunnableMethodImpl<Method, true, false>(aPtr, aMethod));
}

template<typename PtrType, typename Method>
already_AddRefed<typename ::nsRunnableMethodTraits<Method, true, true>::base_type>
NewCancelableRunnableMethod(PtrType aPtr, Method aMethod)
{
  return do_AddRef(new detail::RunnableMethodImpl<Method, true, true>(aPtr, aMethod));
}

already_AddRefed<detail::IdleRunnableMethod<PtrType, Method>>
NewIdleRunnableMethod(PtrType&& aPtr, Method aMethod)
{
  return do_AddRef(new detail::IdleRunnableMethodImpl<PtrType, Method>(
    Forward<PtrType>(aPtr), aMethod));
}

template<typename PtrType, typename Method>
already_AddRefed<detail::IdleRunnableMethod<PtrType, Method>>
NewIdleRunnableMethod(const char* aName, PtrType&& aPtr, Method aMethod)
{
  return detail::SetRunnableName(
    NewIdleRunnableMethod(Forward<PtrType>(aPtr), aMethod), aName);
}

template<typename PtrType, typename Method>
already_AddRefed<detail::IdleRunnableMethodWithTimer<PtrType, Method>>
NewIdleRunnableMethodWithTimer(PtrType&& aPtr, Method aMethod)
{
  return do_AddRef(new detail::IdleRunnableMethodWithTimerImpl<PtrType, Method>(
    Forward<PtrType>(aPtr), aMethod));
}

template<typename PtrType, typename Method>
already_AddRefed<detail::IdleRunnableMethodWithTimer<PtrType, Method>>
NewIdleRunnableMethodWithTimer(const char* aName,
                               PtrType&& aPtr,
                               Method aMethod)
{
  return detail::SetRunnableName(
    NewIdleRunnableMethodWithTimer(Forward<PtrType>(aPtr), aMethod),
    aName);
}


template<typename PtrType, typename Method>
already_AddRefed<typename ::nsRunnableMethodTraits<Method, false, false>::base_type>
NewNonOwningRunnableMethod(PtrType&& aPtr, Method aMethod)
{
  return do_AddRef(new detail::RunnableMethodImpl<Method, false, false>(aPtr, aMethod));
}

template<typename PtrType, typename Method>
already_AddRefed<typename ::nsRunnableMethodTraits<Method, false, true>::base_type>
NewNonOwningCancelableRunnableMethod(PtrType&& aPtr, Method aMethod)
{
  return do_AddRef(new detail::RunnableMethodImpl<Method, false, true>(aPtr, aMethod));
}


template<typename PtrType, typename Method>
already_AddRefed<detail::NonOwningIdleRunnableMethod<PtrType, Method>>
NewNonOwningIdleRunnableMethod(PtrType&& aPtr, Method aMethod)
{
  return do_AddRef(
    new detail::NonOwningIdleRunnableMethodImpl<PtrType, Method>(
      Forward<PtrType>(aPtr), aMethod));
}

template<typename PtrType, typename Method>
already_AddRefed<detail::NonOwningIdleRunnableMethod<PtrType, Method>>
NewNonOwningIdleRunnableMethod(const char* aName,
                               PtrType&& aPtr,
                               Method aMethod)
{
  return detail::SetRunnableName(
    NewNonOwningIdleRunnableMethod(Forward<PtrType>(aPtr), aMethod), aName);
}

template<typename PtrType, typename Method>
already_AddRefed<detail::NonOwningIdleRunnableMethodWithTimer<PtrType, Method>>
NewNonOwningIdleRunnableMethodWithTimer(PtrType&& aPtr,
                                        Method aMethod)
{
  return do_AddRef(
      new detail::NonOwningIdleRunnableMethodWithTimerImpl<PtrType, Method>(
        Forward<PtrType>(aPtr), aMethod));
}

template<typename PtrType, typename Method>
already_AddRefed<detail::NonOwningIdleRunnableMethodWithTimer<PtrType, Method>>
NewNonOwningIdleRunnableMethodWithTimer(const char* aName,
                                        PtrType&& aPtr,
                                        Method aMethod)
{
  return detail::SetRunnableName(NewNonOwningIdleRunnableMethodWithTimer(
                                   Forward<PtrType>(aPtr), aMethod),
                                 aName);

// Similar to NewRunnableMethod. Call like so:
// nsCOMPtr<nsIRunnable> event =
//   NewRunnableMethod<Types,...>(myObject, &MyClass::HandleEvent, myArg1,...);
// 'Types' are the stored type for each argument, see ParameterStorage for details.
template<typename... Storages, typename Method, typename PtrType, typename... Args>
already_AddRefed<typename ::nsRunnableMethodTraits<Method, true, false>::base_type>
NewRunnableMethod(PtrType&& aPtr, Method aMethod, Args&&... aArgs)
{
  static_assert(sizeof...(Storages) == sizeof...(Args),
                "<Storages...> size should be equal to number of arguments");
  return do_AddRef(new detail::RunnableMethodImpl<Method, true, false, Storages...>(
      aPtr, aMethod, mozilla::Forward<Args>(aArgs)...));
}

template<typename... Storages, typename Method, typename PtrType, typename... Args>
already_AddRefed<typename ::nsRunnableMethodTraits<Method, false, false>::base_type>
NewNonOwningRunnableMethod(PtrType&& aPtr, Method aMethod, Args&&... aArgs)
{
  static_assert(sizeof...(Storages) == sizeof...(Args),
                "<Storages...> size should be equal to number of arguments");
  return do_AddRef(new detail::RunnableMethodImpl<Method, false, false, Storages...>(
      aPtr, aMethod, mozilla::Forward<Args>(aArgs)...));
}

template<typename... Storages, typename Method, typename PtrType, typename... Args>
already_AddRefed<typename ::nsRunnableMethodTraits<Method, true, true>::base_type>
NewCancelableRunnableMethod(PtrType&& aPtr, Method aMethod, Args&&... aArgs)
{
  static_assert(sizeof...(Storages) == sizeof...(Args),
                "<Storages...> size should be equal to number of arguments");
  return do_AddRef(new detail::RunnableMethodImpl<Method, true, true, Storages...>(
      aPtr, aMethod, mozilla::Forward<Args>(aArgs)...));
}

template<typename... Storages, typename Method, typename PtrType, typename... Args>
already_AddRefed<typename ::nsRunnableMethodTraits<Method, false, true>::base_type>
NewNonOwningCancelableRunnableMethod(PtrType&& aPtr, Method aMethod,
                                                Args&&... aArgs)
{
  static_assert(sizeof...(Storages) == sizeof...(Args),
                "<Storages...> size should be equal to number of arguments");
  return do_AddRef(new detail::RunnableMethodImpl<Method, false, true, Storages...>(
      aPtr, aMethod, mozilla::Forward<Args>(aArgs)...));
}


template<typename... Storages,
         typename PtrType,
         typename Method,
         typename... Args>
already_AddRefed<detail::IdleRunnableMethod<PtrType, Method>>
NewIdleRunnableMethod(PtrType&& aPtr, Method aMethod, Args&&... aArgs)
{
  static_assert(sizeof...(Storages) == sizeof...(Args),
                "<Storages...> size should be equal to number of arguments");
  return do_AddRef(
    new detail::IdleRunnableMethodImpl<PtrType, Method, Storages...>(
      Forward<PtrType>(aPtr), aMethod, mozilla::Forward<Args>(aArgs)...));
}

template<typename... Storages,
         typename PtrType,
         typename Method,
         typename... Args>
already_AddRefed<detail::IdleRunnableMethod<PtrType, Method>>
NewIdleRunnableMethod(const char* aName,
                      PtrType&& aPtr,
                      Method aMethod,
                      Args&&... aArgs)
{
  static_assert(sizeof...(Storages) == sizeof...(Args),
                "<Storages...> size should be equal to number of arguments");
  return detail::SetRunnableName(
    NewIdleRunnableMethod<Storages...>(
      Forward<PtrType>(aPtr), aMethod, mozilla::Forward<Args>(aArgs)...),
    aName);
}

template<typename... Storages,
         typename PtrType,
         typename Method,
         typename... Args>
already_AddRefed<detail::NonOwningIdleRunnableMethod<PtrType, Method>>
NewNonOwningIdleRunnableMethod(PtrType&& aPtr, Method aMethod, Args&&... aArgs)
{
  static_assert(sizeof...(Storages) == sizeof...(Args),
                "<Storages...> size should be equal to number of arguments");
  return do_AddRef(
    new detail::NonOwningIdleRunnableMethodImpl<PtrType, Method, Storages...>(
      Forward<PtrType>(aPtr), aMethod, mozilla::Forward<Args>(aArgs)...));
}

template<typename... Storages,
         typename PtrType,
         typename Method,
         typename... Args>
already_AddRefed<detail::NonOwningIdleRunnableMethod<PtrType, Method>>
NewNonOwningIdleRunnableMethod(const char* aName,
                               PtrType&& aPtr,
                               Method aMethod,
                               Args&&... aArgs)
{
  static_assert(sizeof...(Storages) == sizeof...(Args),
                "<Storages...> size should be equal to number of arguments");
  return detail::SetRunnableName(
    NewNonOwningIdleRunnableMethod<Storages...>(
      Forward<PtrType>(aPtr), aMethod, mozilla::Forward<Args>(aArgs)...),
    aName);
}

} // namespace mozilla

#endif  // XPCOM_GLUE_AVOID_NSPR

// This class is designed to be used when you have an event class E that has a
// pointer back to resource class R.  If R goes away while E is still pending,
// then it is important to "revoke" E so that it does not try use R after R has
// been destroyed.  nsRevocableEventPtr makes it easy for R to manage such
// situations:
//
//   class R;
//
//   class E : public mozilla::Runnable {
//   public:
//     void Revoke() {
//       mResource = nullptr;
//     }
//   private:
//     R *mResource;
//   };
//
//   class R {
//   public:
//     void EventHandled() {
//       mEvent.Forget();
//     }
//   private:
//     nsRevocableEventPtr<E> mEvent;
//   };
//
//   void R::PostEvent() {
//     // Make sure any pending event is revoked.
//     mEvent->Revoke();
//
//     nsCOMPtr<nsIRunnable> event = new E();
//     if (NS_SUCCEEDED(NS_DispatchToCurrentThread(event))) {
//       // Keep pointer to event so we can revoke it.
//       mEvent = event;
//     }
//   }
//
//   NS_IMETHODIMP E::Run() {
//     if (!mResource)
//       return NS_OK;
//     ...
//     mResource->EventHandled();
//     return NS_OK;
//   }
//
template<class T>
class nsRevocableEventPtr
{
public:
  nsRevocableEventPtr() : mEvent(nullptr) {}
  ~nsRevocableEventPtr() { Revoke(); }

  const nsRevocableEventPtr& operator=(T* aEvent)
  {
    if (mEvent != aEvent) {
      Revoke();
      mEvent = aEvent;
    }
    return *this;
  }

  const nsRevocableEventPtr& operator=(already_AddRefed<T> aEvent)
  {
    RefPtr<T> event = aEvent;
    if (mEvent != event) {
      Revoke();
      mEvent = event.forget();
    }
    return *this;
  }

  void Revoke()
  {
    if (mEvent) {
      mEvent->Revoke();
      mEvent = nullptr;
    }
  }

  void Forget() { mEvent = nullptr; }
  bool IsPending() { return mEvent != nullptr; }
  T* get() { return mEvent; }

private:
  // Not implemented
  nsRevocableEventPtr(const nsRevocableEventPtr&);
  nsRevocableEventPtr& operator=(const nsRevocableEventPtr&);

  RefPtr<T> mEvent;
};

/**
 * A simple helper to suffix thread pool name
 * with incremental numbers.
 */
class nsThreadPoolNaming
{
public:
  nsThreadPoolNaming() : mCounter(0) {}

  /**
   * Creates and sets next thread name as "<aPoolName> #<n>"
   * on the specified thread.  If no thread is specified (aThread
   * is null) then the name is synchronously set on the current thread.
   */
  void SetThreadPoolName(const nsACString& aPoolName,
                         nsIThread* aThread = nullptr);

private:
  mozilla::Atomic<uint32_t> mCounter;

  nsThreadPoolNaming(const nsThreadPoolNaming&) = delete;
  void operator=(const nsThreadPoolNaming&) = delete;
};

/**
 * Thread priority in most operating systems affect scheduling, not IO.  This
 * helper is used to set the current thread to low IO priority for the lifetime
 * of the created object.  You can only use this low priority IO setting within
 * the context of the current thread.
 */
class MOZ_STACK_CLASS nsAutoLowPriorityIO
{
public:
  nsAutoLowPriorityIO();
  ~nsAutoLowPriorityIO();

private:
  bool lowIOPrioritySet;
#if defined(XP_MACOSX)
  int oldPriority;
#endif
};

void
NS_SetMainThread();

/**
 * Return the expiration time of the next timer to run on the current
 * thread.  If that expiration time is greater than aDefault, then
 * return aDefault.  aSearchBound specifies a maximum number of timers
 * to examine to find a timer on the current thread.  If no timer that
 * will run on the current thread is found after examining
 * aSearchBound timers, return the highest seen expiration time as a
 * best effort guess.
 *
 * Timers with either the type nsITimer::TYPE_ONE_SHOT_LOW_PRIORITY or
 * nsITIMER::TYPE_REPEATING_SLACK_LOW_PRIORITY will be skipped when
 * searching for the next expiration time.  This enables timers to
 * have lower priority than callbacks dispatched from
 * nsIThread::IdleDispatch.
 */
extern mozilla::TimeStamp
NS_GetTimerDeadlineHintOnCurrentThread(mozilla::TimeStamp aDefault, uint32_t aSearchBound);

#endif  // nsThreadUtils_h__
