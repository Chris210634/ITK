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

    this->WriteDataToFileWrapper();
    // Wait for other processes to finish
    this->GetMultiThreader()->Barrier();
 
    // Read data from other processes
    this->ReadDataFromFileWrapper();
    // Wait for all parallel processes to catch up.
    // This prevents one process to overwrite its data file before 
    // all other processes get a chance to read it.
    this->GetMultiThreader()->Barrier();

    this->AfterThreadedExecution();
    }
  else
    {
    /* Not processs parallelized */
    this->GetMultiThreader()->SetNumberOfThreads(1);
    this->StartThreadingSequence();
    this->AfterThreadedExecution();
    }
}

template< typename TDomainPartitioner, typename TAssociate >
void
DomainThreader< TDomainPartitioner, TAssociate >
::ReadDataFromFileWrapper()
{
  for (unsigned int threadId = 0 ; threadId < this->m_NumberOfThreadsUsed ; ++threadId)
    {
    if (threadId > MultiThreader::GetLastThreadId() or threadId < MultiThreader::GetFirstThreadId() )
      {
      std::ifstream ifs;
      this->GetMultiThreader()->GetIfstream(ifs, threadId);
      this->ReadDataFromFile(ifs, threadId);
      }
    }
}

template< typename TDomainPartitioner, typename TAssociate >
void
DomainThreader< TDomainPartitioner, TAssociate >
::WriteDataToFileWrapper()
{
  for (unsigned int threadId = MultiThreader::GetFirstThreadId() ;
       threadId <= MultiThreader::GetLastThreadId() ; ++threadId)
    {
    if (threadId < this->m_NumberOfThreadsUsed)
      {
      std::ofstream ofs;
      this->GetMultiThreader()->GetOfstream(ofs, threadId);
      this->WriteDataToFile(ofs, threadId);
      }
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
  //const ThreadIdType threaderNumberOfThreads = this->GetMultiThreader()->GetNumberOfThreads();
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
  const ThreadIdType threadCount = info->NumberOfThreads;
  ThreadIdType threadId = MultiThreader::ConvertThreadId( info->ThreadID, threadCount );

  // Get the sub-domain to process for this thread.
  DomainType subdomain;
  const ThreadIdType total = thisDomainThreader->GetDomainPartitioner()->PartitionDomain(threadId,
                                            threadCount,
                                            thisDomainThreader->m_CompleteDomain,
                                            subdomain);

  // Execute the actual method with appropriate sub-domain.
  // If the threadId is greater than the total number of regions
  // that PartitionDomain will create, don't use this thread.
  // Sometimes the threads dont break up very well and it is just
  // as efficient to leave a few threads idle.
  if ( threadId < total )
    {
    std::cerr << "*** DomainThreader thread # " << threadId << '\n';
    thisDomainThreader->ThreadedExecution( subdomain, threadId );
    }

  return ITK_THREAD_RETURN_VALUE;
}
}

#endif
