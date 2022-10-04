/* FILE: checkthread.cc
 *
 * Test code for oxsthread classes.
 *
 * Build with:
 *
 *    g++ -I../../../pkg/oc -I../../../pkg/oc/<platform> \
 *         checkthread.cc oxsexcept.cc                   \
 *         -L../../../pkg/oc/<platform> -loc -ltcl -ltk
 *
 * On Windows + Visual C++
 *
 *     cl /EHac /GR \
 *      /I../../../pkg/oc /I../../../pkg/oc/wintel /I/tcl/include \
 *      /Tpcheckthread.cc /Tpoxsexcept.cc                         \
 *      /link /LIBPATH:../../../pkg/oc/wintel oc.lib              \
 *            /LIBPATH:/tcl/lib tcl85.lib tk85.lib                \
 *            /SUBSYSTEM:CONSOLE user32.lib advapi32.lib
 *
 * On Windows + MinGW g++
 *
 *    g++ -I../../../pkg/oc -I../../../pkg/oc/wintel -I/c/Tcl/include \
 *         checkthread.cc oxsexcept.cc                                \
 *         -Wl,--subsystem,console                                    \
 *         -L../../../pkg/oc/wintel -loc -L/c/Tcl/lib -ltcl85 -ltk85
 *
 */

#include <cstdio>
#include <cstdlib>

#include "oxsthread.cc"

# if defined(_WIN32) && !defined(__GNUC__)
  typedef __int64 HUGEINT;
# else
  typedef long long HUGEINT;
# endif

# if defined(_WIN32)
  void sleep(int secs) { Sleep(secs*1000); }
#endif

////////////////////////////////////////////////////////////////////////

class ThreadA : public Oxs_ThreadRunObj {
public:
  static std::mutex job_control;
  static int offset;

  int id;
  int blocksize;
  int vecsize;
  HUGEINT threadsum;

  ThreadA() : id(-1), blocksize(0), vecsize(0), threadsum(0) {}

  void Cmd(int threadnumber, void* data);
};

std::mutex ThreadA::job_control;
int ThreadA::offset(0);

void ThreadA::Cmd(int threadnumber,void* /* data */)
{
  while(1) {
    std::unique_lock<std::mutex> lck(job_control);
    int istart = offset;
    int istop = ( offset += blocksize );
    lck.unlock();

    if(istart>=vecsize) break;
    if(istop>vecsize) istop=vecsize;

    job_control.lock();
    printf("Job %d (thread %d) computing range %d - %d\n",
           id,threadnumber,istart,istop-1);
    fflush(stdout);
    lck.unlock();

    for(int i=istart;i<istop;++i) {
      threadsum += i;
    }
    // sleep(1);
  }
}

void Usage()
{
  fprintf(stderr,"Usage: checkthread <thread_count>\n");
  exit(1);
}

int Oc_AppMain(int argc, char** argv)
{
  if(argc != 2) Usage();
  int threadcount = atoi(argv[1]);
  if(threadcount<1) Usage();

  const int sumlimit  = 100000;
  const int blocksize = 1 + sumlimit/(3*threadcount);

  Oxs_ThreadTree threadtree;

  printf("Number of compute threads: %d\n",(int)threadcount);

  vector<ThreadA> threaddata;
  threaddata.resize(threadcount);

  {
    std::lock_guard<std::mutex> lck(ThreadA::job_control);
    ThreadA::offset = 0;
  }

  OC_INT4m ithread;
  for(ithread=0;ithread<threadcount;++ithread) {
    threaddata[ithread].id        = ithread;
    threaddata[ithread].blocksize = blocksize;
    threaddata[ithread].vecsize   = sumlimit + 1; // Last addend: vecsize - 1
    threaddata[ithread].threadsum = 0;
    if(ithread>0) threadtree.Launch(threaddata[ithread],0);
  }
  threadtree.LaunchRoot(threaddata[0],0);

  HUGEINT sum = 0;
  for(ithread=0;ithread<threadcount;++ithread) {
    sum += threaddata[ithread].threadsum;
  }

  HUGEINT correct_sum = HUGEINT(sumlimit)*HUGEINT(sumlimit+1);
  correct_sum /= 2;

  printf("Sum = %lld (should be %lld)\n",sum,correct_sum);
  fflush(stdout);

  Oxs_ThreadTree::EndThreads();  // Thread cleanup; NOP if non-threaded.

  printf("Finished.\n");

  return 0;
}
