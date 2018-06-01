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
#include "itkMultiThreaderBase.h"
#include "itkPlatformMultiThreader.h"
#include "itkPoolMultiThreader.h"
#include "itkMutexLockHolder.h"
#include "itkNumericTraits.h"
#include "itkSimpleFastMutexLock.h"
#include "itksys/SystemTools.hxx"
#include "itkImageSourceCommon.h"
#include "itkProcessObject.h"
#include <iostream>
#include <string>
#include <algorithm>
#include <cctype>

#if defined(ITK_USE_TBB)
#include "itkTBBMultiThreader.h"
#endif


namespace itk
{
  struct MultiThreaderBaseGlobals
  {
    // Initialize static members.
    MultiThreaderBaseGlobals():GlobalDefaultThreaderTypeIsInitialized(false),
#if defined(ITK_USE_TBB)
    m_GlobalDefaultThreader(MultiThreaderBase::ThreaderType::TBB),
#else
    m_GlobalDefaultThreader(MultiThreaderBase::ThreaderType::Pool),
#endif
    m_GlobalMaximumNumberOfThreads(ITK_MAX_THREADS),
    // Global default number of threads : 0 => Not initialized.
    m_GlobalDefaultNumberOfThreads(0)
    {};
    // GlobalDefaultThreaderTypeIsInitialized is used only in this
    // file to ensure that the ITK_GLOBAL_DEFAULT_THEADER or
    // ITK_USE_THREADPOOL environmenal variables are
    // only used as a fall back option.  If the SetGlobalDefaultThreaderType
    // API is ever used by the developer, the developers choice is
    // respected over the environmental variable.
    bool GlobalDefaultThreaderTypeIsInitialized;
    SimpleFastMutexLock globalDefaultInitializerLock;

    // Global value to control weather the threadpool implementation should
    // be used. This defaults to the environmental variable
    // ITK_GLOBAL_DEFAULT_THEADER. If that is not present, then
    // ITK_USE_THREADPOOL is examined.
    MultiThreaderBase::ThreaderType m_GlobalDefaultThreader;

    // Global variable defining the maximum number of threads that can be used.
    //  The m_GlobalMaximumNumberOfThreads must always be less than or equal to
    //  ITK_MAX_THREADS and greater than zero. */
    ThreadIdType m_GlobalMaximumNumberOfThreads;

    //  Global variable defining the default number of threads to set at
    //  construction time of a MultiThreaderBase instance.  The
    //  m_GlobalDefaultNumberOfThreads must always be less than or equal to the
    //  m_GlobalMaximumNumberOfThreads and larger or equal to 1 once it has been
    //  initialized in the constructor of the first MultiThreaderBase instantiation.
    ThreadIdType m_GlobalDefaultNumberOfThreads;
  };
}//end of itk namespace

