// Copyright Kimura Software Inc.

#pragma once

#include "CoreMinimal.h"
#include "HAL/Runnable.h"
#include "HAL/CriticalSection.h"
#include "Templates/Atomic.h"
#include "KimuraSourceControlOperation.h"

//-----------------------------------------------------------------------------
// KimuraSourceControlRunnable
//-----------------------------------------------------------------------------
class KimuraSourceControlRunnable : public FRunnable
{

	friend class FKimuraSourceControlProvider;

protected:

	KimuraSourceControlRunnable();
	virtual ~KimuraSourceControlRunnable();

	// FRunnable interface implementation
	virtual uint32			Run() override;
	virtual void			Stop() override;
	virtual void			Exit() override;

	bool EnqueueOperation(TSharedPtr<KimuraSourceControlOperation, ESPMode::ThreadSafe> op);


protected:

	TAtomic<bool>									StopRequested{ false };
	class FEvent*										ResumeWorkEvent;
	FCriticalSection									QueueMutex;

	// thread safe queue
	TQueue<TSharedPtr<KimuraSourceControlOperation, ESPMode::ThreadSafe>>	OperationQueue;

	void CancelRemainingOperations();

};
