/*=========================================================================
 *
 *  Copyright Insight Software Consortium
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *         http://www.apache.org/licenses/LICENSE-2.0.txt
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 *=========================================================================*/
/*=========================================================================
 *
 *  Portions of this file are subject to the VTK Toolkit Version 3 copyright.
 *
 *  Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
 *
 *  For complete copyright, license and disclaimer of warranty information
 *  please refer to the NOTICE file at the top of the ITK source tree.
 *
 *=========================================================================*/
#ifndef itkMultiThreader_h
#define itkMultiThreader_h

#include "itkMutexLock.h"
#include "itkThreadSupport.h"
#include "itkIntTypes.h"
#include "itkBarrier.h"

#include "itkThreadPool.h"
#include <iostream>
#include <chrono>

namespace itk
{
/** \class MultiThreader
 * \brief A class for performing multithreaded execution
 *
 * Multithreader is a class that provides support for multithreaded
 * execution using pthread_create on any platform
 * supporting POSIX threads.  This class can be used to execute a single
 * method on multiple threads, or to specify a method per thread.
 *
 * \ingroup OSSystemObjects
 *
 * If ITK_USE_PTHREADS is defined, then
 * pthread_create() will be used to create multiple threads (on
 * a sun, for example).
 * \ingroup ITKCommon
 *
 * If ITK_USE_PARALLEL_PROCESS is defined, then this class performs 
 * process-parallelized execution.
 *
 * Prerequisites are for process paralelized execution are that 
 * ITK_GLOBAL_DEFAULT_NUMBER_OF_THREADS is defined in every process. 
 * The appropriate number of processes must be launched,
 * each with ITK_PROCESS_NUMBER defined, (between 0 and number
 * of processes). Process #0 must be launched first, since it initiates the 
 * appropriate files in the shared directory. However, if the appropriate files
 * are initiated before ITK is run, then any process can run first. 
 * The number of processes is constant throughout execution. Each process is 
 * single threaded.
 * 
 * Each process updates two files: the barrier file and the data file,
 * whose names can be defined using environmental variables 
 * ITK_BARRIER_FILE_PREFIX and ITK_DATA_FILE_PREFIX, respectively. 
 * These default to /tmp/itktmp and /tmp/itkhold. The process number
 * is appended to the file prefix. The barrier file synchronizes timing 
 * of the processes by storing the stage number of the process. The data 
 * file contains results from parallel execution that needs to be read 
 * by other parallel processes.
 *
 * Threading is nor supported in process parallelized build.
 */

class ITKCommon_EXPORT MultiThreader : public Object
{
public:
  /** Standard class typedefs. */
  typedef MultiThreader            Self;
  typedef Object                   Superclass;
  typedef SmartPointer<Self>       Pointer;
  typedef SmartPointer<const Self> ConstPointer;

  /* Remove later: Timing variables */
  static std::chrono::duration<double> t_read_write_time;
  static std::chrono::duration<double> t_after_threaded_time;
  static std::chrono::duration<double> t_single_worker_time;
  static std::chrono::duration<double> t_multi_worker_time;
  /**********************************/

  /** Method for creation through the object factory. */
  itkNewMacro(Self);

  /** Run-time type information (and related methods). */
  itkTypeMacro(MultiThreader, Object);

  /** Get/Set the number of threads to create. It will be clamped to the range
   * [ 1, m_GlobalMaximumNumberOfThreads ], so the caller of this method should
   * check that the requested number of threads was accepted. */
  void SetNumberOfThreads(ThreadIdType numberOfThreads);

  itkGetConstMacro(NumberOfThreads, ThreadIdType);