namespace itk
{

::itk::MultiThreaderBaseGlobals *
MultiThreaderBase
::GetMultiThreaderBaseGlobals()
{
  if( m_MultiThreaderBaseGlobals == nullptr )
    {
    static auto func = [](void * a){ MultiThreaderBase::SetMultiThreaderBaseGlobals(a); };
    static auto deleteFunc = [](){ delete MultiThreaderBase::m_MultiThreaderBaseGlobals; };
    m_MultiThreaderBaseGlobals = Singleton<MultiThreaderBaseGlobals>("MultiThreaderBaseGlobals", func, deleteFunc);
    }
  return m_MultiThreaderBaseGlobals;
}


void
MultiThreaderBase
::SetMultiThreaderBaseGlobals( void * multiThreaderBaseGlobals )
{
  m_MultiThreaderBaseGlobals = reinterpret_cast<MultiThreaderBaseGlobals *>(multiThreaderBaseGlobals);
}

#if ! defined (ITK_LEGACY_REMOVE)
void MultiThreaderBase::SetGlobalDefaultUseThreadPool( const bool GlobalDefaultUseThreadPool )
{
  if (GlobalDefaultUseThreadPool)
    {
    SetGlobalDefaultThreader(ThreaderType::Pool);
    }
  else
    {
    SetGlobalDefaultThreader(ThreaderType::Platform);
    }
}

bool MultiThreaderBase::GetGlobalDefaultUseThreadPool( )
{
  return (GetGlobalDefaultThreader() == ThreaderType::Pool);
}
#endif

void MultiThreaderBase::SetGlobalDefaultThreader(ThreaderType threaderType)
{
  // This is called once, on-demand to ensure that m_MultiThreaderBaseGlobals is
  // initialized.
  static MultiThreaderBaseGlobals * multiThreaderBaseGlobals = GetMultiThreaderBaseGlobals();
  Unused(multiThreaderBaseGlobals);
  m_MultiThreaderBaseGlobals->m_GlobalDefaultThreader = threaderType;
  m_MultiThreaderBaseGlobals->GlobalDefaultThreaderTypeIsInitialized = true;
}

MultiThreaderBase::ThreaderType
MultiThreaderBase
::GetGlobalDefaultThreader()
{
  // This method must be concurrent thread safe

  // This is called once, on-demand to ensure that m_MultiThreaderBaseGlobals is
  // initialized.
  static MultiThreaderBaseGlobals * multiThreaderBaseGlobals = GetMultiThreaderBaseGlobals();
  Unused(multiThreaderBaseGlobals);

  if( !m_MultiThreaderBaseGlobals->GlobalDefaultThreaderTypeIsInitialized )
    {
    MutexLockHolder< SimpleFastMutexLock > lock(m_MultiThreaderBaseGlobals->globalDefaultInitializerLock);

    // After we have the lock, double check the initialization
    // flag to ensure it hasn't been changed by another thread.
    if (!m_MultiThreaderBaseGlobals->GlobalDefaultThreaderTypeIsInitialized )
      {
      std::string envVar;
      // first check ITK_GLOBAL_DEFAULT_THEADER
      if ( itksys::SystemTools::GetEnv("ITK_GLOBAL_DEFAULT_THEADER", envVar) )
        {
        envVar = itksys::SystemTools::UpperCase(envVar);
        ThreaderType threaderT = ThreaderTypeFromString(envVar);
        if (threaderT != ThreaderType::Unknown)
          {
          MultiThreaderBase::SetGlobalDefaultThreader(threaderT);
          }
        }
      // if that was not set check ITK_USE_THREADPOOL (deprecated)
      else if( !m_MultiThreaderBaseGlobals->GlobalDefaultThreaderTypeIsInitialized
          && itksys::SystemTools::GetEnv("ITK_USE_THREADPOOL",envVar) )
        {
        envVar = itksys::SystemTools::UpperCase(envVar);
        itkGenericOutputMacro("Warning: ITK_USE_THREADPOOL \
has been deprecated since ITK v5.0. \
You should now use ITK_GLOBAL_DEFAULT_THEADER\
\nFor example ITK_GLOBAL_DEFAULT_THEADER=Pool");
        if(envVar != "NO" && envVar != "OFF" && envVar != "FALSE")
          {
          MultiThreaderBase::SetGlobalDefaultThreader(ThreaderType::Pool);
          }
        else
          {
          MultiThreaderBase::SetGlobalDefaultThreader(ThreaderType::Platform);
          }
        }

      // always set that we are initialized
      m_MultiThreaderBaseGlobals->GlobalDefaultThreaderTypeIsInitialized=true;
      }
    }
  return m_MultiThreaderBaseGlobals->m_GlobalDefaultThreader;
}

MultiThreaderBase::ThreaderType
MultiThreaderBase
::ThreaderTypeFromString(std::string threaderString)
{
  threaderString = itksys::SystemTools::UpperCase(threaderString);
  if (threaderString == "PLATFORM")
    {
    return ThreaderType::Platform;
    }
  else if (threaderString == "POOL")
    {
    return ThreaderType::Pool;
    }
  else if (threaderString == "TBB")
    {
    return ThreaderType::TBB;
    }
  else
    {
    return ThreaderType::Unknown;
    }
}

void MultiThreaderBase::SetGlobalMaximumNumberOfThreads(ThreadIdType val)
{
  // This is called once, on-demand to ensure that m_MultiThreaderBaseGlobals is
  // initialized.
  static MultiThreaderBaseGlobals * multiThreaderBaseGlobals = GetMultiThreaderBaseGlobals();
  Unused(multiThreaderBaseGlobals);

  m_MultiThreaderBaseGlobals->m_GlobalMaximumNumberOfThreads = val;

  // clamp between 1 and ITK_MAX_THREADS
  m_MultiThreaderBaseGlobals->m_GlobalMaximumNumberOfThreads =
    std::min( m_MultiThreaderBaseGlobals->m_GlobalMaximumNumberOfThreads,
             (ThreadIdType) ITK_MAX_THREADS );
  m_MultiThreaderBaseGlobals->m_GlobalMaximumNumberOfThreads =
    std::max( m_MultiThreaderBaseGlobals->m_GlobalMaximumNumberOfThreads,
              NumericTraits<ThreadIdType>::OneValue() );

  // If necessary reset the default to be used from now on.
  m_MultiThreaderBaseGlobals->m_GlobalDefaultNumberOfThreads =
    std::min( m_MultiThreaderBaseGlobals->m_GlobalDefaultNumberOfThreads,
              m_MultiThreaderBaseGlobals->m_GlobalMaximumNumberOfThreads);
}

ThreadIdType MultiThreaderBase::GetGlobalMaximumNumberOfThreads()
{
  // This is called once, on-demand to ensure that m_MultiThreaderBaseGlobals is
  // initialized.
  static MultiThreaderBaseGlobals * multiThreaderBaseGlobals = GetMultiThreaderBaseGlobals();
  Unused(multiThreaderBaseGlobals);
  return m_MultiThreaderBaseGlobals->m_GlobalMaximumNumberOfThreads;
}

void MultiThreaderBase::SetGlobalDefaultNumberOfThreads(ThreadIdType val)
{
  // This is called once, on-demand to ensure that m_MultiThreaderBaseGlobals is
  // initialized.
  static MultiThreaderBaseGlobals * multiThreaderBaseGlobals = GetMultiThreaderBaseGlobals();
  Unused(multiThreaderBaseGlobals);

  m_MultiThreaderBaseGlobals->m_GlobalDefaultNumberOfThreads = val;

  // clamp between 1 and m_MultiThreaderBaseGlobals->m_GlobalMaximumNumberOfThreads
  m_MultiThreaderBaseGlobals->m_GlobalDefaultNumberOfThreads  =
    std::min( m_MultiThreaderBaseGlobals->m_GlobalDefaultNumberOfThreads,
              m_MultiThreaderBaseGlobals->m_GlobalMaximumNumberOfThreads );
  m_MultiThreaderBaseGlobals->m_GlobalDefaultNumberOfThreads  =
    std::max( m_MultiThreaderBaseGlobals->m_GlobalDefaultNumberOfThreads,
              NumericTraits<ThreadIdType>::OneValue() );

}

void MultiThreaderBase::SetNumberOfThreads(ThreadIdType numberOfThreads)
{
  if( m_NumberOfThreads == numberOfThreads &&
      numberOfThreads <= m_MultiThreaderBaseGlobals->m_GlobalMaximumNumberOfThreads )
    {
    return;
    }

  m_NumberOfThreads = numberOfThreads;

  // clamp between 1 and m_MultiThreaderBaseGlobals->m_GlobalMaximumNumberOfThreads
  m_NumberOfThreads  = std::min( m_NumberOfThreads,
                                 m_MultiThreaderBaseGlobals->m_GlobalMaximumNumberOfThreads );
  m_NumberOfThreads  = std::max( m_NumberOfThreads, NumericTraits<ThreadIdType>::OneValue() );

}

ThreadIdType MultiThreaderBase::GetGlobalDefaultNumberOfThreads()
{
  // This is called once, on-demand to ensure that m_MultiThreaderBaseGlobals is
  // initialized.
  static MultiThreaderBaseGlobals * multiThreaderBaseGlobals =
    GetMultiThreaderBaseGlobals();
  Unused(multiThreaderBaseGlobals);

  if( m_MultiThreaderBaseGlobals->m_GlobalDefaultNumberOfThreads == 0 ) //need to initialize
    {
    m_MultiThreaderBaseGlobals->m_GlobalDefaultNumberOfThreads = ThreadPool::GetGlobalDefaultNumberOfThreads();
    }
  return m_MultiThreaderBaseGlobals->m_GlobalDefaultNumberOfThreads;
}

MultiThreaderBase::Pointer MultiThreaderBase::New()
{
  Pointer smartPtr = ::itk::ObjectFactory< MultiThreaderBase >::Create();
  if ( smartPtr == nullptr )
    {
    ThreaderType threaderType = GetGlobalDefaultThreader();
    switch (threaderType)
      {
      case ThreaderType::Platform:
        return PlatformMultiThreader::New();
      case ThreaderType::Pool:
        return PoolMultiThreader::New();
      case ThreaderType::TBB:
#if defined(ITK_USE_TBB)
        return TBBMultiThreader::New();
#else
        itkGenericExceptionMacro("ITK has been built without TBB support!");
#endif
      default:
        itkGenericExceptionMacro("MultiThreaderBase::GetGlobalDefaultThreader returned Unknown!");
      }
    }
  smartPtr->UnRegister();
  return smartPtr;
}


MultiThreaderBase::MultiThreaderBase()
{
  m_NumberOfThreads = MultiThreaderBase::GetGlobalDefaultNumberOfThreads();
}

MultiThreaderBase::~MultiThreaderBase()
{
}

ITK_THREAD_RETURN_TYPE
MultiThreaderBase
::SingleMethodProxy(void *arg)
{
  // grab the ThreadInfoStruct originally prescribed
  auto * threadInfoStruct = static_cast<MultiThreaderBase::ThreadInfoStruct *>( arg );

  // execute the user specified threader callback, catching any exceptions
  try
    {
    ( *threadInfoStruct->ThreadFunction )(arg);
    threadInfoStruct->ThreadExitCode = ThreadInfoStruct::SUCCESS;
    }
  catch( ProcessAborted & )
    {
    threadInfoStruct->ThreadExitCode = ThreadInfoStruct::ITK_PROCESS_ABORTED_EXCEPTION;
    }
  catch( ExceptionObject & )
    {
    threadInfoStruct->ThreadExitCode = ThreadInfoStruct::ITK_EXCEPTION;
    }
  catch( std::exception & )
    {
    threadInfoStruct->ThreadExitCode = ThreadInfoStruct::STD_EXCEPTION;
    }
  catch( ... )
    {
    threadInfoStruct->ThreadExitCode = ThreadInfoStruct::UNKNOWN;
    }

  return ITK_THREAD_RETURN_VALUE;
}

void
MultiThreaderBase
::ParallelizeArray(
  SizeValueType firstIndex,
  SizeValueType lastIndexPlus1,
  ArrayThreadingFunctorType aFunc,
  ProcessObject* filter )
{
  // This implementation simply delegates parallelization to the old interface
  // SetSingleMethod+SingleMethodExecute. This method is meant to be overloaded!
  if (filter)
    {
    filter->UpdateProgress(0.0f);
    }

  if ( firstIndex + 1 < lastIndexPlus1 )
    {
    struct ArrayCallback acParams {
        aFunc,
        firstIndex,
        lastIndexPlus1,
        filter,
        std::this_thread::get_id(),
        {0} };
    this->SetSingleMethod(&MultiThreaderBase::ParallelizeArrayHelper, &acParams);
    this->SingleMethodExecute();
    }
  else if ( firstIndex + 1 == lastIndexPlus1 )
    {
    aFunc( firstIndex );
    }
  // else nothing needs to be executed

  if (filter)
    {
    filter->UpdateProgress(1.0f);
    if (filter->GetAbortGenerateData())
      {
      std::string msg;
      ProcessAborted e(__FILE__, __LINE__);
      msg += "AbortGenerateData was called in " + std::string(filter->GetNameOfClass() )
          + " during multi-threaded part of filter execution";
      e.SetDescription(msg);
      throw e;
      }
    }
}

ITK_THREAD_RETURN_TYPE
MultiThreaderBase
::ParallelizeArrayHelper(void * arg)
{
  using ThreadInfo = MultiThreaderBase::ThreadInfoStruct;
  auto* threadInfo = static_cast< ThreadInfo* >( arg );
  ThreadIdType threadId = threadInfo->ThreadID;
  ThreadIdType threadCount = threadInfo->NumberOfThreads;
  auto* acParams = static_cast< struct ArrayCallback* >( threadInfo->UserData );

  if ( acParams->filter && acParams->filter->GetAbortGenerateData() )
    {
    std::string msg;
    ProcessAborted e( __FILE__, __LINE__ );
    msg += "AbortGenerateData was called in " + std::string( acParams->filter->GetNameOfClass() )
        + " during multi-threaded part of filter execution";
    e.SetDescription(msg);
    throw e;
    }

  SizeValueType range = acParams->lastIndexPlus1 - acParams->firstIndex;
  double fraction = double( range ) / threadCount;
  SizeValueType first = acParams->firstIndex + fraction * threadId;
  SizeValueType afterLast = acParams->firstIndex + fraction * ( threadId + 1 );
  if ( threadId == threadCount - 1 ) // last thread
    {
    // Avoid possible problems due to floating point arithmetic
    afterLast = acParams->lastIndexPlus1;
    }

  for ( SizeValueType i = first; i < afterLast; i++ )
    {
    acParams->functor( i );
    if ( acParams->filter )
      {
      ++acParams->progress;
      //make sure we are updating progress only from the thead which invoked us
      if ( acParams->callingThread == std::this_thread::get_id() )
        {
        acParams->filter->UpdateProgress( float( acParams->progress ) / range );
        }
      }
    }

  return ITK_THREAD_RETURN_VALUE;
}

void
MultiThreaderBase
::ParallelizeImageRegion(
    unsigned int dimension,
    const IndexValueType index[],
    const SizeValueType size[],
    MultiThreaderBase::ThreadingFunctorType funcP,
    ProcessObject* filter)
{
  // This implementation simply delegates parallelization to the old interface
  // SetSingleMethod+SingleMethodExecute. This method is meant to be overloaded!
  if (filter)
    {
    filter->UpdateProgress(0.0f);
    }

  SizeValueType pixelCount = 1;
  for (unsigned d = 0; d < dimension; d++)
    {
    pixelCount *= size[d];
    }
  struct RegionAndCallback rnc {
      funcP,
      dimension,
      index,
      size,
      filter,
      std::this_thread::get_id(),
      pixelCount,
      {0} };
  this->SetSingleMethod(&MultiThreaderBase::ParallelizeImageRegionHelper, &rnc);
  this->SingleMethodExecute();

  if (filter)
    {
    filter->UpdateProgress(1.0f);
    if (filter->GetAbortGenerateData())
      {
      std::string msg;
      ProcessAborted e(__FILE__, __LINE__);
      msg += "AbortGenerateData was called in " + std::string(filter->GetNameOfClass() )
          + " during multi-threaded part of filter execution";
      e.SetDescription(msg);
      throw e;
      }
    }
}

ITK_THREAD_RETURN_TYPE
MultiThreaderBase
::ParallelizeImageRegionHelper(void * arg)
{
  using ThreadInfo = MultiThreaderBase::ThreadInfoStruct;
  auto * threadInfo = static_cast<ThreadInfo *>(arg);
  ThreadIdType threadId = threadInfo->ThreadID;
  ThreadIdType threadCount = threadInfo->NumberOfThreads;
  auto * rnc = static_cast<struct RegionAndCallback *>(threadInfo->UserData);

  const ImageRegionSplitterBase * splitter = ImageSourceCommon::GetGlobalDefaultSplitter();
  ImageIORegion region(rnc->dimension);
  for (unsigned d = 0; d < rnc->dimension; d++)
    {
    region.SetIndex(d, rnc->index[d]);
    region.SetSize(d, rnc->size[d]);
    }
  ThreadIdType total = splitter->GetSplit(threadId, threadCount, region);

  if (rnc->filter && rnc->filter->GetAbortGenerateData())
    {
    std::string msg;
    ProcessAborted e(__FILE__, __LINE__);
    msg += "AbortGenerateData was called in " + std::string(rnc->filter->GetNameOfClass() )
        + " during multi-threaded part of filter execution";
    e.SetDescription(msg);
    throw e;
    }

  if ( threadId < total )
    {
    rnc->functor(&region.GetIndex()[0], &region.GetSize()[0]);
    if (rnc->filter)
      {
      SizeValueType pixelCount = region.GetNumberOfPixels();
      rnc->pixelProgress += pixelCount;
      //make sure we are updating progress only from the thead which invoked filter->Update();
      if (rnc->callingThread == std::this_thread::get_id())
        {
        rnc->filter->UpdateProgress(float(rnc->pixelProgress) / rnc->pixelCount);
        }
      }
    }

  return ITK_THREAD_RETURN_VALUE;
}

std::ostream& operator << (std::ostream& os,
    const MultiThreaderBase::ThreaderType& threader)
{
  os << MultiThreaderBase::ThreaderTypeToString(threader) << "MultiThreader";
  return os;
}

// Print method for the multithreader
void MultiThreaderBase::PrintSelf(std::ostream & os, Indent indent) const
{
  Superclass::PrintSelf(os, indent);

  os << indent << "Number of Threads: " << m_NumberOfThreads << "\n";
  os << indent << "Global Maximum Number Of Threads: "
     << m_MultiThreaderBaseGlobals->m_GlobalMaximumNumberOfThreads << std::endl;
  os << indent << "Global Default Number Of Threads: "
     << m_MultiThreaderBaseGlobals->m_GlobalDefaultNumberOfThreads << std::endl;
  os << indent << "Global Default Threader Type: "
     << m_MultiThreaderBaseGlobals->m_GlobalDefaultThreader << std::endl;
  os << indent << "SingleMethod: " << m_SingleMethod << std::endl;
  os << indent << "SingleData: " << m_SingleData << std::endl;
}

MultiThreaderBaseGlobals * MultiThreaderBase::m_MultiThreaderBaseGlobals;

}
