// Copyright Kimura Software Inc.

#include "KimuraSourceControlProvider.h"
#include "KimuraSourceControlModule.h"
#include "KimuraSourceControlRunnable.h"
#include "KimuraSourceControlState.h"
#include "KimuraSourceControlChangelistState.h"
#include "SKimuraSourceControlSettings.h"
#include "Logging/MessageLog.h"
#include "SourceControlHelpers.h"
#include "ScopedSourceControlProgress.h"
#include "SourceControlOperations.h"
#include "KimuraSourceControlOperation.h"
#include "Misc/ScopeLock.h"
#include "ISourceControlModule.h"
#include "LevelEditor.h"
#include "ILevelEditor.h"
#include "ISceneOutliner.h"
#include "Modules/ModuleManager.h"

#if PLATFORM_WINDOWS

#pragma warning(push)
#pragma warning(disable : 4005)	// Disable macro redefinition warning for compatibility with Windows SDK 8+

#include "Windows/AllowWindowsPlatformTypes.h"
#include <Windows.h>
#include <shellapi.h>
#include <winreg.h>
#include "Windows/HideWindowsPlatformTypes.h"

#pragma warning(pop)

#endif

#define LOCTEXT_NAMESPACE "FKimuraSourceControlProvider"


//-----------------------------------------------------------------------------
// FKimuraSourceControlProvider::Init
//-----------------------------------------------------------------------------
void FKimuraSourceControlProvider::Init(bool bForceConnection /*= true*/)
{
	UE_LOG(LogKimuraSCM, Log, TEXT("Kimura SCM Init"));

	// these are the operations currently supported by Kimura.
	this->SupportedOperations.Add(FName(TEXT("Connect")));
	this->SupportedOperations.Add(FName(TEXT("UpdateStatus")));
	this->SupportedOperations.Add(FName(TEXT("MarkForAdd")));
	this->SupportedOperations.Add(FName(TEXT("CheckOut")));
	this->SupportedOperations.Add(FName(TEXT("Delete")));
	this->SupportedOperations.Add(FName(TEXT("CheckIn")));
	this->SupportedOperations.Add(FName(TEXT("Revert")));
	this->SupportedOperations.Add(FName(TEXT("RevertUnchanged")));
	this->SupportedOperations.Add(FName(TEXT("Sync")));
	this->SupportedOperations.Add(FName(TEXT("UpdateChangelistsStatus")));
	this->SupportedOperations.Add(FName(TEXT("NewChangelist")));
	this->SupportedOperations.Add(FName(TEXT("DeleteChangelist")));
	this->SupportedOperations.Add(FName(TEXT("EditChangelist")));
	this->SupportedOperations.Add(FName(TEXT("MoveToChangelist")));
	this->SupportedOperations.Add(FName(TEXT("GetWorkspaces")));
	this->SupportedOperations.Add(FName(TEXT("Save")));
	this->SupportedOperations.Add(FName(TEXT("Where")));

	if (this->KimuraRunnableInstance == nullptr)
	{
		// Create a thread to handle source control operations asynchronously.
		this->KimuraRunnableInstance = new KimuraSourceControlRunnable();
		this->RunnableThread = FRunnableThread::Create(this->KimuraRunnableInstance,
			TEXT("Kimura SCM Thread"),
			1 * 1024 * 1024,
			EThreadPriority::TPri_Normal,
			FPlatformAffinity::GetNoAffinityMask());

		if (this->RunnableThread == nullptr)
		{
			UE_LOG(LogKimuraSCM, Error, TEXT("Failed to create the Kimura SCM worker thread."));
			delete this->KimuraRunnableInstance;
			this->KimuraRunnableInstance = nullptr;
			this->Enabled = false;
			return;
		}
	}

	// Kimura source control is enabled whenever a valid installation is detected.
	this->Enabled = FKimuraSourceControlModule::Get().AccessWorkspaceHost().IsValid();

	// auto-connect?
	if (bForceConnection && this->Enabled)
	{
		TSharedRef<FConnect, ESPMode::ThreadSafe> connect = ISourceControlOperation::Create<FConnect>();
		this->Execute(connect, nullptr, TArray<FString>(), EConcurrency::Asynchronous);
	}

}

//-----------------------------------------------------------------------------
// FKimuraSourceControlProvider::Close
//-----------------------------------------------------------------------------
void FKimuraSourceControlProvider::Close()
{
	UE_LOG(LogKimuraSCM, Log, TEXT("Kimura SCM Close"));

	this->Available = false;
	this->Enabled = false;

	{
		FScopeLock AdmissionLock(&this->OperationAdmissionMutex);
		this->ResetConnectionState();
		if (this->KimuraRunnableInstance != nullptr)
		{
			this->KimuraRunnableInstance->Stop();
		}
	}

	if (this->RunnableThread != nullptr)
	{
		this->RunnableThread->WaitForCompletion();
		delete this->RunnableThread;
		this->RunnableThread = nullptr;
	}

	if (this->KimuraRunnableInstance != nullptr)
	{
		delete this->KimuraRunnableInstance;
		this->KimuraRunnableInstance = nullptr;
	}

	this->StateCache.Empty();
	this->StateCachePerWUID.Empty();
	this->PendingStatusFiles.Empty();
	this->ChangeListStateCache.Empty();
	this->RepositoryLinks.Empty();

	this->OngoingSourceControlOperations.Empty();
	this->ResetConnectionState();

	this->AvailableRevisionsPerWUID.Empty();
	this->FilesNotAtLatestRevision.Empty();

	this->WorkspaceDescription = FKimuraWorkspaceDesc();

	this->FileOpTimeStamp = 0;
	this->LatestKnownCLID = 0;
	this->Dirty = false;
	this->OnSourceControlStateChanged.Broadcast();

}

//-----------------------------------------------------------------------------
// FKimuraSourceControlProvider::GetName
//-----------------------------------------------------------------------------
const FName& FKimuraSourceControlProvider::GetName(void) const
{
	static FName KimuraName("Kimura SCM");
	return KimuraName;
}

//-----------------------------------------------------------------------------
// FKimuraSourceControlProvider::GetStatusText
//-----------------------------------------------------------------------------
FText FKimuraSourceControlProvider::GetStatusText() const
{
	const TMap<ISourceControlProvider::EStatus, FString> statusValues = this->GetStatus();
	const auto GetStatusValue = [&statusValues](ISourceControlProvider::EStatus Status) -> FString
	{
		const FString* Value = statusValues.Find(Status);
		if (Value != nullptr)
		{
			return *Value;
		}
		return FString();
	};

	FString status;
	status += FString::Format(TEXT("Enabled: {0}\n"), { GetStatusValue(ISourceControlProvider::EStatus::Enabled) });
	status += FString::Format(TEXT("Available: {0}\n"), { GetStatusValue(ISourceControlProvider::EStatus::Connected) });
	status += FString::Format(TEXT("Workspace: {0}\n"), { GetStatusValue(ISourceControlProvider::EStatus::Workspace) });
	status += FString::Format(TEXT("Workspace path: {0}\n"), { GetStatusValue(ISourceControlProvider::EStatus::WorkspacePath) });
	status += FString::Format(TEXT("Server: {0}\n"), { GetStatusValue(ISourceControlProvider::EStatus::Remote) });
	status += FString::Format(TEXT("Username: {0}\n"), { GetStatusValue(ISourceControlProvider::EStatus::User) });

	return FText::FromString(status);
}

