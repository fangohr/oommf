/* FILE: stopwatch.h                    -*-Mode: c++-*-
 *
 * StopWatch class
 * 
 * NOTICE: Please see the file ../../LICENSE
 *
 * Last modified on: $Date: 2010-07-16 22:33:58 $
 * Last modified by: $Author: donahue $
 */

#ifndef _NB_STOPWATCH
#define _NB_STOPWATCH

#include "evoc.h"

/* End includes */     /* Optional directive to build.tcl */

//////////////////////////////////////////////////////////////////////////
// StopWatch timing class
class Nb_StopWatch {
  // The routine uses the system independent Oc_Times function
  // to track cpu and wall (elapsed) times.
private:
  static const ClassDoc class_doc;
  OC_BOOL running;  // 1=>running, 0=>stopped
  Oc_TimeVal cpu_total,wall_total;   // Total accumulated times.
  Oc_TimeVal cpu_last,wall_last;  // Clock values when watch was
                                 /// last started.
public:
  Nb_StopWatch(OC_BOOL _running=0);
  void Stop();
  void Start();
  void Reset();
  OC_BOOL IsRunning() const { return running; }
  void GetTimes(Oc_TimeVal &cpu,Oc_TimeVal &wall) const;

  // Routines to accumulate time from another stopwatch into *this.
  // Accum just adds other.cpu_total and other.wall_total into cpu_total
  // and wall_total.  ThreadAccum is intended for accumulating time
  // across multiple concurrent threads.  For ThreadAccum, other.cpu_total
  // is added into cpu_total, just as Accum does.  However, wall_total is
  // only changed if other.wall_total is bigger than wall_total, and in
  // that case wall_total is replaced by other.wall_total.
  //    BIG NOTE: Currently, Oc_Times returns processor times, not
  // thread times.  So the cpu data is not useful for ThreadAccum.
  // So, at present, ThreadAccum just ignores cpu times.
  //    Note: Proper usage requires that "other" be reset between
  // accums.  Typically sequence is: (a) threadtimer.Reset(), one for
  // each thread (b) launch threads and start each threadtimer (c) join
  // threads and stop each threadtimer (d) ThreadAccum each threadtime
  // into a temptimer, (e) Accum temptimer into master timer.
  //    Note: These routines may misbehave if used while *this is
  // running, but no check is done.  In particular, they affect only the
  // *_total times; any increment coming from *_last is not considered.
  void Accum(const Nb_StopWatch& other);
  void ThreadAccum(const Nb_StopWatch& other);

};

#endif // _NB_STOPWATCH
