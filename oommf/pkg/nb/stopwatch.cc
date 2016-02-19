/* FILE: stopwatch.cc                   -*-Mode: c++-*-
 *
 * StopWatch class
 *
 * NOTICE: Please see the file ../../LICENSE
 *
 * Last modified on: $Date: 2015/03/31 21:51:35 $
 * Last modified by: $Author: donahue $
 */

#include "stopwatch.h"
#include "errhandlers.h"

/* End includes */     

//////////////////////////////////////////////////////////////////////////
// StopWatch timing class
//
const ClassDoc Nb_StopWatch::class_doc("Nb_StopWatch",
	      "Michael J. Donahue (michael.donahue@nist.gov)",
	      "1.0.0","May-1998");


void Nb_StopWatch::Reset()
{
  // Initialize Oc_TimeVal's
  Oc_Times(cpu_last,wall_last);
  cpu_total=cpu_last; cpu_total.Reset(); // Make compatible
  wall_total=wall_last; wall_total.Reset(); // Ditto
}

Nb_StopWatch::Nb_StopWatch(OC_BOOL _running)
{
  running=_running;
  Reset();
}

void Nb_StopWatch::Start()
{
  if(running) return;
  // Start clock
  running=1;
  Oc_Times(cpu_last,wall_last);
}

void Nb_StopWatch::Stop()
{
  if(!running) return;  // Watch already stopped
  // Stop clock
  Oc_TimeVal cpu_now,wall_now;
  Oc_Times(cpu_now,wall_now);
  cpu_total+=(cpu_now-cpu_last);
  wall_total+=(wall_now-wall_last);
  running=0;
}

void Nb_StopWatch::GetTimes(Oc_TimeVal &cpu,Oc_TimeVal &wall) const
{
  if(!running) {
    cpu=cpu_total; wall=wall_total;
  } else {
    // Include current accumlated times
    Oc_TimeVal cpu_now,wall_now;
    Oc_Times(cpu_now,wall_now);
    cpu=cpu_total+(cpu_now-cpu_last);
    wall=wall_total+(wall_now-wall_last);
  }
}

void Nb_StopWatch::Accum(const Nb_StopWatch& other)
{
  Oc_TimeVal other_cpu,other_wall;
  other.GetTimes(other_cpu,other_wall);
  cpu_total  += other_cpu;
  wall_total += other_wall;
}

void Nb_StopWatch::ThreadAccum(const Nb_StopWatch& other)
{
  //    BIG NOTE: Currently, Oc_Times returns processor times, not
  // thread times.  So the cpu data is not useful for ThreadAccum.
  // So, at present, ThreadAccum just ignores cpu times.
  Oc_TimeVal other_cpu,other_wall;
  other.GetTimes(other_cpu,other_wall);
#if 0
  cpu_total += other_cpu;
#endif
  wall_total = GetBigger(wall_total,other_wall);
}


//////////////////////////////////////////////////////////////////////////
// Nb_WallWatch timing class
//
const ClassDoc Nb_WallWatch::class_doc("Nb_WallWatch",
	      "Michael J. Donahue (michael.donahue@nist.gov)",
	      "1.0.0","Mar-2015");