//-----------------------------------------------------------------------------
// FKimuraSourceControlProvider::GetStatus
//-----------------------------------------------------------------------------
TMap<ISourceControlProvider::EStatus, FString> FKimuraSourceControlProvider::GetStatus() const
{
	TMap<ISourceControlProvider::EStatus, FString> statuses;
	const FKimuraSourceControlSettings& settings = FKimuraSourceControlModule::AccessSettings();

	statuses.Add(ISourceControlProvider::EStatus::Enabled, this->IsEnabled() ? FString("yes") : FString("no"));
	const bool bSourceControlEnabled = ISourceControlModule::Get().IsEnabled();
	statuses.Add(ISourceControlProvider::EStatus::Connected, (bSourceControlEnabled && this->IsAvailable()) ? FString("yes") : FString("no"));
	statuses.Add(ISourceControlProvider::EStatus::Remote, settings.ServerAddress);
	statuses.Add(ISourceControlProvider::EStatus::User, this->WorkspaceDescription.Username.IsEmpty() ? settings.UserName : this->WorkspaceDescription.Username);
	statuses.Add(ISourceControlProvider::EStatus::Workspace, this->WorkspaceDescription.Name);
	statuses.Add(ISourceControlProvider::EStatus::WorkspacePath, this->WorkspaceDescription.WorkspacePath);

	return statuses;
}

//-----------------------------------------------------------------------------
// FKimuraSourceControlProvider::IsEnabled
//-----------------------------------------------------------------------------
bool FKimuraSourceControlProvider::IsEnabled() const
{
	return this->Enabled && FKimuraSourceControlModule::AccessWorkspaceHost().IsValid();
}

//-----------------------------------------------------------------------------
// FKimuraSourceControlProvider::IsAvailable
//-----------------------------------------------------------------------------
bool FKimuraSourceControlProvider::IsAvailable() const
{
	return this->Available && FKimuraSourceControlModule::AccessWorkspaceHost().IsValid();
}

//-----------------------------------------------------------------------------
// FKimuraSourceControlProvider::QueryStateBranchConfig
//-----------------------------------------------------------------------------
bool FKimuraSourceControlProvider::QueryStateBranchConfig(const FString& ConfigSrc, const FString& ConfigDest)
{
	return false;
}

//-----------------------------------------------------------------------------
// FKimuraSourceControlProvider::RegisterStateBranches
//-----------------------------------------------------------------------------
void FKimuraSourceControlProvider::RegisterStateBranches(const TArray<FString>& BranchNames, const FString& ContentRootIn)
{
}

//-----------------------------------------------------------------------------
// FKimuraSourceControlProvider::GetStateBranchIndex
//-----------------------------------------------------------------------------
int32 FKimuraSourceControlProvider::GetStateBranchIndex(const FString& BranchName) const
{
	return 0;
}

//-----------------------------------------------------------------------------
// FKimuraSourceControlProvider::GetState
//-----------------------------------------------------------------------------
ECommandResult::Type FKimuraSourceControlProvider::GetState(const TArray<FString>& InFiles, TArray< TSharedRef<ISourceControlState, ESPMode::ThreadSafe> >& OutState, EStateCacheUsage::Type InStateCacheUsage)
{
	if (!IsAvailable())
	{
		return ECommandResult::Failed;
	}

	TArray<FString> AbsoluteFiles = SourceControlHelpers::AbsoluteFilenames(InFiles);

	if (InStateCacheUsage == EStateCacheUsage::ForceUpdate)
	{
		this->Execute(ISourceControlOperation::Create<FUpdateStatus>(), nullptr, AbsoluteFiles);
	}

	for (TArray<FString>::TConstIterator It(AbsoluteFiles); It; It++)
	{
		// Note: Files that are not under the workspace are NOT ignored.

		FString s = *It;
		OutState.Add(this->GetOrCreateState(*It));
	}

	return ECommandResult::Succeeded;                  
}

//-----------------------------------------------------------------------------
// FKimuraSourceControlProvider::GetState
//-----------------------------------------------------------------------------
ECommandResult::Type FKimuraSourceControlProvider::GetState(const TArray<FSourceControlChangelistRef>& InChangelists, TArray<FSourceControlChangelistStateRef>& OutState, EStateCacheUsage::Type InStateCacheUsage)
{
	if (!IsAvailable())
	{
		return ECommandResult::Failed;
	}

	if (InStateCacheUsage == EStateCacheUsage::ForceUpdate)
	{
		TSharedRef<FUpdatePendingChangelistsStatus, ESPMode::ThreadSafe> updateChangelist = ISourceControlOperation::Create<FUpdatePendingChangelistsStatus>();
		updateChangelist.Get().SetUpdateAllChangelists(true);
		updateChangelist.Get().SetUpdateFilesStates(true);
		updateChangelist.Get().SetUpdateShelvedFilesStates(false);

		this->Execute(updateChangelist, nullptr, TArray<FString>(), EConcurrency::Synchronous);
	}

	for (FSourceControlChangelistRef Changelist : InChangelists)
	{
		FString id = Changelist.Get().GetIdentifier();

		TSharedRef<FKimuraSourceControlChangelistState, ESPMode::ThreadSafe>* State = this->ChangeListStateCache.Find(MakeChangeListCacheKey(id));
		if (State != NULL)
		{
			// found cached item
			OutState.Add(*State);
		}
	}

	return ECommandResult::Succeeded;
}

//-----------------------------------------------------------------------------
// FKimuraSourceControlProvider::GetCachedStateByPredicate
//-----------------------------------------------------------------------------
TArray<FSourceControlStateRef> FKimuraSourceControlProvider::GetCachedStateByPredicate(TFunctionRef<bool(const FSourceControlStateRef&)> Predicate) const
{
	TArray<FSourceControlStateRef> Result;
	for (const auto& CacheItem : StateCache)
	{
		FSourceControlStateRef State = CacheItem.Value;
		if (Predicate(State))
		{
			Result.Add(State);
		}
	}
	return Result;
}

//-----------------------------------------------------------------------------
// FKimuraSourceControlProvider::RegisterSourceControlStateChanged_Handle
//-----------------------------------------------------------------------------
FDelegateHandle FKimuraSourceControlProvider::RegisterSourceControlStateChanged_Handle(const FSourceControlStateChanged::FDelegate& SourceControlStateChanged)
{
	return this->OnSourceControlStateChanged.Add(SourceControlStateChanged);
}

