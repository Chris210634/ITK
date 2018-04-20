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
#ifndef itkDomainThreader_hxx
#define itkDomainThreader_hxx

#include "itkDomainThreader.h"

namespace itk
{
template< typename TDomainPartitioner, typename TAssociate >
DomainThreader< TDomainPartitioner, TAssociate >
::DomainThreader()
{
  this->m_DomainPartitioner   = DomainPartitionerType::New();
  this->m_MultiThreader       = MultiThreader::New();
  this->m_NumberOfThreadsUsed = 0;
  this->m_Associate           = ITK_NULLPTR;
}

template< typename TDomainPartitioner, typename TAssociate >
DomainThreader< TDomainPartitioner, TAssociate >
::~DomainThreader()
{
}

template< typename TDomainPartitioner, typename TAssociate >
MultiThreader *
DomainThreader< TDomainPartitioner, TAssociate >
::GetMultiThreader() const
{
  return this->m_MultiThreader;
}

template< typename TDomainPartitioner, typename TAssociate >
ThreadIdType
DomainThreader< TDomainPartitioner, TAssociate >
::GetMaximumNumberOfThreads() const
{
  return this->m_MultiThreader->GetNumberOfThreads();
}

template< typename TDomainPartitioner, typename TAssociate >
void
DomainThreader< TDomainPartitioner, TAssociate >
::SetMaximumNumberOfThreads( const ThreadIdType threads )
{
  if( threads != this->GetMaximumNumberOfThreads() )
    {
    this->m_MultiThreader->SetNumberOfThreads( threads );
    this->Modified();
    }
}

#ifdef ITK_USE_PARALLEL_PROCESSES
template< typename TDomainPartitioner, typename TAssociate >
void
DomainThreader< TDomainPartitioner, TAssociate >
::Execute( TAssociate * enclosingClass, const DomainType & completeDomain )
{
  this->m_Associate = enclosingClass;
  this->m_CompleteDomain = completeDomain;

  this->DetermineNumberOfThreadsUsed();

  // This function will set number of threads correctly if process parallelized.
  this->BeforeThreadedExecution();

  if ( this -> IsProcessParallelized() )
    {
    // This calls ThreadedExecution in each thread.
    this->StartThreadingSequence();

    this->GetMultiThreader()->GlobalBarrier();

    this->AfterThreadedExecution();
    }
  else
    {
    /* Not processs parallelized */
    this->GetMultiThreader()->SetNumberOfThreads(MultiThreader::GetThreadsPerWorker());
    //this->GetMultiThreader()->SetNumberOfThreads(1);
    this->StartThreadingSequence();
    this->AfterThreadedExecution();
    }
}

template< typename TDomainPartitioner, typename TAssociate >
void
DomainThreader< TDomainPartitioner, TAssociate >
::ReadDataFromFileWrapper(unsigned int localThreadId)
{
  unsigned int threadsPerWorker = MultiThreader::GetThreadsPerWorker();
  unsigned int numberOfThreadsWritten = 0;
  if ( MultiThreader::GetFirstThreadId() < this->m_NumberOfThreadsUsed )
    {
    numberOfThreadsWritten = this->m_NumberOfThreadsUsed - MultiThreader::GetFirstThreadId();
    }
  if ( numberOfThreadsWritten > threadsPerWorker)
    {
    numberOfThreadsWritten = threadsPerWorker;
    }
  unsigned int numberOfThreadsToRead = this->m_NumberOfThreadsUsed - numberOfThreadsWritten;
  MultiThreader::ThreadBlock tb = MultiThreader::DistributeJobsEvenly(
                                                       threadsPerWorker,
                                                       numberOfThreadsToRead,
                                                       localThreadId);

  for (unsigned int k = tb.firstIndex ; k < tb.firstIndex + tb.length ; ++ k )
    {
    unsigned int globalThreadId = MultiThreader::MapIndexToGlobalThreadId(k,
                                                           MultiThreader::GetFirstThreadId(),
                                                           MultiThreader::GetLastThreadId() );
    BufferedIfstream ifs;
    this->GetMultiThreader()->GetIfstream(ifs, globalThreadId, localThreadId);
    this->ReadDataFromFile(ifs, globalThreadId);
    }
}

#else
template< typename TDomainPartitioner, typename TAssociate >
void
DomainThreader< TDomainPartitioner, TAssociate >
::Execute( TAssociate * enclosingClass, const DomainType & completeDomain )
{
  this->m_Associate = enclosingClass;
  this->m_CompleteDomain = completeDomain;

  this->DetermineNumberOfThreadsUsed();

  this->BeforeThreadedExecution();

  // This calls ThreadedExecution in each thread.
  this->StartThreadingSequence();

  this->AfterThreadedExecution();
}
#endif

template< typename TDomainPartitioner, typename TAssociate >
bool
DomainThreader< TDomainPartitioner, TAssociate >
::IsProcessParallelized() const
{
  return false;
}

template< typename TDomainPartitioner, typename TAssociate >
void
DomainThreader< TDomainPartitioner, TAssociate >
::DetermineNumberOfThreadsUsed()
{
  const ThreadIdType threaderNumberOfThreads = this->GetMultiThreader()->GetTotalNumberOfThreads();

  // Attempt a single dummy partition, just to get the number of subdomains actually created
  DomainType subdomain;
  this->m_NumberOfThreadsUsed = this->m_DomainPartitioner->PartitionDomain(0,
                                            threaderNumberOfThreads,
                                            this->m_CompleteDomain,
                                            subdomain);

  if( this->m_NumberOfThreadsUsed < threaderNumberOfThreads )
    {
    // If PartitionDomain is only able to create a lesser number of subdomains,
    // ensure that superfluous threads aren't created
    // DomainThreader::SetMaximumNumberOfThreads *should* already have been called by this point,
    // but it's not fatal if it somehow gets called later
    this->GetMultiThreader()->SetNumberOfThreads(this->m_NumberOfThreadsUsed);
    }
  else if( this->m_NumberOfThreadsUsed > threaderNumberOfThreads )
    {
    itkExceptionMacro( "A subclass of ThreadedDomainPartitioner::PartitionDomain"
                      << "returned more subdomains than were requested" );
    }
}

template< typename TDomainPartitioner, typename TAssociate >
void
DomainThreader< TDomainPartitioner, TAssociate >
::StartThreadingSequence()
{
  // Set up the multithreaded processing
  ThreadStruct str;
  str.domainThreader = this;

  MultiThreader* multiThreader = this->GetMultiThreader();
  multiThreader->SetSingleMethod(this->ThreaderCallback, &str);

  // multithread the execution
  multiThreader->SingleMethodExecute();
}

template< typename TDomainPartitioner, typename TAssociate >
ITK_THREAD_RETURN_TYPE
DomainThreader< TDomainPartitioner, TAssociate >
::ThreaderCallback( void* arg )
{
  MultiThreader::ThreadInfoStruct* info = static_cast<MultiThreader::ThreadInfoStruct *>(arg);
  ThreadStruct *str = static_cast<ThreadStruct *>(info->UserData);
  DomainThreader *thisDomainThreader = str->domainThreader;
  ThreadIdType  total, localThreadId, globalThreadId, threadCount;
  DomainType subdomain;
  localThreadId = info->ThreadID;

  if ( not thisDomainThreader->IsProcessParallelized() or MultiThreader::GetNumberOfWorkers() == 1)
    {
    threadCount = MultiThreader::GetThreadsPerWorker();
    total = thisDomainThreader->GetDomainPartitioner()->PartitionDomain(localThreadId,
                                            threadCount,
                                            thisDomainThreader->m_CompleteDomain,
                                            subdomain);
    if ( localThreadId < total )
      {
      thisDomainThreader->ThreadedExecution( subdomain, localThreadId );
      }
    return ITK_THREAD_RETURN_VALUE;
    }

  // Process Parallelized and MultiWorker Execution
  threadCount = info->NumberOfThreads;
  globalThreadId = MultiThreader::ConvertThreadId( localThreadId, threadCount );
  total = thisDomainThreader->GetDomainPartitioner()->PartitionDomain(globalThreadId,
                                            threadCount,
                                            thisDomainThreader->m_CompleteDomain,
                                            subdomain);
  if ( globalThreadId < total )
    {
    thisDomainThreader->ThreadedExecution( subdomain, globalThreadId );
    BufferedOfstream ofs;
    MultiThreader::GetOfstream(ofs, globalThreadId, localThreadId);
    thisDomainThreader->WriteDataToFile(ofs, globalThreadId);
    }
  MultiThreader::ThreadedBarrier(localThreadId);
  thisDomainThreader->ReadDataFromFileWrapper(localThreadId);
  return ITK_THREAD_RETURN_VALUE;
}
}

#endif
