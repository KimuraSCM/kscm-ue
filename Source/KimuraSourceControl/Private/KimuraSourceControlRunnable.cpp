// Copyright Kimura Software Inc.

#include "KimuraSourceControlRunnable.h"
#include "Misc/ScopeLock.h"

//-----------------------------------------------------------------------------
// KimuraSourceControlRunnable::KimuraSourceControlRunnable
//-----------------------------------------------------------------------------
KimuraSourceControlRunnable::KimuraSourceControlRunnable()
{
	this->ResumeWorkEvent = FPlatformProcess::GetSynchEventFromPool(false);
	if (this->ResumeWorkEvent != nullptr)
	{
		this->ResumeWorkEvent->Reset();
	}
	else
	{
		this->StopRequested = true;
	}
}


//-----------------------------------------------------------------------------
// KimuraSourceControlRunnable::~KimuraSourceControlRunnable
//-----------------------------------------------------------------------------
KimuraSourceControlRunnable::~KimuraSourceControlRunnable()
{
	FScopeLock QueueLock(&this->QueueMutex);
	this->StopRequested = true;
	this->CancelRemainingOperations();

	if (this->ResumeWorkEvent != nullptr)
	{
		FPlatformProcess::ReturnSynchEventToPool(this->ResumeWorkEvent);
		this->ResumeWorkEvent = nullptr;
	}
}


//-----------------------------------------------------------------------------
// KimuraSourceControlRunnable::Run
//-----------------------------------------------------------------------------
uint32 KimuraSourceControlRunnable::Run()
{
	while (!this->StopRequested)
	{
		if (this->ResumeWorkEvent != nullptr)
		{
			this->ResumeWorkEvent->Wait();
		}

		if (this->StopRequested)
		{
			break;
		}

		// Select and execute operations one at a time. 
		while (!this->StopRequested)
		{
			TSharedPtr<KimuraSourceControlOperation, ESPMode::ThreadSafe> op;
			{
				FScopeLock QueueLock(&this->QueueMutex);
				if (this->StopRequested || !this->OperationQueue.Dequeue(op))
				{
					break;
				}
			}

			if (op.IsValid())
			{
				op->Execute();
			}
		}
	}

	return 0;
}


//-----------------------------------------------------------------------------
// KimuraSourceControlRunnable::Stop
//-----------------------------------------------------------------------------
void KimuraSourceControlRunnable::Stop()
{
	{
		FScopeLock QueueLock(&this->QueueMutex);
		this->StopRequested = true;
		this->CancelRemainingOperations();
	}

	if (this->ResumeWorkEvent != nullptr)
	{
		this->ResumeWorkEvent->Trigger();
	}
}


//-----------------------------------------------------------------------------
// KimuraSourceControlRunnable::Exit
//-----------------------------------------------------------------------------
void KimuraSourceControlRunnable::Exit()
{

}

//-----------------------------------------------------------------------------
// KimuraSourceControlRunnable::EnqueueOperation
//-----------------------------------------------------------------------------
bool KimuraSourceControlRunnable::EnqueueOperation(TSharedPtr<KimuraSourceControlOperation, ESPMode::ThreadSafe> op)
{
	if (!op.IsValid())
	{
		return false;
	}

	{
		FScopeLock QueueLock(&this->QueueMutex);
		if (this->StopRequested)
		{
			op->MarkCanceled();
			return false;
		}

		this->OperationQueue.Enqueue(op);
	}

	if (this->ResumeWorkEvent != nullptr)
	{
		this->ResumeWorkEvent->Trigger();
	}

	return true;
}


//-----------------------------------------------------------------------------
// KimuraSourceControlRunnable::CancelRemainingOperations
//-----------------------------------------------------------------------------
void KimuraSourceControlRunnable::CancelRemainingOperations()
{
	TSharedPtr<KimuraSourceControlOperation, ESPMode::ThreadSafe> op;
	while (this->OperationQueue.Dequeue(op))
	{
		if (op.IsValid())
		{
			op->MarkCanceled();
		}
	}

}