//-----------------------------------------------------------------------------
// FKimuraSourceControlProvider::UnregisterSourceControlStateChanged_Handle
//-----------------------------------------------------------------------------
void FKimuraSourceControlProvider::UnregisterSourceControlStateChanged_Handle(FDelegateHandle Handle)
{
	this->OnSourceControlStateChanged.Remove(Handle);
}

//-----------------------------------------------------------------------------
// FKimuraSourceControlProvider::Execute
//-----------------------------------------------------------------------------
ECommandResult::Type FKimuraSourceControlProvider::Execute
	(
		const FSourceControlOperationRef& InOperation,
		FSourceControlChangelistPtr InChangelist,
		const TArray<FString>& InFiles,
		EConcurrency::Type InConcurrency /*= EConcurrency::Synchronous*/,
		const FSourceControlOperationComplete& InOperationCompleteDelegate /*= FSourceControlOperationComplete()*/
	)
{

	FName OpName = InOperation->GetName();
	if (OpName != "GetWorkspaces" && !this->IsAvailable() && OpName != "Connect")
	{
		return ECommandResult::Failed;
	}

	//UE_LOG(LogKimuraSCM, Log, TEXT("Execute operation - %s"), *InOperation->GetName().ToString());

	TArray<FString> AbsoluteFiles = SourceControlHelpers::AbsoluteFilenames(InFiles);

	// map ISourceControlOperation to Kimura operation
	TSharedPtr<KimuraSourceControlOperation, ESPMode::ThreadSafe> op = this->CreateKimuraOperation(InOperation, InChangelist, AbsoluteFiles, InOperationCompleteDelegate);

	if (op == nullptr)
	{
		FFormatNamedArguments Arguments;
		Arguments.Add(TEXT("OperationName"), FText::FromName(InOperation->GetName()));
		Arguments.Add(TEXT("ProviderName"), FText::FromName(GetName()));
		FText Message(FText::Format(LOCTEXT("UnsupportedOperation", "Operation '{OperationName}' not supported by source control provider '{ProviderName}'"), Arguments));
		FMessageLog("SourceControl").Error(Message);
		InOperation->AddErrorMessge(Message);
		InOperationCompleteDelegate.ExecuteIfBound(InOperation, ECommandResult::Failed);
		return ECommandResult::Failed;
	}

	// Execute this operation from our runner. All Kimura operations are executed sequentially.
	bool bEnqueued = false;
	{
		FScopeLock AdmissionLock(&this->OperationAdmissionMutex);
		if (this->KimuraRunnableInstance != nullptr &&
			this->RunnableThread != nullptr)
		{
			bEnqueued = this->KimuraRunnableInstance->EnqueueOperation(op);
			if (bEnqueued)
			{
				// Keep track of all active operations so that Tick() can process
				// completed ops from the main thread.
				this->OngoingSourceControlOperations.Add(op);
			}
		}
	}

	if (!bEnqueued)
	{
		op->MarkCanceled();
		InOperationCompleteDelegate.ExecuteIfBound(InOperation, ECommandResult::Failed);
		return ECommandResult::Failed;
	}

	if (InConcurrency == EConcurrency::Synchronous)
	{
		// Wait for the operation to complete from our runnable thread
		const bool bCompleted = op->BlockingWaitForCompletion();

		// Process completed operations
		this->Tick();

		return bCompleted && op->HasSucceeded() ? ECommandResult::Succeeded : ECommandResult::Failed;
	}
	else
	{
		// For now, success. Tick() will detect when this operation completes and call the operation completion delegate.
		return ECommandResult::Succeeded;
	}

	return ECommandResult::Failed;
}

//-----------------------------------------------------------------------------
// FKimuraSourceControlProvider::CanExecuteOperation
//-----------------------------------------------------------------------------
bool FKimuraSourceControlProvider::CanExecuteOperation(const FSourceControlOperationRef& op) const
{
	bool bSupported = this->SupportedOperations.Contains(op.Get().GetName());
	if (!bSupported)
	{
		UE_LOG(LogKimuraSCM, Warning, TEXT("Unsupported operation: %s!"), *op.Get().GetName().ToString());
	}

	return bSupported;
}

//-----------------------------------------------------------------------------
// FKimuraSourceControlProvider::CanCancelOperation
//-----------------------------------------------------------------------------
bool FKimuraSourceControlProvider::CanCancelOperation(const TSharedRef<ISourceControlOperation, ESPMode::ThreadSafe>& InOperation) const
{
	return false;
}

//-----------------------------------------------------------------------------
// FKimuraSourceControlProvider::CancelOperation
//-----------------------------------------------------------------------------
void FKimuraSourceControlProvider::CancelOperation(const TSharedRef<ISourceControlOperation, ESPMode::ThreadSafe>& InOperation)
{
}

//-----------------------------------------------------------------------------
// FKimuraSourceControlProvider::GetLabels
//-----------------------------------------------------------------------------
TArray<TSharedRef<class ISourceControlLabel>> FKimuraSourceControlProvider::GetLabels(const FString& InMatchingSpec) const
{
	TArray<TSharedRef<ISourceControlLabel>> Labels;
	return Labels;
}

//-----------------------------------------------------------------------------
// FKimuraSourceControlProvider::GetChangelists
//-----------------------------------------------------------------------------
TArray<FSourceControlChangelistRef> FKimuraSourceControlProvider::GetChangelists(EStateCacheUsage::Type InStateCacheUsage)
{
	if (InStateCacheUsage == EStateCacheUsage::ForceUpdate)
	{
		TSharedRef<FUpdatePendingChangelistsStatus, ESPMode::ThreadSafe> updateChangelists = ISourceControlOperation::Create<FUpdatePendingChangelistsStatus>();
		updateChangelists.Get().SetUpdateAllChangelists(true);
		updateChangelists.Get().SetUpdateFilesStates(true);
		updateChangelists.Get().SetUpdateShelvedFilesStates(false);

		this->Execute(updateChangelists, nullptr, TArray<FString>(), EConcurrency::Synchronous);
	}

	TArray<FSourceControlChangelistRef> r;
	for (const auto& CacheItem : this->ChangeListStateCache)
	{
		r.Add(CacheItem.Value.Get().GetChangelist());
	}

	return r;
}

//-----------------------------------------------------------------------------
// FKimuraSourceControlProvider::UsesLocalReadOnlyState
//-----------------------------------------------------------------------------
bool FKimuraSourceControlProvider::UsesLocalReadOnlyState() const
{
	return false;
}

//-----------------------------------------------------------------------------
// FKimuraSourceControlProvider::UsesChangelists
//-----------------------------------------------------------------------------
bool FKimuraSourceControlProvider::UsesChangelists() const
{
	return true;
}

//-----------------------------------------------------------------------------
// FKimuraSourceControlProvider::UsesUncontrolledChangelists
//-----------------------------------------------------------------------------
bool FKimuraSourceControlProvider::UsesUncontrolledChangelists() const
{
	return false;
}

