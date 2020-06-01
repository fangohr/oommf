/* FILE: stopwatch.h                    -*-Mode: c++-*-
 *
 * StopWatch class
 * 
 * NOTICE: Please see the file ../../LICENSE
 *
 * Last modified on: $Date: 2015/05/26 22:42:52 $
 * Last modified by: $Author: donahue $
 */

#ifndef _NB_STOPWATCH
#define _NB_STOPWATCH

#include <string>

#include "evoc.h"

OC_USE_STRING;

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
  void Reset();
  void Start();
  void Stop();
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


// Extension of Nb_StopWatch that includes some extra fields for
// output identification and diagnosing performance issues.
class Nb_Timer {
private:
  Nb_StopWatch timer;
  size_t bytes;
  OC_INT4m pass_count;
public:
  OC_INT4m throw_away_count; // Number of initial passes to
  /// drop.  Useful if first pass or two through a code block is
  /// different from the common-case pass.
  String name;
  Nb_Timer() : bytes(0), pass_count(0), throw_away_count(0) {}
  Nb_Timer(const char* iname)
    : bytes(0), pass_count(0), throw_away_count(0), name(iname) {}
  void Reset() { pass_count = 0; bytes = 0; timer.Reset(); }
  void Start() { timer.Start(); }
  void Stop(size_t add_bytes=0) {
    timer.Stop();
    bytes += add_bytes;
    if(++pass_count == throw_away_count) {
      bytes=0;
      timer.Reset();
    }
  }
  void GetTimes(Oc_TimeVal &cpu,Oc_TimeVal &wall) const {
    timer.GetTimes(cpu,wall);
  }
  void Print(FILE* fptr,
             const char* line_head,const char* line_tail,
             int force=0) {
    // Dump timing info to fptr, unless force==0 and walltime<=0.0.
    Oc_TimeVal cpu,wall;
    timer.GetTimes(cpu,wall);
    if(force || double(wall)>0.0) {
      if(bytes<1) { // Standard print
        fprintf(fptr,"%.1000s%12s%9.2f cpu /%9.2f wall,"
                "%7" OC_INT4m_MOD "d passes (%.1000s)\n",
                line_head,name.c_str(),
                double(cpu),double(wall),pass_count,line_tail);
      } else { // Extended print
        fprintf(fptr,"%.1000s%12s%9.2f cpu /%9.2f wall,"
                " (%.1000s)\n",
                line_head,name.c_str(),
                double(cpu),double(wall),line_tail);
        fprintf(fptr,"%*s passes =%7" OC_INT4m_MOD "d",
                int(strlen(line_head)+12),"\\--->",pass_count);
        if(throw_away_count>0) {
          fprintf(fptr," (%" OC_INT4m_MOD "d)",-1*throw_away_count);
        }
        // Pick GB definition, 1024^3 or 10^9?
        double gigabytes = double(bytes)/(1024.*1024.*1024.);
        fprintf(fptr,
                ", bytes=%8.3f GB, bytes/pass=%8.3f GB, %6.2f GB/sec\n",
                gigabytes,gigabytes/pass_count,
                gigabytes/double(wall));
      }
    }
  }
};


//////////////////////////////////////////////////////////////////////////
// Nb_WallWatch timing class.  This class is similar to Nb_StopWatch,
// but this class returns only wall time, not processor time.  Also,
// Nb_WallWatch is more lightweight because it uses the Oc_Ticks class
// instead of Oc_Times.
//
// WARNING: The Stop, Start and Reset routines modify the "running"
//          member value, and so are not reentrant on the same
//          instance.  This means that each instance of Nb_WallWatch
//          should only be called from a unique thread, or otherwise
//          external mutex protection should be provided.
class Nb_WallWatch {
private:
  static const ClassDoc class_doc;
  OC_BOOL running;  // 1=>running, 0=>stopped
  Oc_Ticks wall_total; // Total accumulated times.
  Oc_Ticks wall_last;  // Clock values when watch was
                      /// last started.
public:
#ifdef __clang__
# pragma clang diagnostic push
# pragma clang diagnostic ignored "-Wself-assign-field"
#endif
  void Reset() {
    wall_last.ReadWallClock();
    wall_total -= wall_total;  // Zero accumulated time
  }
#ifdef __clang__
# pragma clang diagnostic pop
#endif

  Nb_WallWatch(OC_BOOL _running=0) : running(_running) {
    Reset();
  }

  void Start() {
    if(running) return;
    // Start clock
    running = 1;
    wall_last.ReadWallClock();
  }
  void Stop() {
    if(!running) return;  // Watch already stopped
    Oc_Ticks temp;
    temp.ReadWallClock();
    temp -= wall_last;
    wall_total += temp;
    running = 0;
  }

  OC_BOOL IsRunning() const { return running; }

  double GetTime() const { // Returns time in seconds.
    if(!running) {
      return wall_total.Seconds();
    }
    // Include current accumlated times
    Oc_Ticks temp;
    temp.ReadWallClock();
    temp -= wall_last;
    temp += wall_total;
    return temp.Seconds();
  }
};

#endif // _NB_STOPWATCH