  /** Set/Get the maximum number of threads to use when multithreading.  It
   * will be clamped to the range [ 1, ITK_MAX_THREADS ] because several arrays
   * are already statically allocated using the ITK_MAX_THREADS number.
   * Therefore the caller of this method should check that the requested number
   * of threads was accepted. */
  static void SetGlobalMaximumNumberOfThreads(ThreadIdType val);
  static ThreadIdType  GetGlobalMaximumNumberOfThreads();

#ifdef ITK_USE_PARALLEL_PROCESSES
  /** This function configures the file prefixes and thread number.
    * If thread number is invalid, the process exits. */
  static void ConfigureStaticMembers();
#endif

#ifndef ITK_USE_PARALLEL_PROCESSES
  /** Set/Get whether to use the to use the thread pool
   * implementation or the spawing implementation of
   * starting threads.
   */
  static void SetGlobalDefaultUseThreadPool( const bool GlobalDefaultUseThreadPool );
  static bool GetGlobalDefaultUseThreadPool( );
#endif

  /** Set/Get the value which is used to initialize the NumberOfThreads in the
   * constructor.  It will be clamped to the range [1, m_GlobalMaximumNumberOfThreads ].
   * Therefore the caller of this method should check that the requested number
   * of threads was accepted. */
  static void SetGlobalDefaultNumberOfThreads(ThreadIdType val);
  static ThreadIdType  GetGlobalDefaultNumberOfThreads();

  /** Execute the SingleMethod (as define by SetSingleMethod) using
   * m_NumberOfThreads threads. As a side effect the m_NumberOfThreads will be
   * checked against the current m_GlobalMaximumNumberOfThreads and clamped if
   * necessary. */
  void SingleMethodExecute();

  /** Execute the MultipleMethods (as define by calling SetMultipleMethod for
   * each of the required m_NumberOfThreads methods) using m_NumberOfThreads
   * threads. As a side effect the m_NumberOfThreads will be checked against the
   * current m_GlobalMaximumNumberOfThreads and clamped if necessary. */
  void MultipleMethodExecute();

  /** Set the SingleMethod to f() and the UserData field of the
   * ThreadInfoStruct that is passed to it will be data.
   * This method (and all the methods passed to SetMultipleMethod)
   * must be of type itkThreadFunctionType and must take a single argument of
   * type void *. */
  void SetSingleMethod(ThreadFunctionType, void *data);

  /** Set the MultipleMethod at the given index to f() and the UserData
   * field of the ThreadInfoStruct that is passed to it will be data. */
  void SetMultipleMethod(ThreadIdType index, ThreadFunctionType, void *data);

#ifndef ITK_USE_PARALLEL_PROCESSES
  /** Create a new thread for the given function. Return a thread id
     * which is a number between 0 and ITK_MAX_THREADS - 1. This
   * id should be used to kill the thread at a later time. */
  ThreadIdType SpawnThread(ThreadFunctionType, void *data);

  /** Terminate the thread that was created with a SpawnThreadExecute() */
  void TerminateThread(ThreadIdType thread_id);

  /** Set the ThreadPool used by this MultiThreader. If not set,
    * the default ThreadPool will be used. Currently ThreadPool
    * is only used in SingleMethodExecute. */
  itkSetObjectMacro(ThreadPool, ThreadPool);

  /** Get the ThreadPool used by this MultiThreader */
  itkGetModifiableObjectMacro(ThreadPool, ThreadPool);

  /** Set the flag to use a threadpool instead of spawning individual
    * threads
    */
  itkSetMacro(UseThreadPool,bool);
  /** Get the UseThreadPool flag*/
  itkGetMacro(UseThreadPool,bool);