//-----------------------------------------------------------------------------
// FKimuraSourceControlProvider::UsesCheckout
//-----------------------------------------------------------------------------
bool FKimuraSourceControlProvider::UsesCheckout() const
{
	return true;
}

//-----------------------------------------------------------------------------
// FKimuraSourceControlProvider::UsesFileRevisions
//-----------------------------------------------------------------------------
bool FKimuraSourceControlProvider::UsesFileRevisions() const
{
	return true;
}

//-----------------------------------------------------------------------------
// FKimuraSourceControlProvider::UsesSnapshots
//-----------------------------------------------------------------------------
bool FKimuraSourceControlProvider::UsesSnapshots() const
{
	return false;
}

//-----------------------------------------------------------------------------
// FKimuraSourceControlProvider::AllowsDiffAgainstDepot
//-----------------------------------------------------------------------------
bool FKimuraSourceControlProvider::AllowsDiffAgainstDepot() const
{
	return true;
}

#if UE_ENGINE_VERSION_LTE(5, 7)

//-----------------------------------------------------------------------------
// FKimuraSourceControlProvider::IsAtLatestRevision
//-----------------------------------------------------------------------------
TOptional<bool> FKimuraSourceControlProvider::IsAtLatestRevision() const
{
	return TOptional<bool>();
}

//-----------------------------------------------------------------------------
// FKimuraSourceControlProvider::GetNumLocalChanges
//-----------------------------------------------------------------------------
TOptional<int> FKimuraSourceControlProvider::GetNumLocalChanges() const
{
	return TOptional<int>();
}

#endif 

//-----------------------------------------------------------------------------
// FKimuraSourceControlProvider::Tick
//-----------------------------------------------------------------------------
void FKimuraSourceControlProvider::Tick()
{

	TArray<TSharedPtr<KimuraSourceControlOperation, ESPMode::ThreadSafe>> completedOps;

	for (int i = 0; i < this->OngoingSourceControlOperations.Num(); i++)
	{
		TSharedPtr<KimuraSourceControlOperation, ESPMode::ThreadSafe>& op = this->OngoingSourceControlOperations[i];

		if (op->HasCompleted())
		{
			// Refresh server availability
			if (op->GetName() == "Connect")
			{
				this->SetAvailable(op->HasSucceeded());
				this->Dirty = true;
			}

			if (op->GetName() == "UpdateStatus")
			{
				this->SetAvailable(op->HasSucceeded());
				if (!op->HasSucceeded())
				{
					FScopeLock AdmissionLock(&this->OperationAdmissionMutex);
					this->ConnectionState->MarkDisconnected();
				}
			}

			// Save for right after...
			completedOps.Add(op);

			this->OngoingSourceControlOperations.RemoveAt(i--);
		}
	}

	// Executing this outside the previous loop allows 'ApplyChangesToStates' to enqueue new operations.
	for (auto& op : completedOps)
	{
		if (op->HasSucceeded())
		{
			op->ApplyChangesToStates();
		}

		op->CompletionTime = FPlatformTime::Seconds();

		double totalTime = (op->CompletionTime - op->CreationTime) * 1000.0;
		UE_LOG(LogKimuraSCM, Log, TEXT("Operation '%s' has completed in %f ms"), *op->Operation->GetName().ToString(), totalTime);

		op->OnOperationCompleteDelegate.ExecuteIfBound(op->Operation, op->HasSucceeded() ? ECommandResult::Succeeded : ECommandResult::Failed);

	}

	// whenever there are files with a 'pending status', and 
	if (this->Available && this->PendingStatusFiles.Num() > 0)
	{
		// an ongoing update might remove files from the pending status set, so we want to wait until 
		// there are no more UpdateStatus operation present before queuing another one
		bool bStatusUpdateInProgress = false;
		for (const TSharedPtr<KimuraSourceControlOperation, ESPMode::ThreadSafe>& op : this->OngoingSourceControlOperations)
		{
			if (op.IsValid() && op->GetName() == "UpdateStatus")
			{
				bStatusUpdateInProgress = true;
				break;
			}
		}

		if (!bStatusUpdateInProgress)
		{
			// sort out valid files
			TArray<FString> pendingFiles;
			TArray<FString> invalidPendingFiles;
			for (const FString& Filename : this->PendingStatusFiles)
			{
				if (this->IsFileUnderWorkspace(Filename))
				{
					pendingFiles.Add(Filename);
				}
				else
				{
					invalidPendingFiles.Add(Filename);
				}
			}

			// remove invalid files
			for (const FString& Filename : invalidPendingFiles)
			{
				this->PendingStatusFiles.Remove(Filename);
			}

			// kick off an update on the pending files
			if (pendingFiles.Num() > 0)
			{
				this->RequestStatusUpdateOnFiles(pendingFiles);
			}
		}
	}

	if (this->Dirty)
	{
		this->Dirty = false;
		this->OnSourceControlStateChanged.Broadcast();
		this->RefreshWorldOutliner();
	}
}


#if SOURCE_CONTROL_WITH_SLATE
//-----------------------------------------------------------------------------
// FKimuraSourceControlProvider::MakeSettingsWidget
//-----------------------------------------------------------------------------
TSharedRef<class SWidget> FKimuraSourceControlProvider::MakeSettingsWidget() const
{
	return SNew(SKimuraSourceControlSettings);
}

#endif

