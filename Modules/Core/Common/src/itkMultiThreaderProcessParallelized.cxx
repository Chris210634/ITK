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
#ifdef ITK_USE_PARALLEL_PROCESSES

#include "itkObjectFactory.h"
#include "itksys/SystemTools.hxx"
#include <unistd.h>

#include "itkMultiThreader.h"
#include "itkNumericTraits.h"

#if !defined( ITK_LEGACY_FUTURE_REMOVE )
# include "vcl_algorithm.h"
#endif

#include <iostream>
#include <string>
#include <algorithm>
#include <fstream>
#include <climits>
#include <time.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/wait.h>

namespace itk
{
static SimpleFastMutexLock globalDefaultInitializerLock;

ThreadIdType MultiThreader::m_ThreadNumber       = 0;
unsigned long MultiThreader::m_CurrentStage      = 0;

// Initialize files with default
std::string MultiThreader::m_DataFilePrefix    = "/tmp/itktmp";
std::string MultiThreader::m_BarrierFilePrefix = "/tmp/itkhold";

//Initialize the default time out on waiting for other processes
int MultiThreader::m_WaitTimeOutSeconds = 60;

// Initialize static member that controls global maximum number of threads.
ThreadIdType MultiThreader::m_GlobalMaximumNumberOfThreads = ITK_MAX_THREADS;

// Initialize static member that controls global default number of threads : 0
// => Not initialized.
ThreadIdType MultiThreader::m_GlobalDefaultNumberOfThreads = 0;

void MultiThreader::SetGlobalMaximumNumberOfThreads(ThreadIdType val)
{
  m_GlobalMaximumNumberOfThreads = val;

  // clamp between 1 and ITK_MAX_THREADS
  m_GlobalMaximumNumberOfThreads = std::min( m_GlobalMaximumNumberOfThreads,
                                             (ThreadIdType) ITK_MAX_THREADS );
  m_GlobalMaximumNumberOfThreads = std::max( m_GlobalMaximumNumberOfThreads,
                                             NumericTraits<ThreadIdType>::OneValue() );

  // If necessary reset the default to be used from now on.
  m_GlobalDefaultNumberOfThreads = std::min( m_GlobalDefaultNumberOfThreads,
                                             m_GlobalMaximumNumberOfThreads);
}

ThreadIdType MultiThreader::GetGlobalMaximumNumberOfThreads()
{
  return m_GlobalMaximumNumberOfThreads;
}

void MultiThreader::SetGlobalDefaultNumberOfThreads(ThreadIdType val)
{
  m_GlobalDefaultNumberOfThreads = val;

  // clamp between 1 and m_GlobalMaximumNumberOfThreads
  m_GlobalDefaultNumberOfThreads  = std::min( m_GlobalDefaultNumberOfThreads,
                                              m_GlobalMaximumNumberOfThreads );
  m_GlobalDefaultNumberOfThreads  = std::max( m_GlobalDefaultNumberOfThreads,
                                              NumericTraits<ThreadIdType>::OneValue() );

}

void MultiThreader::SetNumberOfThreads(ThreadIdType numberOfThreads)
{
  if( m_NumberOfThreads == numberOfThreads &&
      numberOfThreads <= m_GlobalMaximumNumberOfThreads )
    {
    return;
    }

  m_NumberOfThreads = numberOfThreads;

  // clamp between 1 and m_GlobalMaximumNumberOfThreads
  m_NumberOfThreads  = std::min( m_NumberOfThreads,
                                 m_GlobalMaximumNumberOfThreads );
  m_NumberOfThreads  = std::max( m_NumberOfThreads, NumericTraits<ThreadIdType>::OneValue() );

}

ThreadIdType MultiThreader::GetGlobalDefaultNumberOfThreads()
{
  // if default number has been set then don't try to update it; just
  // return the value
  if( m_GlobalDefaultNumberOfThreads != 0 )
    {
    return m_GlobalDefaultNumberOfThreads;
    }

  /* The ITK_NUMBER_OF_THREADS_ENV_LIST contains is an
   * environmental variable that holds a ':' separated
   * list of environmental variables that whould be
   * queried in order for setting the m_GlobalMaximumNumberOfThreads.
   *
   * This is intended to be a mechanism suitable to easy
   * runtime modification to ease using the proper number
   * of threads for load balancing batch processing
   * systems where the number of threads
   * authorized for use may be less than the number
   * of physical processors on the computer.
   *
   * This list contains the Sun|Oracle Grid Engine
   * environmental variable "NSLOTS" by default
   */
  std::vector<std::string> ITK_NUMBER_OF_THREADS_ENV_LIST;
  std::string       itkNumberOfThreadsEvnListString = "";
  if( itksys::SystemTools::GetEnv("ITK_NUMBER_OF_THREADS_ENV_LIST",
                                  itkNumberOfThreadsEvnListString) )
    {
    // NOTE: We always put "ITK_GLOBAL_DEFAULT_NUMBER_OF_THREADS" at the end
    // unconditionally.
    itkNumberOfThreadsEvnListString += ":ITK_GLOBAL_DEFAULT_NUMBER_OF_THREADS";
    }
  else
    {
    itkNumberOfThreadsEvnListString = "NSLOTS:ITK_GLOBAL_DEFAULT_NUMBER_OF_THREADS";
    }
    {
    std::stringstream numberOfThreadsEnvListStream(itkNumberOfThreadsEvnListString);
    std::string       item;
    while( std::getline(numberOfThreadsEnvListStream, item, ':') )
      {
      if( item.size() > 0 ) // Do not add empty items.
        {
        ITK_NUMBER_OF_THREADS_ENV_LIST.push_back(item);
        }
      }
    }
  // first, check for environment variable
  std::string itkGlobalDefaultNumberOfThreadsEnv = "0";
  for( std::vector<std::string>::const_iterator lit = ITK_NUMBER_OF_THREADS_ENV_LIST.begin();
       lit != ITK_NUMBER_OF_THREADS_ENV_LIST.end();
       ++lit )
    {
    if( itksys::SystemTools::GetEnv(lit->c_str(), itkGlobalDefaultNumberOfThreadsEnv) )
      {
      m_GlobalDefaultNumberOfThreads =
        static_cast<ThreadIdType>( atoi( itkGlobalDefaultNumberOfThreadsEnv.c_str() ) );
      }
    }

  // otherwise, exit with error
  if( m_GlobalDefaultNumberOfThreads <= 0 )
    {
    std::cerr << "ITK_GLOBAL_DEFAULT_NUMBER_OF_THREADS not set ot <= 0. Exiting ...\n";
    exit(1);
    }

  // limit the number of threads to m_GlobalMaximumNumberOfThreads
  m_GlobalDefaultNumberOfThreads  = std::min( m_GlobalDefaultNumberOfThreads,
                                              m_GlobalMaximumNumberOfThreads );

  // verify that the default number of threads is larger than zero
  m_GlobalDefaultNumberOfThreads  = std::max( m_GlobalDefaultNumberOfThreads,
                                              NumericTraits<ThreadIdType>::OneValue() );
  return m_GlobalDefaultNumberOfThreads;
}
  
void MultiThreader::ConfigureStaticMembers()
{
  // If stage is not zero, then everything's been configured already.
  if( m_CurrentStage != 0 )
    {
    return;
    }

  //Configure prefixes for process communication files
  if(const char* env_data_file_prefix = std::getenv("ITK_DATA_FILE_PREFIX"))
    {
    m_DataFilePrefix = std::string(env_data_file_prefix);
    }
  if(const char* env_barrier_file_prefix = std::getenv("ITK_BARRIER_FILE_PREFIX"))
    {
    m_BarrierFilePrefix = std::string(env_barrier_file_prefix);
    }

  //Configure time out waiting for other processes in seconds
  if(const char* env_time_out_seconds = std::getenv("ITK_TIME_OUT_SECONDS"))
    {
    m_WaitTimeOutSeconds = atoi(env_time_out_seconds);
    }

  //Configure thread number 
  if(const char* env_thread_number = std::getenv("ITK_PROCESS_NUMBER"))
    {
    m_ThreadNumber = static_cast<ThreadIdType>(atoi(env_thread_number));
    if ( m_ThreadNumber >=  m_GlobalDefaultNumberOfThreads)
      {
      std::cerr << "ITK_PROCESS_NUMBER out of range, exiting.\n";
      exit(1);
      }
    m_CurrentStage = 1;

    // Process number 0 is responsible for setting all itk barrier files to 0
    // if ITK_BARRIER_FILE_RESET is not 1
    bool barrier_file_reset = false;
    if(const char* env_barrier_file_reset = std::getenv("ITK_BARRIER_FILE_RESET"))
      {
      barrier_file_reset = atoi(env_barrier_file_reset);
      }
    
    if ((!barrier_file_reset) and m_ThreadNumber == 0)
      {
      unsigned long s = 0;
      std::ofstream ofs;
      for (unsigned int i=0 ; i< m_GlobalDefaultNumberOfThreads ; i++)
        {
        std::string filename = m_BarrierFilePrefix + std::to_string(i);
        ofs.open(filename,std::ios::binary);
        ofs.write((char*)(&s),sizeof(s));
        ofs.close();
        }
      }
    }
  else 
    {
    std::cerr << "ITK_PROCESS_NUMBER not set, exiting.\n";
    exit(1);
    }
}

// Constructor. Default all the methods to ITK_NULLPTR. Since the
// ThreadInfoArray is static, the ThreadIDs can be initialized here
// and will not change.
MultiThreader::MultiThreader()
{
  for( ThreadIdType i = 0; i < ITK_MAX_THREADS; ++i )
    {
    m_ThreadInfoArray[i].ThreadID           = i;
    m_ThreadInfoArray[i].ActiveFlag         = ITK_NULLPTR;
    m_ThreadInfoArray[i].ActiveFlagLock     = ITK_NULLPTR;

    m_MultipleMethod[i]                     = ITK_NULLPTR;
    m_MultipleData[i]                       = ITK_NULLPTR;

    m_SpawnedThreadActiveFlag[i]            = 0;
    m_SpawnedThreadActiveFlagLock[i]        = ITK_NULLPTR;
    m_SpawnedThreadInfoArray[i].ThreadID    = i;
    }

  m_SingleMethod = ITK_NULLPTR;
  m_SingleData = ITK_NULLPTR;
  m_NumberOfThreads = this->GetGlobalDefaultNumberOfThreads();
  this->ConfigureStaticMembers();
}

MultiThreader::~MultiThreader()
{
}

// Set the user defined method that will be run on NumberOfThreads threads
// when SingleMethodExecute is called.
void MultiThreader::SetSingleMethod(ThreadFunctionType f, void *data)
{
  m_SingleMethod = f;
  m_SingleData   = data;
}

// Set one of the user defined methods that will be run on NumberOfThreads
// threads when MultipleMethodExecute is called. This method should be
// called with index = 0, 1, ..,  NumberOfThreads-1 to set up all the
// required user defined methods
void MultiThreader::SetMultipleMethod(ThreadIdType index, ThreadFunctionType f, void *data)
{
  // You can only set the method for 0 through NumberOfThreads-1
  if( index >= m_NumberOfThreads )
    {
    itkExceptionMacro(<< "Can't set method " << index << " with a thread count of " << m_NumberOfThreads);
    }
  else
    {
    m_MultipleMethod[index] = f;
    m_MultipleData[index]   = data;
    }
}

void MultiThreader::MultipleMethodExecute()
{
  // raise not implemented error
}

// Execute the method set as the SingleMethod on NumberOfThreads threads.
void MultiThreader::SingleMethodExecute()
{
  //ThreadIdType        thread_loop = 0;
  //ThreadProcessIdType process_id[ITK_MAX_THREADS];

  if( !m_SingleMethod )
    {
    itkExceptionMacro(<< "No single method set!");
    }

  // obey the global maximum number of threads limit
  m_NumberOfThreads = std::min( m_GlobalMaximumNumberOfThreads, m_NumberOfThreads );

  // Spawn a set of threads through the SingleMethodProxy. Exceptions
  // thrown from a thread will be caught by the SingleMethodProxy. A
  // naive mechanism is in place for determining whether a thread
  // threw an exception.
  //
  // Thanks to Hannu Helminen for suggestions on how to catch
  // exceptions thrown by threads.
  bool        exceptionOccurred = false;
  std::string exceptionDetails;

  // Single threaded (non parallelized) segment must be executed as thread 0
  if ( m_NumberOfThreads == 1)
    {
    try
      {
      m_ThreadInfoArray[0].UserData = m_SingleData;
      m_ThreadInfoArray[0].NumberOfThreads = m_NumberOfThreads;
      m_SingleMethod( (void *)( &m_ThreadInfoArray[0] ) );
      }
    catch( std::exception & e )
      {
      // get the details of the exception to rethrow them
      exceptionDetails = e.what();
      // if this method fails, we must make sure all threads are
      // correctly cleaned
      exceptionOccurred = true;
      }
    catch( ... )
      {
      // if this method fails, we must make sure all threads are
      // correctly cleaned
      exceptionOccurred = true;
      }
    }

  // Multithreaded process, only execute the part that is assigned to
  // m_ThreadNumber 
  else
    {
    try
      {
      m_ThreadInfoArray[m_ThreadNumber].UserData    = m_SingleData;
      m_ThreadInfoArray[m_ThreadNumber].NumberOfThreads = m_NumberOfThreads;
      m_ThreadInfoArray[m_ThreadNumber].ThreadFunction = m_SingleMethod;
      m_SingleMethod( (void *)( &m_ThreadInfoArray[m_ThreadNumber] ) );
      }
    catch( std::exception & e )
      {
      // get the details of the exception to rethrow them
      exceptionDetails = e.what();
      // if this method fails, we must make sure all threads are
      // correctly cleaned
      exceptionOccurred = true;
      }
    catch( ... )
      {
      // if this method fails, we must make sure all threads are
      // correctly cleaned
      exceptionOccurred = true;
      }
    }

  if( exceptionOccurred )
    {
    if( exceptionDetails.empty() )
      {
      itkExceptionMacro("Exception occurred during SingleMethodExecute");
      }
    else
      {
      itkExceptionMacro(<< "Exception occurred during SingleMethodExecute" << std::endl << exceptionDetails);
      }
    }
}

ITK_THREAD_RETURN_TYPE
MultiThreader
::SingleMethodProxy(void *arg)
{
  // grab the ThreadInfoStruct originally prescribed
  MultiThreader::ThreadInfoStruct
  * threadInfoStruct =
    reinterpret_cast<MultiThreader::ThreadInfoStruct *>( arg );

  // execute the user specified threader callback, catching any exceptions
  try
    {
    ( *threadInfoStruct->ThreadFunction )(threadInfoStruct);
    threadInfoStruct->ThreadExitCode = MultiThreader::ThreadInfoStruct::SUCCESS;
    }
  catch( ProcessAborted & )
    {
    threadInfoStruct->ThreadExitCode =
      MultiThreader::ThreadInfoStruct::ITK_PROCESS_ABORTED_EXCEPTION;
    }
  catch( ExceptionObject & )
    {
    threadInfoStruct->ThreadExitCode =
      MultiThreader::ThreadInfoStruct::ITK_EXCEPTION;
    }
  catch( std::exception & )
    {
    threadInfoStruct->ThreadExitCode =
      MultiThreader::ThreadInfoStruct::STD_EXCEPTION;
    }
  catch( ... )
    {
    threadInfoStruct->ThreadExitCode = MultiThreader::ThreadInfoStruct::UNKNOWN;
    }

  return ITK_THREAD_RETURN_VALUE;
}

void 
MultiThreader
::ProcessDone(ThreadProcessIdType threadHandle)
{
  std::string filename = m_BarrierFilePrefix + std::to_string(threadHandle);
  std::ofstream ofs;
  ofs.open(filename, std::ios::binary);
  ofs.write((char*)(&m_CurrentStage),sizeof(m_CurrentStage));
  ofs.close();
}

void 
MultiThreader
::WaitForProcess(ThreadProcessIdType threadHandle)
{
  clock_t start_clock = clock();
  std::string filename = m_BarrierFilePrefix + std::to_string(threadHandle);
  std::ifstream ifs;
  unsigned long stage = 0;
  while (stage < m_CurrentStage)
    {
    ifs.open(filename, std::ios::binary);
    ifs.read((char*)(&stage),sizeof(stage));
    ifs.close();
    if (stage == ULONG_MAX)
      {
      std::cerr << "Process #" << threadHandle << " exited. exiting ... \n";
      exit(1);
      }
    if ( (((float)(clock() - start_clock)) / CLOCKS_PER_SEC) > m_WaitTimeOutSeconds )
      {
      std::cerr << "Timed out waiting for process #" << threadHandle << " exiting ... \n";
      MultiThreader::Exit();
      }
    }
}

void
MultiThreader
::WaitForSingleMethodThread(ThreadProcessIdType threadHandle)
{
  if (threadHandle == m_ThreadNumber) return;
  else
    {
    WaitForProcess(threadHandle);
    }
}

void MultiThreader::Barrier()
{
  ProcessDone(m_ThreadNumber);
  for( unsigned int thread_loop = 0 ; thread_loop < m_GlobalDefaultNumberOfThreads ; ++thread_loop )
    {
    WaitForSingleMethodThread(thread_loop);
    }
  m_CurrentStage++;
  if (m_CurrentStage == ULONG_MAX)
    {
    std::cerr << "Stage number overflow. exiting ... \n";
    MultiThreader::Exit();
    }
}

void MultiThreader::GetIfstream(std::ifstream & is, ThreadProcessIdType threadHandle)
{
  std::string in_filename = m_DataFilePrefix + std::to_string(threadHandle);
  is.open(in_filename, std::ios::binary);
}

void MultiThreader::GetOfstream(std::ofstream & os, ThreadProcessIdType threadHandle)
{
  std::string out_filename = m_DataFilePrefix + std::to_string(threadHandle);
  os.open(out_filename, std::ios::binary);
}

void MultiThreader::Sync(char * data, std::size_t len)
{
  if (GetThreadNumber() == 0)
    {
    std::ofstream ofs;
    GetOfstream(ofs,0);
    ofs.write(data,len);
    ofs.close();
    Barrier();
    }
  else
    {
    Barrier();
    std::ifstream ifs;
    GetIfstream(ifs,0);
    ifs.read(data,len);
    ifs.close();
    }
  Barrier();
}

void MultiThreader::Exit()
{
  std::ofstream ofs;
  GetOfstream(ofs,GetThreadNumber());
  unsigned long max_ulong = ULONG_MAX;
  ofs.write((char*)(&max_ulong),sizeof(max_ulong));
  exit(1);
}

// Print method for the multithreader
void MultiThreader::PrintSelf(std::ostream & os, Indent indent) const
{
  Superclass::PrintSelf(os, indent);

  os << indent << "Thread Count: " << m_NumberOfThreads << "\n";
  os << indent << "Global Maximum Number Of Threads: "
     << m_GlobalMaximumNumberOfThreads << std::endl;
  os << indent << "Global Default Number Of Threads: "
     << m_GlobalDefaultNumberOfThreads << std::endl;
}

ThreadProcessIdType MultiThreader::GetThreadNumber()
{
  return m_ThreadNumber;
}

}
#endif