  /** Declare functions from Process Parallelized MultiThreader
   *  so that everything compiles. */
  static ThreadProcessIdType GetThreadNumber()
    {
    std::cerr << "itk::MultiThreader::GetThreadNumber is not implemented. Exiting ... \n";
    exit(1);
    return (ThreadProcessIdType)(0);
    };
  static void GetIfstream(std::ifstream & itkNotUsed(is), ThreadProcessIdType itkNotUsed(threadHandle))
    {
    std::cerr << "itk::MultiThreader::GetIfstream is not implemented. Exiting ... \n";
    exit(1);
    };
  static void GetOfstream(std::ofstream & itkNotUsed(os), ThreadProcessIdType itkNotUsed(threadHandle))
    {
    std::cerr << "itk::MultiThreader::GetOfstream is not implemented. Exiting ... \n";
    exit(1);
    };
  static void ProcessDone(ThreadProcessIdType itkNotUsed(threadHandle))
    {
    std::cerr << "itk::MultiThreader::ProcessDone is not implemented. Exiting ... \n";
    exit(1);
    };
  static void GlobalBarrier()
    {
    std::cerr << "itk::MultiThreader::Barrier is not implemented. Exiting ... \n";
    exit(1);
    };
  static void Sync(char * itkNotUsed(data), std::size_t itkNotUsed(len))
    {
    std::cerr << "itk::MultiThreader::Sync is not implemented. Exiting ... \n";
    exit(1);
    };
  static void Exit()
    {
    std::cerr << "itk::MultiThreader::Exit is not implemented. Exiting ... \n";
    exit(1);
    };

#else
  /** get the Process number assigned to this process by 
    * environmental variable ITK_PROCESS_NUMBER. */
  static ThreadProcessIdType GetWorkerNumber();

  static unsigned int GetNumberOfWorkers();

  static unsigned int GetThreadsPerWorker();

  static unsigned int GetTotalNumberOfThreads();

  static unsigned int GetLastThreadId();

  static unsigned int GetFirstThreadId();

  struct ThreadBlock {
    unsigned int firstIndex;
    unsigned int length;
  };

  static ThreadBlock DistributeJobsEvenly(unsigned int nWorkers,
                                          unsigned int nJobs, unsigned int workerId);
  static unsigned int MapIndexToGlobalThreadId(unsigned int jobIndex,
                                               unsigned int firstThreadId,
                                               unsigned int lastThreadId);

  static ThreadProcessIdType ConvertThreadId(ThreadProcessIdType threadId, int threadCount);

  /** Set input file stream is to read from data file of process #threadHandle. */
  static void GetIfstream(std::ifstream & is, ThreadProcessIdType threadHandle);

  /** Set output file stream os to write to data file of process #threadHandle. */
  static void GetOfstream(std::ofstream & os, ThreadProcessIdType threadHandle);

  /** Wait for a thread running the prescribed SingleMethod. A similar
   * abstraction needs to be added for MultipleMethod (SpawnThread
   * already has a routine to do this. */
  static void WaitForSingleMethodThread(ThreadProcessIdType);

  /** Wait for process #threadhandle by repeatedly reading its barrier file until
    * the process reaches the same stage number as current process. */
  static void WaitForProcess(ThreadProcessIdType threadHandle);
 
  /** Declare to other processes that current process is done with current stage
    * by incrementing the stage number in current process's barrier file. */
  static void ProcessDone(ThreadProcessIdType threadHandle);
  
  /** All processes must reach this point before continuing. 
    * Each time a processs hits a barrier, the stage number should
    * increase by one. 
    * Barriers are used to synchronize timing of parallel processes. */
  static void GlobalBarrier();

  static void ThreadedBarrier(unsigned int localThreadId);

  /** Call if process needs to exit. This writes the max unsigned long to 
    * the barrier file, which tells other processes to exit. */
  static void Exit();
  
  /** Synchronze the variable pointed to by data of length len.
    * More specifically, process #0 broadcasts its value to 
    * the other processes. All other processes overwrite their 
    * variable with process #0's value. 
    * This is useful for synchronizing random number generator seeds 
    * or decimal values that drift over time. */
  static void Sync(char * data, std::size_t len);
  
  /** Helper function, for c++98 support. */
  static std::string to_string(unsigned int i);