//-----------------------------------------------------------------------------
// FKimuraSourceControlProvider::CreateKimuraOperation
//-----------------------------------------------------------------------------
TSharedPtr<KimuraSourceControlOperation, ESPMode::ThreadSafe> FKimuraSourceControlProvider::CreateKimuraOperation
	(
	
		TSharedRef<ISourceControlOperation, ESPMode::ThreadSafe> InOperation, 
		FSourceControlChangelistPtr InChangelist, 
		const TArray<FString>& InFiles, 
		const FSourceControlOperationComplete& InDelegate

	)
{
	const FName Connect = "Connect";
	const FName UpdateStatus = "UpdateStatus";		// refresh or get history
	const FName MarkForAdd = "MarkForAdd";
	const FName MarkForEdit = "CheckOut";
	const FName MarkForDelete = "Delete";
	const FName CheckIn = "CheckIn";
	const FName Revert = "Revert";
	const FName Sync = "Sync";
	const FName SyncFileRevision = "SyncFileRevision";
	const FName UpdateChangelistsStatus = "UpdateChangelistsStatus";
	const FName CreateChangelist = "NewChangelist";
	const FName DeleteChangelist = "DeleteChangelist";
	const FName EditChangelist = "EditChangelist";
	const FName MoveToChangelist = "MoveToChangelist";
	const FName GetWorkspaces = "GetWorkspaces";
	const FName Save = "Save";
	const FName Where = "Where";


	const FName RevertUnchanged = "RevertUnchanged";

	FName OpName = InOperation->GetName();

	bool bFilesNeedToBeUnderWorkspace = OpName != SyncFileRevision;

	// Identify valid files. Only update files under the workspace. Ignore temporary, etc. 
	TArray<FString> validFiles;
	for (const FString& f : InFiles)
	{
		if (!bFilesNeedToBeUnderWorkspace || this->IsFileUnderWorkspace(f))
		{
			validFiles.Add(f);
		}
		else
		{
			UE_LOG(LogKimuraSCM, Log, TEXT("Ignore file - %s"), *f);
		}
	}

	TSharedPtr<KimuraSourceControlOperation, ESPMode::ThreadSafe>	op = nullptr;

	if (OpName == GetWorkspaces) { op = MakeShareable(new KimuraOperationGetAvailableWorkspaces(InOperation)); }
	else if (OpName == Connect) { op = MakeShareable(new KimuraOperationConnect(InOperation, this->ConnectionState)); }
	else if (OpName == UpdateStatus)
	{
		TSharedRef<FUpdateStatus, ESPMode::ThreadSafe> UpdateStatusOp = StaticCastSharedRef<FUpdateStatus>(InOperation);

		if (UpdateStatusOp->ShouldUpdateHistory())
		{
			op = MakeShareable(new KimuraOperationGetHistory(InOperation));
		}
		else
		{
			op = MakeShareable(new KimuraOperationUpdateStatus(InOperation));
		}
	}
	else if (OpName == MarkForAdd) { op = MakeShareable(new KimuraOperationMarkForAdd(InOperation)); }
	else if (OpName == MarkForEdit) { op = MakeShareable(new KimuraOperationMarkForEdit(InOperation)); }
	else if (OpName == MarkForDelete) { op = MakeShareable(new KimuraOperationMarkForRemove(InOperation)); }
	else if (OpName == Revert) { op = MakeShareable(new KimuraOperationRevert(InOperation)); }
	else if (OpName == RevertUnchanged) { op = MakeShareable(new KimuraOperationRevert(InOperation, true)); }
	else if (OpName == CheckIn)
	{ 
		if (InChangelist != nullptr)
		{
			op = MakeShareable(new KimuraOperationSubmitWorkspaceChangelist(InOperation, InChangelist));
		}
		else
		{
			op = MakeShareable(new KimuraOperationSubmitWorkspaceFiles(InOperation));
		}
	}				
	else if (OpName == UpdateChangelistsStatus) { op = MakeShareable(new KimuraOperationUpdateChangelistsStatus(InOperation)); }
	else if (OpName == CreateChangelist) { op = MakeShareable(new KimuraOperationCreateChangelist(InOperation)); }
	else if (OpName == DeleteChangelist) { op = MakeShareable(new KimuraOperationDeleteChangelist(InOperation, InChangelist)); }
	else if (OpName == EditChangelist) { op = MakeShareable(new KimuraOperationEditChangelist(InOperation, InChangelist)); }
	else if (OpName == MoveToChangelist) { op = MakeShareable(new KimuraOperationMoveToChangelist(InOperation, InChangelist)); }
	else if (OpName == Sync) { op = MakeShareable(new KimuraOperationSync(InOperation)); }
	else if (OpName == SyncFileRevision) { op = MakeShareable(new KimuraOperationSync(InOperation)); }
	else if (OpName == Save) { op = MakeShareable(new KimuraOperationSave(InOperation)); }
	else if (OpName == Where) { op = MakeShareable(new KimuraOperationWhere(InOperation)); }

	if (op != nullptr)
	{
		op->Files = validFiles;
		op->OnOperationCompleteDelegate = InDelegate;
		op->CreationTime = FPlatformTime::Seconds();
	}

	return op;
}

//-----------------------------------------------------------------------------
// FKimuraSourceControlProvider::AddRepositoryFileOperation
//-----------------------------------------------------------------------------
void FKimuraSourceControlProvider::AddRepositoryFileOperation
	(
	
		FRepositoryLink& InRepositoryLink, 
		const FKimuraFileOperation& InOperation
		
	)
{
	FKimuraFileOperation& insertedOp = InRepositoryLink.FileOperations.Add(InOperation.SID, InOperation);

	FString AbsFilename = this->WorkspaceDescription.WorkspacePath + InRepositoryLink.Description.WorkspacePath + InOperation.F;

	TSharedRef<FKimuraSourceControlState, ESPMode::ThreadSafe> state = this->GetOrCreateState(AbsFilename);

	if (this->IsFileOperationByLocalUser(InRepositoryLink, InOperation))
	{
		if (InOperation.Op == "edit")
		{
			state->MarkedForEdit = true;
		}
		else if (InOperation.Op == "add")
		{
			state->MarkedForAdd = true;
		}
		else if (InOperation.Op == "delete")
		{
			state->MarkedForDelete = true;
		}
		else if (InOperation.Op == "lock")
		{
			state->Locked = true;
		}
	}
	else
	{
		if (InOperation.Op == "edit")
		{
			state->MarkedForEditByOthers++;
		}
		else if (InOperation.Op == "add")
		{
			state->MarkedForEditByOthers++;
		}
		else if (InOperation.Op == "delete")
		{
			state->MarkedForDeleteByOthers++;
		}
		else if (InOperation.Op == "lock")
		{
			state->LockedByOther++;
		}

		if (!state->OthersWithOps.Contains(InOperation.U))
		{
			state->OthersWithOps.Add(InOperation.U);
		}
	}

	this->Dirty = true;
}

