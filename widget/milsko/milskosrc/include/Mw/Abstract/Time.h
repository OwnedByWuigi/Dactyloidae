/*!
 * @file Mw/Abstract/Time.h
 * @brief Time
 */
#ifndef __MW_ABSTRACT_TIME_H__
#define __MW_ABSTRACT_TIME_H__

#include <Mw/MachDep.h>

#ifdef __cplusplus
extern "C" {
#endif

/*!
 * @brief Sleeps for milliseconds
 * @param ms Milliseconds
 */
MWDECL void MWAPI MwTimeSleep(int ms);

/*!
 * @brief Gets a monotonic time
 * @return Monotonic time
 */
MWDECL long MWAPI MwTimeGetTick(void);

#ifdef __cplusplus
}
#endif

#endif