  /** These two functions should not be called and are only 
    * declared so the code compiles. 
  ThreadIdType SpawnThread(ThreadFunctionType itkNotUsed(f), void * itkNotUsed(data))
    {
    std::cerr << "itk::MultiThreader::SpawnThread is not implemented. Exiting ... \n";
    MultiThreader::Exit();
    return (ThreadIdType)(0); // Return statement to suppress warning
    };
  void TerminateThread(ThreadIdType itkNotUsed(thread_id))
    {
    std::cerr << "itk::MultiThreader::TerminateThread is not implemented. Exiting ... \n";
    MultiThreader::Exit();
    };
    */
  ThreadIdType SpawnThread(ThreadFunctionType f, void * data);
  void TerminateThread(ThreadIdType thread_id);
#endif 

  /** This is the structure that is passed to the thread that is
   * created from the SingleMethodExecute, MultipleMethodExecute or
   * the SpawnThread method. It is passed in as a void *, and it is up
   * to the method to cast correctly and extract the information.  The
   * ThreadID is a number between 0 and NumberOfThreads-1 that
   * indicates the id of this thread. The NumberOfThreads is
   * this->NumberOfThreads for threads created from
   * SingleMethodExecute or MultipleMethodExecute, and it is 1 for
   * threads created from SpawnThread.  The UserData is the (void
   * *)arg passed into the SetSingleMethod, SetMultipleMethod, or
   * SpawnThread method. */
#ifdef ThreadInfoStruct
#undef ThreadInfoStruct
#endif
  struct ThreadInfoStruct
    {
    ThreadIdType ThreadID;
    ThreadIdType NumberOfThreads;
    int *ActiveFlag;
    MutexLock::Pointer ActiveFlagLock;
    void *UserData;
    ThreadFunctionType ThreadFunction;
    enum { SUCCESS, ITK_EXCEPTION, ITK_PROCESS_ABORTED_EXCEPTION, STD_EXCEPTION, UNKNOWN } ThreadExitCode;
    };

protected:
  MultiThreader();
  ~MultiThreader();
  virtual void PrintSelf(std::ostream & os, Indent indent) const ITK_OVERRIDE;

private:
  ITK_DISALLOW_COPY_AND_ASSIGN(MultiThreader);

#ifndef ITK_USE_PARALLEL_PROCESSES
  // Thread pool instance and factory
  ThreadPool::Pointer m_ThreadPool;

  // choose whether to use Spawn or ThreadPool methods
  bool m_UseThreadPool;
#endif

  /** An array of thread info containing a thread id
   *  (0, 1, 2, .. ITK_MAX_THREADS-1), the thread count, and a pointer
   *  to void so that user data can be passed to each thread. */
  ThreadInfoStruct m_ThreadInfoArray[ITK_MAX_THREADS];

  /** The methods to invoke. */
  ThreadFunctionType m_SingleMethod;
  ThreadFunctionType m_MultipleMethod[ITK_MAX_THREADS];

  /** Storage of MutexFunctions and ints used to control spawned
   *  threads and the spawned thread ids. */
  int                 m_SpawnedThreadActiveFlag[ITK_MAX_THREADS];
  MutexLock::Pointer  m_SpawnedThreadActiveFlagLock[ITK_MAX_THREADS];
  ThreadProcessIdType m_SpawnedThreadProcessID[ITK_MAX_THREADS];
  ThreadInfoStruct    m_SpawnedThreadInfoArray[ITK_MAX_THREADS];

  /** Internal storage of the data. */
  void *m_SingleData;
  void *m_MultipleData[ITK_MAX_THREADS];

  /** Global variable defining the maximum number of threads that can be used.
   *  The m_GlobalMaximumNumberOfThreads must always be less than or equal to
   *  ITK_MAX_THREADS and greater than zero. */
  static ThreadIdType m_GlobalMaximumNumberOfThreads;

#ifndef ITK_USE_PARALLEL_PROCESSES
  /** Global value to effect weather the threadpool implementation should
   * be used.  This defaults to the environmental variable "ITK_USE_THREADPOOL"
   * if set, else it default to false and new threads are spawned.
   */
  static bool m_GlobalDefaultUseThreadPool;
#endif