//-----------------------------------------------------------------------------
// FKimuraSourceControlProvider::ConvertAbsoluteFilenameToWorkspaceFilename
//-----------------------------------------------------------------------------
bool FKimuraSourceControlProvider::ConvertAbsoluteFilenameToWorkspaceFilename(const FString& InAbsFilename, FString& OutWorkspacefilename) const
{
	FString adjustedFilename = InAbsFilename.Replace(TEXT("\\"), TEXT("/"), ESearchCase::CaseSensitive);

	if (adjustedFilename.ToLower().StartsWith(this->WorkspaceDescription.WorkspacePath))
	{
		OutWorkspacefilename = adjustedFilename;
		OutWorkspacefilename.RemoveAt(0, this->WorkspaceDescription.WorkspacePath.Len());
		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// FKimuraSourceControlProvider::ConvertWorkspaceFilenameToAbsFilename
//-----------------------------------------------------------------------------
void FKimuraSourceControlProvider::ConvertWorkspaceFilenameToAbsoluteFilename(const FString& InWorkspacefilename, FString& OutAbsFilename) const
{
	OutAbsFilename = this->WorkspaceDescription.WorkspacePath + InWorkspacefilename;
}

//-----------------------------------------------------------------------------
// FKimuraSourceControlProvider::GetAvailableWorkspacesForCurrentProject
//-----------------------------------------------------------------------------
void FKimuraSourceControlProvider::GetAvailableWorkspacesForCurrentProject(TArray<FKimuraWorkspaceDesc>& OutWorkspaceDescriptions)
{
	TSharedRef<FGetKimuraWorkspaces, ESPMode::ThreadSafe> getWorkspacesOp = ISourceControlOperation::Create<FGetKimuraWorkspaces>();
	this->Execute(getWorkspacesOp, nullptr, TArray<FString>(), EConcurrency::Synchronous, FSourceControlOperationComplete());

	OutWorkspaceDescriptions = getWorkspacesOp->WorkspaceDescriptions;

}

//-----------------------------------------------------------------------------
// FKimuraSourceControlProvider::GetFilenamesFromChangelist
//-----------------------------------------------------------------------------
void FKimuraSourceControlProvider::GetFilenamesFromChangelist(FString& InCLID, TArray<FString>& OutFiles)
{
	TSharedRef<FKimuraSourceControlChangelistState, ESPMode::ThreadSafe> cl = FKimuraSourceControlModule::Get().KimuraSourceControlProvider.GetOrCreateChangeListState(InCLID);

	for (auto& a : cl.Get().Files)
	{
		OutFiles.Add(a.Get().GetFilename());
	}
}

//-----------------------------------------------------------------------------
// FKimuraSourceControlProvider::GetOrCreateChangeListState
//-----------------------------------------------------------------------------
TSharedRef<FKimuraSourceControlChangelistState, ESPMode::ThreadSafe> FKimuraSourceControlProvider::GetOrCreateChangeListState(const FString& InCLID)
{
	const FString CacheKey = MakeChangeListCacheKey(InCLID);

	TSharedRef<FKimuraSourceControlChangelistState, ESPMode::ThreadSafe>* State = ChangeListStateCache.Find(CacheKey);
	if (State != nullptr)
	{
		// found cached item for this file, return that
		return *State;
	}

	FKimuraSourceControlChangelist cl(InCLID);

	// create new state.
	TSharedRef<FKimuraSourceControlChangelistState, ESPMode::ThreadSafe> NewState = MakeShareable(new FKimuraSourceControlChangelistState(cl));
	this->ChangeListStateCache.Add(CacheKey, NewState);


	return NewState;
}

//-----------------------------------------------------------------------------
// FKimuraSourceControlProvider::GetOrCreateState
//-----------------------------------------------------------------------------
TSharedRef<FKimuraSourceControlState, ESPMode::ThreadSafe> FKimuraSourceControlProvider::GetOrCreateState(const FString& InAbsFilename)
{
	FString lwrFilename = InAbsFilename.ToLower();
	lwrFilename = lwrFilename.Replace(TEXT("\\"), TEXT("/"));

	TSharedRef<FKimuraSourceControlState, ESPMode::ThreadSafe>* State = StateCache.Find(lwrFilename);
	if (State != nullptr)
	{
		// found cached item for this file, return it
		return *State;
	}

	// create new state.
	TSharedRef<FKimuraSourceControlState, ESPMode::ThreadSafe> NewState = MakeShared<FKimuraSourceControlState>(InAbsFilename);
	StateCache.Add(lwrFilename, NewState);
	StateCachePerWUID.Add(NewState->WUID, NewState);

	// new files are automatically added to the 'pending status' files
	if (NewState->UnderWorkspace)
	{
		this->PendingStatusFiles.Add(NewState->GetFilename());
	}

	// if file is source controlled and has revisions.
	if (this->AvailableRevisionsPerWUID.Contains(NewState->WUID))
	{
		// get number of revisions available
		NewState->AvailableRevisions = this->AvailableRevisionsPerWUID[NewState->WUID];

		// assign current revision. Unless specifically told the file is not at latest revision, assume latest revision
		if (this->FilesNotAtLatestRevision.Contains(NewState->WUID))
		{
			NewState->CurrentRevisionNumber = this->FilesNotAtLatestRevision[NewState->WUID];
		}
		else
		{
			NewState->CurrentRevisionNumber = NewState->AvailableRevisions;
		}
	}

#if KIMURA_VERBOSE
	if (NewState->UnderWorkspace)
	{
		UE_LOG(LogKimuraSCM, Log, TEXT("Created state for workspace file - %s"), *NewState->WorkspaceFilename);
	}
	else
	{
		UE_LOG(LogKimuraSCM, Log, TEXT("Created state for file NOT under workspace - %s"), *NewState->LocalFilename);
	}
#endif

	return NewState;
}

//-----------------------------------------------------------------------------
// FKimuraSourceControlProvider::IsFileUnderWorkspace
//-----------------------------------------------------------------------------
bool FKimuraSourceControlProvider::IsFileUnderWorkspace(const FString& InFilename) const
{
	if (!IsAvailable())
	{
		return false;
	}

	return InFilename.StartsWith(this->WorkspaceDescription.WorkspacePath);
}

//-----------------------------------------------------------------------------
// FKimuraSourceControlProvider::RemoveRepositoryFileOperation
//-----------------------------------------------------------------------------
void FKimuraSourceControlProvider::RemoveRepositoryFileOperation(FRepositoryLink& InRepositoryLink, uint64 InOpId)
{
	if (!InRepositoryLink.FileOperations.Contains(InOpId))
	{
		return;
	}

	const FKimuraFileOperation& op = InRepositoryLink.FileOperations[InOpId];

	FString AbsFilename = this->WorkspaceDescription.WorkspacePath + InRepositoryLink.Description.WorkspacePath + op.F;

	TSharedRef<FKimuraSourceControlState, ESPMode::ThreadSafe> state = this->GetOrCreateState(AbsFilename);

	if (this->IsFileOperationByLocalUser(InRepositoryLink, op))
	{
		if (op.Op == "edit")
		{
			state->MarkedForEdit = false;
		}
		else if (op.Op == "add")
		{
			state->MarkedForAdd = false;
		}
		else if (op.Op == "delete")
		{
			state->MarkedForDelete = false;
		}
		else if (op.Op == "lock")
		{
			state->Locked = false;
		}
	}
	else
	{
		if (op.Op == "edit")
		{
			state->MarkedForEditByOthers--;
		}
		else if (op.Op == "add")
		{
			state->MarkedForEditByOthers--;
		}
		else if (op.Op == "delete")
		{
			state->MarkedForDeleteByOthers--;
		}
		else if (op.Op == "lock")
		{
			state->LockedByOther--;
		}

		if (state->OthersWithOps.Contains(op.U))
		{
			state->OthersWithOps.Remove(op.U);
		}
	}

	InRepositoryLink.FileOperations.Remove(InOpId);

	this->Dirty = true;
}

//-----------------------------------------------------------------------------
// FKimuraSourceControlProvider::RequestStatusUpdateOnFiles
//-----------------------------------------------------------------------------
void FKimuraSourceControlProvider::RequestStatusUpdateOnFiles(const TArray<FString>& InFiles)
{
	this->Execute(ISourceControlOperation::Create<FUpdateStatus>(), nullptr, InFiles, EConcurrency::Asynchronous);
}

//-----------------------------------------------------------------------------
// FKimuraSourceControlProvider::RefreshWorldOutliner
//-----------------------------------------------------------------------------
void FKimuraSourceControlProvider::RefreshWorldOutliner()
{
	if (!FModuleManager::Get().IsModuleLoaded(TEXT("LevelEditor")))
	{
		return;
	}

	FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));
	const TSharedPtr<ILevelEditor> LevelEditor = LevelEditorModule.GetLevelEditorInstance().Pin();
	if (!LevelEditor.IsValid())
	{
		return;
	}

	for (const TWeakPtr<ISceneOutliner>& Outliner : LevelEditor->GetAllSceneOutliners())
	{
		if (const TSharedPtr<ISceneOutliner> PinnedOutliner = Outliner.Pin())
		{
			PinnedOutliner->Refresh();
		}
	}
}

//-----------------------------------------------------------------------------
// FKimuraSourceControlProvider::RequestUpdatePendingChanglistsStatus
//-----------------------------------------------------------------------------
void FKimuraSourceControlProvider::RequestUpdatePendingChanglistsStatus()
{
	TSharedRef<FUpdatePendingChangelistsStatus, ESPMode::ThreadSafe> updateCL = ISourceControlOperation::Create<FUpdatePendingChangelistsStatus>();
	updateCL.Get().SetUpdateAllChangelists(true);
	updateCL.Get().SetUpdateFilesStates(true);
	updateCL.Get().SetUpdateShelvedFilesStates(false);

	this->Execute(updateCL, nullptr, TArray<FString>(), EConcurrency::Asynchronous);

}

//-----------------------------------------------------------------------------
// FKimuraSourceControlProvider::ResetConnectionState
//-----------------------------------------------------------------------------
void FKimuraSourceControlProvider::ResetConnectionState()
{
	this->Available = false;
	this->ConnectionState->MarkDisconnected();
}

//-----------------------------------------------------------------------------
// FKimuraSourceControlProvider::SetAvailable
//-----------------------------------------------------------------------------
void FKimuraSourceControlProvider::SetAvailable(bool InAvailable)
{
	if (InAvailable && !this->Available)
	{
		UE_LOG(LogKimuraSCM, Log, TEXT("Server is available"));
	}
	else if (this->Available && !InAvailable)
	{
		UE_LOG(LogKimuraSCM, Error, TEXT("Server is unavailable"));
	}

	this->Available = InAvailable;
}

//-----------------------------------------------------------------------------
// FKimuraSourceControlProvider::ClearRepositoryLinks
//-----------------------------------------------------------------------------
void FKimuraSourceControlProvider::ClearRepositoryLinks()
{
	for (auto& repositoryLink : this->RepositoryLinks)
	{
		TArray<uint64> operationIds;
		repositoryLink.Value.FileOperations.GetKeys(operationIds);

		for (uint64 operationId : operationIds)
		{
			this->RemoveRepositoryFileOperation(repositoryLink.Value, operationId);
		}
	}

	this->RepositoryLinks.Empty();
	this->FileOpTimeStamp = 0;
	this->LatestKnownCLID = 0;
	this->Dirty = true;
}

//-----------------------------------------------------------------------------
// FKimuraSourceControlProvider::UpdateWorkspaceDescriptionAndSetupRepositoryLinks
//-----------------------------------------------------------------------------
void FKimuraSourceControlProvider::UpdateWorkspaceDescriptionAndSetupRepositoryLinks(const FKimuraWorkspaceDesc& InWorkspaceDescription)
{
	// Clear old operation effects while WorkspaceDescription still describes the
	// old workspace, then install the new workspace and repository links.
	this->ClearRepositoryLinks();
	this->WorkspaceDescription = InWorkspaceDescription;

	for (const FRepositoryLinkDesc& desc : this->WorkspaceDescription.RepositoryLinks)
	{
		FRepositoryLink& repoLink = this->RepositoryLinks.Emplace(desc.UID);
		repoLink.Description = desc;
		repoLink.FileOperations.Empty();
	}
}

//-----------------------------------------------------------------------------
// FKimuraSourceControlProvider::UpdateChangeListStates
//-----------------------------------------------------------------------------
void FKimuraSourceControlProvider::UpdateChangeListStates(const TArray<FKimuraChangeListDescription>& InNewChangeListDescriptions)
{
	// This function makes the assumption that file operations are up-to-date and we used to create the file states to be referenced
	// by these change lists

	this->ChangeListStateCache.Empty();
	const FDateTime UpdateTime = FDateTime::UtcNow();

	for (const FKimuraChangeListDescription& desc : InNewChangeListDescriptions)
	{
		const FString changelistId = desc.Id_AsString.IsEmpty() ? desc.Id : desc.Id_AsString;
		FKimuraSourceControlChangelistStateRef newChangeList = this->GetOrCreateChangeListState(changelistId);

		newChangeList.Get().Description = desc.Description;
		newChangeList.Get().TimeStamp = UpdateTime;

		// Change list description contain the WUIDs of files it has operations for.  
		for (const FString&  wuidAsString : desc.FilesByWUID_AsStrings)
		{
			uint64 wuid = FString_To_uint64(wuidAsString);

			// Fetch the file state and add it to the change list state. 
			TSharedRef<FKimuraSourceControlState, ESPMode::ThreadSafe>* f = this->StateCachePerWUID.Find(wuid);
			if (f != nullptr)
			{
				newChangeList.Get().Files.AddUnique(*f);
			}
		}
	}


	this->Dirty = true;

}

//-----------------------------------------------------------------------------
// FKimuraSourceControlProvider::UpdateFileHistories
//-----------------------------------------------------------------------------
void FKimuraSourceControlProvider::UpdateFileHistories(const TArray<FKimuraFileHistory>& InFileHistories)
{
	for (const FKimuraFileHistory& h : InFileHistories)
	{
		FString AbsFilename = this->WorkspaceDescription.WorkspacePath + h.Name;

		TSharedRef<FKimuraSourceControlState, ESPMode::ThreadSafe> state = this->GetOrCreateState(AbsFilename);

		state->UpdateHistory(h);
	}
}

//-----------------------------------------------------------------------------
// FKimuraSourceControlProvider::AddNewAvailableRevisions
//-----------------------------------------------------------------------------
void FKimuraSourceControlProvider::UpdateFileRevisionsAvailable(const TMap<FString, int>& InNewRevisions)
{
	for (auto& Elem : InNewRevisions)
	{
		uint64 wuid = FString_To_uint64(Elem.Key);

		// cache or update existing revision info
		int* pRev = this->AvailableRevisionsPerWUID.Find(wuid);
		if (pRev)
		{
			*pRev = Elem.Value;
		}
		else
		{
			this->AvailableRevisionsPerWUID.Add(wuid, Elem.Value);
		}

		// update available revision number of existing source control states
		TSharedRef<FKimuraSourceControlState, ESPMode::ThreadSafe>* pStateRef = this->StateCachePerWUID.Find(wuid);
		if (pStateRef != nullptr)
		{
			// by default, we assume latest revision. 'UpdateFilesNotAtLatestRevision' is likely to adjust the revision number right after this call.
			TSharedRef<FKimuraSourceControlState, ESPMode::ThreadSafe> state = *pStateRef;
			state->AvailableRevisions = Elem.Value;
			state->CurrentRevisionNumber = Elem.Value;

#if KIMURA_VERBOSE
			UE_LOG(LogKimuraSCM, Log, TEXT("Updating revision from UpdateFileRevisionsAvailable - %s"), *state->WorkspaceFilename);
#endif
		}
	}

	this->Dirty = true;
}