  /*  Global variable defining the default number of threads to set at
   *  construction time of a MultiThreader instance.  The
   *  m_GlobalDefaultNumberOfThreads must always be less than or equal to the
   *  m_GlobalMaximumNumberOfThreads and larger or equal to 1 once it has been
   *  initialized in the constructor of the first MultiThreader instantiation.
   */
  static ThreadIdType m_GlobalDefaultNumberOfThreads;
  
  /** The number of threads to use.
   *  The m_NumberOfThreads must always be less than or equal to
   *  the m_GlobalMaximumNumberOfThreads before it is used during the execution
   *  of a threaded method. Its value is clamped in the SingleMethodExecute()
   *  and MultipleMethodExecute(). Its value is initialized to
   *  m_GlobalDefaultNumberOfThreads at construction time. Its value is clamped
   *  to the current m_GlobalMaximumNumberOfThreads in the
   *  SingleMethodExecute() and MultipleMethodExecute() methods.
   */
  ThreadIdType m_NumberOfThreads;

  /** Static function used as a "proxy callback" by the MultiThreader.  The
   * threading library will call this routine for each thread, which
   * will delegate the control to the prescribed SingleMethod. This
   * routine acts as an intermediary between the MultiThreader and the
   * user supplied callback (SingleMethod) in order to catch any
   * exceptions thrown by the threads. */
  static ITK_THREAD_RETURN_TYPE SingleMethodProxy(void *arg);

  /**  Platform specific number of threads */
  static ThreadIdType  GetGlobalDefaultNumberOfThreadsByPlatform();

  /** spawn a new thread for the SingleMethod */
  ThreadProcessIdType SpawnDispatchSingleMethodThread(ThreadInfoStruct *);
  /** wait for a thread in the threadpool to finish work */
  void SpawnWaitForSingleMethodThread(ThreadProcessIdType);

#ifndef ITK_USE_PARALLEL_PROCESSES
  /** Assign work to a thread in the thread pool */
  ThreadProcessIdType ThreadPoolDispatchSingleMethodThread(ThreadInfoStruct *);
  /** wait for a thread in the threadpool to finish work */
  void ThreadPoolWaitForSingleMethodThread(ThreadProcessIdType);

  /** Spawn a thread for the prescribed SingleMethod.  This routine
   * spawns a thread to the SingleMethodProxy which runs the
   * prescribed SingleMethod.  The SingleMethodProxy allows for
   * exceptions within a thread to be naively handled. A similar
   * abstraction needs to be added for MultipleMethod and
   * SpawnThread. */
  ThreadProcessIdType DispatchSingleMethodThread(ThreadInfoStruct *);

  /** Wait for a thread running the prescribed SingleMethod. A similar
   * abstraction needs to be added for MultipleMethod (SpawnThread
   * already has a routine to do this. */
  void WaitForSingleMethodThread(ThreadProcessIdType);

#else
  /* Number assigned by ITK_PROCESS_NUMBER */
  static ThreadIdType m_WorkerNumber;
  static unsigned int m_NumberOfWorkers;
  static unsigned int m_ThreadsPerWorker;
  static unsigned int m_FirstThreadId;
  static unsigned int m_LastThreadId;
  static Barrier::Pointer m_localThreadBarrier;

  /* Stage number to synchronize processes. Starts from 0 */
  static unsigned long m_CurrentStage;

  /* Define the prefix to the file names used to communicate with other threads. */
  static std::string m_DataFilePrefix;
  static std::string m_BarrierFilePrefix;

  /** This is the number of seconds which we will wait for other processes
    * before timing out. Defaults to 60, can be overriden by ITK_TIME_OUT_SECONDS. */
  static int m_WaitTimeOutSeconds;
#endif

  /** Friends of Multithreader.
   * ProcessObject is a friend so that it can call PrintSelf() on its
   * Multithreader. */
  friend class ProcessObject;
};
}  // end namespace itk
#endif