//-----------------------------------------------------------------------------
// FKimuraSourceControlProvider::UpdateFileStates
//-----------------------------------------------------------------------------
void FKimuraSourceControlProvider::UpdateFileStates(const TMap<FString, int>& InNewFileStates)
{
	bool bStateChanged = false;

	for (auto& Elem : InNewFileStates)
	{
		uint64 fuid = FString_To_uint64(Elem.Key);

		TSharedRef<FKimuraSourceControlState, ESPMode::ThreadSafe>* pStateRef = this->StateCachePerWUID.Find(fuid);
		if (pStateRef != nullptr)
		{
			TSharedRef<FKimuraSourceControlState, ESPMode::ThreadSafe> state = *pStateRef;

			// whenever a file is updated, we can remove it from the 'pending status' file (if present)
			this->PendingStatusFiles.Remove(state->GetFilename());

			// check if file is ignored
			const bool bIgnored = (Elem.Value & 1) != 0;
			const bool bLocallyModified = (Elem.Value & 2) != 0;
			bStateChanged |= state->Ignored != bIgnored || state->LocallyModified != bLocallyModified;
			state->Ignored = bIgnored;

			// check if file is modified
			state->LocallyModified = bLocallyModified;

			// Timestamp signals the point at which we can display information about this file.
			//state->TimeStamp = UpdateTime;

			if (!state->StateKnown)
			{
				bStateChanged = true;
				state->StateKnown = true;
			}
		}
	}

	if (bStateChanged)
	{
		this->Dirty = true;
	}
}

//-----------------------------------------------------------------------------
// FKimuraSourceControlProvider::UpdateFilesNotAtLatestRevision
//-----------------------------------------------------------------------------
void FKimuraSourceControlProvider::UpdateFilesNotAtLatestRevision(const TMap<FString, int>& InNewFilesNotAtLatestRevision)
{
	// convert incoming map to a TMap<uint64, int>
	TMap<uint64, int32> filesNotAtLatestRevision;
	for (auto& Elem : InNewFilesNotAtLatestRevision)
	{
		filesNotAtLatestRevision.Add(FString_To_uint64(Elem.Key), Elem.Value);
	}

	// Identify files that were previously outdated but are now at the latest revision
	{
		TArray<uint64> FUIDsNowAtLatestRevision;

		for (auto& Elem : this->FilesNotAtLatestRevision)
		{
			if (!filesNotAtLatestRevision.Contains(Elem.Key))
			{
				FUIDsNowAtLatestRevision.Add(Elem.Key);
			}
		}

		// Update the status of files now at the latest revision and remove them from the list
		for (uint64 id : FUIDsNowAtLatestRevision)
		{
			// Only update the State if we have one for th FUID
			TSharedRef<FKimuraSourceControlState, ESPMode::ThreadSafe>* pStateRef = this->StateCachePerWUID.Find(id);
			if (pStateRef != nullptr)
			{
				TSharedRef<FKimuraSourceControlState, ESPMode::ThreadSafe> state = *pStateRef;
				state->CurrentRevisionNumber = state->AvailableRevisions;
			}

			this->FilesNotAtLatestRevision.Remove(id);
		}
	}

	// Go over the files that are considered outdated
	for (auto& Elem : filesNotAtLatestRevision)
	{
		// Add or update revision
		int* pRev = this->FilesNotAtLatestRevision.Find(Elem.Key);
		if (pRev)
		{
			*pRev = Elem.Value;
		}
		else
		{
			this->FilesNotAtLatestRevision.Add(Elem.Key, Elem.Value);
		}

		// If we have a cached source control state for this file, update its revision number
		TSharedRef<FKimuraSourceControlState, ESPMode::ThreadSafe>* pStateRef = this->StateCachePerWUID.Find(Elem.Key);
		if (pStateRef != nullptr)
		{
			TSharedRef<FKimuraSourceControlState, ESPMode::ThreadSafe> state = *pStateRef;
			state->CurrentRevisionNumber = Elem.Value;
		}
	}

	this->Dirty = true;
}

//-----------------------------------------------------------------------------
// FKimuraSourceControlProvider::UpdateRepositoryFileOperations
//-----------------------------------------------------------------------------
void FKimuraSourceControlProvider::UpdateRepositoryFileOperations
	(
	
		const TArray<FRepositoryFileOperations>& InNewRepoFileOperations
		
	)
{
	// We go through each of the workspace's RepositoryLinks
	for (auto& repositoryLinkbyUID : this->RepositoryLinks)
	{
		// and try to match them against incoming data 
		const FRepositoryFileOperations* pRepoWithFileOps = nullptr;
		for (const FRepositoryFileOperations& inRepoFileOps : InNewRepoFileOperations)
		{
			if (repositoryLinkbyUID.Value.Description.UID == inRepoFileOps.UID)
			{
				pRepoWithFileOps = &inRepoFileOps;
				break;
			}
		}

		// Create a map of all file operations associated with this repository link, and fix ulong conversion to uint64.
		TMap<uint64, FKimuraFileOperation> newFileOpsById;
		if (pRepoWithFileOps)
		{
			for (const FKimuraFileOperation& f : pRepoWithFileOps->Ops)
			{
				uint64 sid = FString_To_uint64(f.SID_AsString);

				FKimuraFileOperation& newOp = newFileOpsById.Add(sid, f);

				// Correct the fields that were not deserialized properly. 
				newOp.SID = sid;
				newOp.RL = FString_To_uint64(newOp.RL_AsString);
				newOp.RUID = FString_To_uint64(newOp.RUID_AsString);
			}
		}

		// Find all the operations that aren't present anymore
		TArray<uint64> invalidFileOpIds;
		{
			// find the ops which aren't present anymore
			for (const auto& fileOpById : repositoryLinkbyUID.Value.FileOperations)
			{
				if (!newFileOpsById.Contains(fileOpById.Key))
				{
					invalidFileOpIds.Add(fileOpById.Key);
				}
			}

			// Remove file operations on the repository link
			for (uint64 id : invalidFileOpIds)
			{
				this->RemoveRepositoryFileOperation(repositoryLinkbyUID.Value, id);
			}
		}

		// Add all new file operations that aren't tracked already
		for (auto& elem : newFileOpsById)
		{
			if (!repositoryLinkbyUID.Value.FileOperations.Contains(elem.Key))
			{
				this->AddRepositoryFileOperation(repositoryLinkbyUID.Value, elem.Value);
			}
		}

		if (invalidFileOpIds.Num() != 0 || newFileOpsById.Num() != 0)
		{
			this->Dirty = true;
		}

	}

}

#undef LOCTEXT_NAMESPACE
