// Copyright Kimura Software Inc.

#include "KimuraSourceControlOperation.h"
#include "KimuraSourceControlModule.h"
#include "ScopedSourceControlProgress.h"
#include "KimuraSourceControlProvider.h"
#include "SourceControlOperations.h"
#include "KimuraSourceControlChangelistState.h"
#include "KimuraSourceControlState.h"


//-----------------------------------------------------------------------------
// KimuraSourceControlOperation::BlockingWaitForCompletion
//-----------------------------------------------------------------------------
bool KimuraSourceControlOperation::BlockingWaitForCompletion(double TimeoutSeconds)
{
	FScopedSourceControlProgress Progress(Operation->GetInProgressString());
	const double Deadline = FPlatformTime::Seconds() + TimeoutSeconds;

	while (!this->Completed && !this->Canceled)
	{
		Progress.Tick();
		if (FPlatformTime::Seconds() >= Deadline)
		{
			UE_LOG(LogKimuraSCM, Error, TEXT("Timed out waiting for source-control operation '%s' to complete."), *this->GetName());
			return false;
		}

		// Sleep for a bit so we don't busy-wait so much.
		FPlatformProcess::Sleep(0.1f);
	}

	return this->Completed;
}


//-----------------------------------------------------------------------------
// KimuraSourceControlOperation::Print
//-----------------------------------------------------------------------------
void KimuraSourceControlOperation::PrintInfo()
{
	UE_LOG(LogKimuraSCM, Log, TEXT("%s"), *this->GetName());
	
	this->PrintAdditionalInfo();
	
	//UE_LOG(KimuraSCM, Log, TEXT("%s"), *this->CommandArguments);
	
	for (const FString& f : this->Files)
	{
		UE_LOG(LogKimuraSCM, Log, TEXT("   '%s'"), *f);
	}

}


//-----------------------------------------------------------------------------
// KimuraSourceControlOperationImpl::ExecuteOnWorkspaceHost
//-----------------------------------------------------------------------------
template <typename P, typename T>
void KimuraSourceControlOperationImpl<P, T>::ExecuteOnWorkspaceHost()
{

#if KIMURA_VERBOSE
	this->PrintInfo();
#endif

	auto OnFail = [this](const FString& InMessage)
	{
		// any failure completes the operation with a Failed code
		this->Result.Code = TEXT("-1");
		this->Result.Msg = InMessage;
		this->Failed = true;
		this->Completed = true;
		UE_LOG(LogKimuraSCM, Error, TEXT("Operation '%s' failed: %s"), *this->GetName(), *InMessage);
	};

	// serialize operation's parameters to json
	FString paramsAsJson;
	if (!FJsonObjectConverter::UStructToJsonObjectString(this->Params, paramsAsJson, 0, 0, 0, nullptr, false))
	{
		OnFail(TEXT("Failed to serialize operation parameters to JSON."));
		return;
	}

	FWorkspaceHostCommand command;
	command.Command = *this->GetName();
	command.Payload = paramsAsJson;

	// Wrap the JSON command name and payload together in a single JSON object.
	FString workspaceHostCommandAsJson;
	if (!FJsonObjectConverter::UStructToJsonObjectString(command, workspaceHostCommandAsJson, 0, 0, 0, nullptr, false))
	{
		OnFail(TEXT("Failed to serialize the workspace command to JSON."));
		return;
	}

	// Execute on the workspace
	FString resultAsString;
	if (!FKimuraSourceControlModule::AccessWorkspaceHost().SendCommand(workspaceHostCommandAsJson, resultAsString))
	{
		OnFail(TEXT("The workspace host failed to execute the command."));
		return;
	}

	if (!this->DeserializeResultFromString(resultAsString))
	{
		OnFail(TEXT("The workspace host returned invalid or incomplete JSON."));
		return;
	}

	this->Failed = !this->Result.HasSucceeded();
	this->Completed = true;

#if KIMURA_VERBOSE
	this->Result.Print();
#endif

}


//-----------------------------------------------------------------------------
// KimuraOperationGetAvailableWorkspaces::Execute
//-----------------------------------------------------------------------------
void KimuraOperationGetAvailableWorkspaces::Execute()
{

	if (!FKimuraSourceControlModule::AccessWorkspaceHost().IsValid())
	{
		this->Failed = true;
		this->Completed = true;

		UE_LOG(LogKimuraSCM, Error, TEXT("Workspace Host not initialized."));
		return;
	}

	this->Params.ProjectPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());

	this->ExecuteOnWorkspaceHost();

	// on success, copy workspace descriptions
	if (this->Result.HasSucceeded())
	{
		TSharedRef<FGetKimuraWorkspaces, ESPMode::ThreadSafe> getWorkspaces = StaticCastSharedRef<FGetKimuraWorkspaces>(this->Operation);
		getWorkspaces->WorkspaceDescriptions = this->Result.WorkspaceDescriptions;
	}

	this->Failed = !this->Result.HasSucceeded();
	this->Completed = true;


}


//-----------------------------------------------------------------------------
// KimuraOperationConnect::Execute
//-----------------------------------------------------------------------------
void KimuraOperationConnect::Execute()
{
	if (!FKimuraSourceControlModule::AccessWorkspaceHost().IsValid())
	{
		this->SharedConnectionState->MarkDisconnected();
		this->Failed = true;
		this->Completed = true;

		UE_LOG(LogKimuraSCM, Error, TEXT("Workspace Host not initialized."));
		return;
	}

	if (this->SharedConnectionState->IsConnected())
	{
		UE_LOG(LogKimuraSCM, Log, TEXT("Connect request ignored because the provider is already connected."));
		this->Failed = false;
		this->Completed = true;
		return;
	}

	this->SharedConnectionState->MarkConnecting();

	FKimuraSourceControlSettings& settings = FKimuraSourceControlModule::AccessSettings();

	// Set up params
	{

		this->Params.ServerAddress = settings.ServerAddress;
		this->Params.Username = settings.UserName;
		this->Params.Password = settings.Password;
		this->Params.Workspace = settings.Workspace;
		this->Params.ServerMustProvideTrustedCertificate = settings.ServerMustProvideTrustedCertificate;

		switch (settings.ClientCertSource)
		{
			case 0:
			{
				break;
			}
			case 1:
			{
				// thumbprint, from store
				this->Params.ClientCertificateThumbprint = settings.ClientCertificateThumbprint;
				break;

			}
			case 2:
			{
				// from PFX file
				FString realPath = FPaths::ConvertRelativePathToFull(settings.ClientCertificatePFX);
				this->Params.ClientCertificatePFX = realPath;
				this->Params.ClientCertificatePFXPwd = settings.ClientCertificatePFXPassword;
				break;
			}
		}

	}


	this->ExecuteOnWorkspaceHost();

	// on success, update workspace description
	if (this->Result.HasSucceeded())
	{
		this->SharedConnectionState->MarkConnected();
		FKimuraSourceControlProvider& provider = FKimuraSourceControlModule::Get().KimuraSourceControlProvider;
		provider.UpdateWorkspaceDescriptionAndSetupRepositoryLinks(this->Result.Description);
	}
	else
	{
		this->SharedConnectionState->MarkDisconnected();
		this->Result.Print();
	}

	this->Failed = !this->Result.HasSucceeded();
	this->Completed = true;

}


//-----------------------------------------------------------------------------
// KimuraOperationConnect::PrintAdditionalInfo
//-----------------------------------------------------------------------------
void KimuraOperationConnect::PrintAdditionalInfo()
{
}


//-----------------------------------------------------------------------------
// KimuraOperationConnect::ApplyChangesToStates
//-----------------------------------------------------------------------------
void KimuraOperationConnect::ApplyChangesToStates()
{
}


//-----------------------------------------------------------------------------
// KimuraOperationUpdateStatus::Execute
//-----------------------------------------------------------------------------
void KimuraOperationUpdateStatus::Execute()
{
	FKimuraSourceControlProvider& provider = FKimuraSourceControlModule::Get().KimuraSourceControlProvider;

	this->Params.Request = "UpdateStatus";
	this->Params.FileOpTS = FString::Printf(TEXT("%llu"), provider.FileOpTimeStamp);
	this->Params.CLID = FString::Printf(TEXT("%llu"), provider.LatestKnownCLID);
	this->Params.Files = this->Files;

	this->ExecuteOnWorkspaceHost();

	if (this->Result.HasSucceeded())
	{
		// convert result timestamp and clid from string to uint64
		this->Result.FileOpTimeStamp = FString_To_uint64(this->Result.FileOpTimeStampAsString);
		this->Result.LatestCLID = FString_To_uint64(this->Result.LatestCLIDAsString);
	}

}


//-----------------------------------------------------------------------------
// KimuraOperationUpdateStatus::PrintExtras
//-----------------------------------------------------------------------------
void KimuraOperationUpdateStatus::PrintAdditionalInfo()
{
	TSharedRef<FUpdateStatus, ESPMode::ThreadSafe> UpdateStatusOp = StaticCastSharedRef<FUpdateStatus>(this->Operation);

	bool bShouldCheckAllFiles = UpdateStatusOp->ShouldCheckAllFiles();
	bool bShouldGetOpenedOnly = UpdateStatusOp->ShouldGetOpenedOnly();
	bool bShouldUpdateHistory = UpdateStatusOp->ShouldUpdateHistory();
	bool bShouldUpdateModified = UpdateStatusOp->ShouldUpdateModifiedState();

	UE_LOG(LogKimuraSCM, Log, TEXT("bShouldCheckAllFiles=%d, bShouldGetOpenedOnly=%d, bShouldUpdateHistory=%d, bShouldUpdateModified=%d"), bShouldCheckAllFiles, bShouldGetOpenedOnly, bShouldUpdateHistory, bShouldUpdateModified);
}


//-----------------------------------------------------------------------------
// KimuraOperationUpdateStatus::ApplyChangesToStates
//-----------------------------------------------------------------------------
void KimuraOperationUpdateStatus::ApplyChangesToStates()
{
	FKimuraSourceControlProvider& provider = FKimuraSourceControlModule::Get().KimuraSourceControlProvider;

	// timestamp and CLID will prevent subsequent calls from obtaining redundant data
	if (provider.FileOpTimeStamp != this->Result.FileOpTimeStamp)
	{
		provider.UpdateRepositoryFileOperations(this->Result.RepositoryFileOps);
		provider.FileOpTimeStamp = this->Result.FileOpTimeStamp;
	}

	// File revisions
	provider.UpdateFileRevisionsAvailable(this->Result.AvailableRevisionsPerWUID);

	// Files not at latest revision
	provider.UpdateFilesNotAtLatestRevision(this->Result.FilesNotAtLatestRevision);

	// update states (ignored, modified) of files
	provider.UpdateFileStates(this->Result.FileStatesPerWUID);

	provider.LatestKnownCLID = this->Result.LatestCLID;

}


//-----------------------------------------------------------------------------
// KimuraOperationMarkForAdd::Execute
//-----------------------------------------------------------------------------
void KimuraOperationMarkForAdd::Execute()
{
	this->Params.Request = "MarkForAdd";
	this->Params.Files = this->Files;

	this->ExecuteOnWorkspaceHost();

	return;
}


//-----------------------------------------------------------------------------
// KimuraOperationMarkForAdd::ApplyChangesToStates
//-----------------------------------------------------------------------------
void KimuraOperationMarkForAdd::ApplyChangesToStates()
{
	FKimuraSourceControlProvider& provider = FKimuraSourceControlModule::Get().KimuraSourceControlProvider;

	provider.UpdateRepositoryFileOperations(this->Result.RepositoryFileOps);
	provider.UpdateFileStates(this->Result.FileStatesPerWUID);

	FKimuraSourceControlModule::Get().KimuraSourceControlProvider.RequestUpdatePendingChanglistsStatus();

}


//-----------------------------------------------------------------------------
// KimuraOperationMarkForEdit::Execute
//-----------------------------------------------------------------------------
void KimuraOperationMarkForEdit::Execute()
{

	this->Params.Request = "MarkForEdit";
	this->Params.Files = this->Files;

	this->ExecuteOnWorkspaceHost();

	return;
}


//-----------------------------------------------------------------------------
// KimuraOperationMarkForEdit::ApplyChangesToStates
//-----------------------------------------------------------------------------
void KimuraOperationMarkForEdit::ApplyChangesToStates()
{
	FKimuraSourceControlProvider& provider = FKimuraSourceControlModule::Get().KimuraSourceControlProvider;

	provider.UpdateRepositoryFileOperations(this->Result.RepositoryFileOps);
	provider.UpdateFileStates(this->Result.FileStatesPerWUID);

	FKimuraSourceControlModule::Get().KimuraSourceControlProvider.RequestUpdatePendingChanglistsStatus();
}


//-----------------------------------------------------------------------------
// KimuraOperationSave::Execute
//-----------------------------------------------------------------------------
void KimuraOperationSave::Execute()
{
	FKimuraSourceControlProvider& provider = FKimuraSourceControlModule::Get().KimuraSourceControlProvider;

	this->Params.Request = "Save";

	// Go through all files and identify those not yet marked for add or edit
	for (const FString& f : this->Files)
	{
		TSharedRef<FKimuraSourceControlState, ESPMode::ThreadSafe> s = provider.GetOrCreateState(f);

		if (s->CanAdd())
		{
			this->Params.Files.Add(f);
		}
	}

	if (this->Params.Files.Num() > 0)
	{
		this->ExecuteOnWorkspaceHost();
	}
	else
	{
		this->Result.Code = "0";
		this->Completed = true;
		UE_LOG(LogKimuraSCM, Log, TEXT("No additional operation necessary for 'save'"));

	}
}


//-----------------------------------------------------------------------------
// KimuraOperationSave::ApplyChangesToStates
//-----------------------------------------------------------------------------
void KimuraOperationSave::ApplyChangesToStates()
{
	FKimuraSourceControlProvider& provider = FKimuraSourceControlModule::Get().KimuraSourceControlProvider;

	provider.UpdateRepositoryFileOperations(this->Result.RepositoryFileOps);
	provider.UpdateFileStates(this->Result.FileStatesPerWUID);

	FKimuraSourceControlModule::Get().KimuraSourceControlProvider.RequestUpdatePendingChanglistsStatus();
}


//-----------------------------------------------------------------------------
// KimuraOperationMarkForRemove::Execute
//-----------------------------------------------------------------------------
void KimuraOperationMarkForRemove::Execute()
{
	this->Params.Request = "MarkForDelete";
	this->Params.Files = this->Files;

	this->ExecuteOnWorkspaceHost();

	return;

}


//-----------------------------------------------------------------------------
// KimuraOperationMarkForRemove::ApplyChangesToStates
//-----------------------------------------------------------------------------
void KimuraOperationMarkForRemove::ApplyChangesToStates()
{
	FKimuraSourceControlProvider& provider = FKimuraSourceControlModule::Get().KimuraSourceControlProvider;

	provider.UpdateRepositoryFileOperations(this->Result.RepositoryFileOps);
	provider.UpdateFileStates(this->Result.FileStatesPerWUID);

	FKimuraSourceControlModule::Get().KimuraSourceControlProvider.RequestUpdatePendingChanglistsStatus();

}


//-----------------------------------------------------------------------------
// KimuraOperationRevert::Execute
//-----------------------------------------------------------------------------
void KimuraOperationRevert::Execute()
{
	this->Params.Request = "Revert";
	this->Params.Files = this->Files;
	this->Params.UnchangedOnly = this->bUnchangedOnly;

	this->ExecuteOnWorkspaceHost();


	return;

}


//-----------------------------------------------------------------------------
// KimuraOperationRevert::ApplyChangesToStates
//-----------------------------------------------------------------------------
void KimuraOperationRevert::ApplyChangesToStates()
{
	FKimuraSourceControlProvider& provider = FKimuraSourceControlModule::Get().KimuraSourceControlProvider;

	provider.UpdateRepositoryFileOperations(this->Result.RepositoryFileOps);
	provider.UpdateFileStates(this->Result.FileStatesPerWUID);

	FKimuraSourceControlModule::Get().KimuraSourceControlProvider.RequestUpdatePendingChanglistsStatus();
}


//-----------------------------------------------------------------------------
// KimuraOperationSubmitWorkspaceFiles::Execute
//-----------------------------------------------------------------------------
void KimuraOperationSubmitWorkspaceFiles::Execute()
{
	TSharedRef<FCheckIn, ESPMode::ThreadSafe> CheckInOperation = StaticCastSharedRef<FCheckIn>(this->Operation);

	this->Params.Request = "SubmitWorkspaceFiles";
	this->Params.Files = this->Files;
	this->Params.Description = CheckInOperation.Get().GetDescription().ToString();

	this->ExecuteOnWorkspaceHost();

	return;

}


//-----------------------------------------------------------------------------
// KimuraOperationSubmitWorkspaceFiles::ApplyChangesToStates
//-----------------------------------------------------------------------------
void KimuraOperationSubmitWorkspaceFiles::ApplyChangesToStates()
{
	FKimuraSourceControlModule::Get().KimuraSourceControlProvider.RequestUpdatePendingChanglistsStatus();
	FKimuraSourceControlModule::Get().KimuraSourceControlProvider.RequestStatusUpdateOnFiles(this->Files);
}


//-----------------------------------------------------------------------------
// KimuraOperationSubmitWorkspaceChangelist::Execute
//-----------------------------------------------------------------------------
void KimuraOperationSubmitWorkspaceChangelist::Execute()
{
	TSharedRef<FCheckIn, ESPMode::ThreadSafe> CheckInOperation = StaticCastSharedRef<FCheckIn>(this->Operation);

	this->Params.Request = "SubmitWorkspaceChangelist";
	this->Params.CLID = this->CLID;
	this->Params.Description = CheckInOperation.Get().GetDescription().ToString();

	// Save files associated with this change list for later
	this->Files.Empty();
	FKimuraSourceControlModule::Get().KimuraSourceControlProvider.GetFilenamesFromChangelist(this->CLID, this->Files);

	this->ExecuteOnWorkspaceHost();

	return;

}


//-----------------------------------------------------------------------------
// KimuraOperationSubmitWorkspaceChangelist::ApplyChangesToStates
//-----------------------------------------------------------------------------
void KimuraOperationSubmitWorkspaceChangelist::ApplyChangesToStates()
{
	FKimuraSourceControlModule::Get().KimuraSourceControlProvider.RequestUpdatePendingChanglistsStatus();
	FKimuraSourceControlModule::Get().KimuraSourceControlProvider.RequestStatusUpdateOnFiles(this->Files);
}


//-----------------------------------------------------------------------------
// KimuraOperationSync::Execute
//-----------------------------------------------------------------------------
void KimuraOperationSync::Execute()
{
	this->Params.Files = this->Files;

	if (this->Operation->GetName() == "SyncFileRevision")
	{
		const TSharedRef<FSyncFileRevision, ESPMode::ThreadSafe> getRevisionOp = StaticCastSharedRef<FSyncFileRevision>(this->Operation);
		this->Params.CLID = getRevisionOp->CLID;
		this->Params.Force = true;
		this->Params.DestinationPath = getRevisionOp->DestinationPath;
	}
	else
	{
		const TSharedRef<FSync, ESPMode::ThreadSafe> syncOp = StaticCastSharedRef<FSync>(this->Operation);
		this->Params.Force = syncOp->IsForced();
		this->Params.SyncCurrent = syncOp->IsLastSyncedFlagSet();
		this->Params.SyncLatest = syncOp->IsHeadRevisionFlagSet();
		this->Params.CLID = syncOp->GetRevision();

		// Setting CLID to 0 gets the latest revision.
		if (this->Params.CLID.IsEmpty())
		{
			this->Params.CLID = "0";
		}
	}

	this->ExecuteOnWorkspaceHost();

}


//-----------------------------------------------------------------------------
// KimuraOperationSync::ApplyChangesToStates
//-----------------------------------------------------------------------------
void KimuraOperationSync::ApplyChangesToStates()
{
	if (!this->Params.DestinationPath.IsEmpty())
	{
		return;
	}

	FKimuraSourceControlModule::Get().KimuraSourceControlProvider.RequestUpdatePendingChanglistsStatus();
	FKimuraSourceControlModule::Get().KimuraSourceControlProvider.RequestStatusUpdateOnFiles(this->Files);
}


//-----------------------------------------------------------------------------
// KimuraOperationGetHistory::Execute
//-----------------------------------------------------------------------------
void KimuraOperationGetHistory::Execute()
{
	this->Params.Request = "GetHistory";
	this->Params.Files = this->Files;

	this->ExecuteOnWorkspaceHost();

}


//-----------------------------------------------------------------------------
// KimuraOperationGetHistory::ApplyChangesToStates
//-----------------------------------------------------------------------------
void KimuraOperationGetHistory::ApplyChangesToStates()
{
	FKimuraSourceControlProvider& provider = FKimuraSourceControlModule::Get().KimuraSourceControlProvider;

	provider.UpdateFileHistories(this->Result.FileHistories);

}


EKimuraResultCodes FKimuraResult::ToResultCode()
{
	uint64 resultCode = FString_To_uint64(this->Code);
	return (EKimuraResultCodes)resultCode;
}

//-----------------------------------------------------------------------------
// FKimuraResult::Print
//-----------------------------------------------------------------------------
void FKimuraResult::Print()
{
	UE_LOG(LogKimuraSCM, Log, TEXT("Result: Code '%s', Msg '%s'"), *this->Code, *this->Msg);
}


//-----------------------------------------------------------------------------
// FKimuraRefreshResult::Print
//-----------------------------------------------------------------------------
void FKimuraUpdateStatusResult::Print()
{
	UE_LOG(LogKimuraSCM, Log, TEXT("Result: '%s', LatestCLID: %s, FileOpTimeStamp: %s, Num files revisions retrieved: %d, Num files not at latest revision: %d"), *this->Code, *this->LatestCLIDAsString, *this->FileOpTimeStampAsString, this->AvailableRevisionsPerWUID.Num(), this->FilesNotAtLatestRevision.Num());

}


//-----------------------------------------------------------------------------
// FKimuraFileOpResult::Print
//-----------------------------------------------------------------------------
void FKimuraFileOpResult::Print()
{
	UE_LOG(LogKimuraSCM, Log, TEXT("KimuraFileOpResult: '%s', Num ops to add: %d, Num ops to remove: %d"), *this->Code, this->FileOpsToAdd.Num(), this->FileOpIDsToRemove.Num());

	if (!this->HasSucceeded())
	{
		if (this->ToResultCode() == EKimuraResultCodes::NoFilesToSubmit)
		{
			UE_LOG(LogKimuraSCM, Warning, TEXT("KimuraFileOpResult: No files require submission, as they are unmodified."));
		}
	}
}


//-----------------------------------------------------------------------------
// FKimuraGetHistoryResult::Print
//-----------------------------------------------------------------------------
void FKimuraGetHistoryResult::Print()
{
	UE_LOG(LogKimuraSCM, Log, TEXT("Result: '%s', Retrieved history on %d files"), *this->Code, this->FileHistories.Num());
}


//-----------------------------------------------------------------------------
// KimuraOperationUpdateChangelistsStatus::Execute
//-----------------------------------------------------------------------------
void KimuraOperationUpdateChangelistsStatus::Execute()
{
	this->Params.Request = "UpdateChangelistsStatus";

	TSharedRef<FUpdatePendingChangelistsStatus, ESPMode::ThreadSafe> updatePendingChangelistsStatus = StaticCastSharedRef<FUpdatePendingChangelistsStatus>(this->Operation);

	this->Params.UpdateFilesStates = updatePendingChangelistsStatus.Get().ShouldUpdateFilesStates();
	this->Params.UpdateAllChangelists = updatePendingChangelistsStatus.Get().ShouldUpdateAllChangelists();
	this->Params.UpdateShelvedFilesStates = updatePendingChangelistsStatus.Get().ShouldUpdateShelvedFilesStates();
	for (const auto& cl : updatePendingChangelistsStatus.Get().GetChangelistsToUpdate())
	{
		this->Params.ChangeListsToUpdate.Add(cl.Get().GetIdentifier());
	}

	this->ExecuteOnWorkspaceHost();
}


//-----------------------------------------------------------------------------
// KimuraOperationUpdateChangelistsStatus::ApplyChangesToStates
//-----------------------------------------------------------------------------
void KimuraOperationUpdateChangelistsStatus::ApplyChangesToStates()
{
	
	FKimuraSourceControlProvider& provider = FKimuraSourceControlModule::Get().KimuraSourceControlProvider;

	provider.UpdateRepositoryFileOperations(this->Result.RepositoryFileOps);
	provider.UpdateChangeListStates(this->Result.ChangeLists);

}



//-----------------------------------------------------------------------------
// KimuraOperationCreateChangelist::Execute
//-----------------------------------------------------------------------------
void KimuraOperationCreateChangelist::Execute()
{
	this->Params.Request = "CreateChangelist";

	TSharedRef<FNewChangelist, ESPMode::ThreadSafe> newChangelistsStatus = StaticCastSharedRef<FNewChangelist>(this->Operation);

	this->Params.Description = newChangelistsStatus.Get().GetDescription().ToString();


	this->ExecuteOnWorkspaceHost();
}


//-----------------------------------------------------------------------------
// KimuraOperationCreateChangelist::ApplyChangesToStates
//-----------------------------------------------------------------------------
void KimuraOperationCreateChangelist::ApplyChangesToStates()
{

	FKimuraSourceControlProvider& provider = FKimuraSourceControlModule::Get().KimuraSourceControlProvider;

	provider.UpdateRepositoryFileOperations(this->Result.RepositoryFileOps);
	provider.UpdateChangeListStates(this->Result.ChangeLists);


}

//-----------------------------------------------------------------------------
// KimuraOperationDeleteChangelist::Execute
//-----------------------------------------------------------------------------
void KimuraOperationDeleteChangelist::Execute()
{
	this->Params.Request = "DeleteChangelist";
	this->Params.CLID = this->CLID;

	TSharedRef<FDeleteChangelist, ESPMode::ThreadSafe> deleteChangelist = StaticCastSharedRef<FDeleteChangelist>(this->Operation);

	this->ExecuteOnWorkspaceHost();
}


//-----------------------------------------------------------------------------
// KimuraOperationDeleteChangelist::ApplyChangesToStates
//-----------------------------------------------------------------------------
void KimuraOperationDeleteChangelist::ApplyChangesToStates()
{
	FKimuraSourceControlProvider& provider = FKimuraSourceControlModule::Get().KimuraSourceControlProvider;

	provider.UpdateRepositoryFileOperations(this->Result.RepositoryFileOps);
	provider.UpdateChangeListStates(this->Result.ChangeLists);

}


//-----------------------------------------------------------------------------
// KimuraOperationEditChangelist::Execute
//-----------------------------------------------------------------------------
void KimuraOperationEditChangelist::Execute()
{
	this->Params.Request = "EditChangelist";

	TSharedRef<FEditChangelist, ESPMode::ThreadSafe> editChangelist = StaticCastSharedRef<FEditChangelist>(this->Operation);

	this->Params.CLID = this->CLID;
	this->Params.Description = editChangelist.Get().GetDescription().ToString();

	this->ExecuteOnWorkspaceHost();
}


//-----------------------------------------------------------------------------
// KimuraOperationMoveToChangelist::Execute
//-----------------------------------------------------------------------------
void KimuraOperationMoveToChangelist::Execute()
{
	this->Params.Request = "MoveToChangelist";

	FKimuraSourceControlProvider& provider = FKimuraSourceControlModule::Get().KimuraSourceControlProvider;
	TSharedRef<FKimuraSourceControlChangelistState, ESPMode::ThreadSafe> targetChangelist = provider.GetOrCreateChangeListState(this->CLID);

	this->Params.CLID = this->CLID;
	this->Params.Files = this->Files;
	this->Params.Description = targetChangelist->Description;

	this->ExecuteOnWorkspaceHost();
}


//-----------------------------------------------------------------------------
// KimuraOperationMoveToChangelist::ApplyChangesToStates
//-----------------------------------------------------------------------------
void KimuraOperationMoveToChangelist::ApplyChangesToStates()
{
	FKimuraSourceControlProvider& provider = FKimuraSourceControlModule::Get().KimuraSourceControlProvider;

	provider.UpdateRepositoryFileOperations(this->Result.RepositoryFileOps);
	provider.UpdateChangeListStates(this->Result.ChangeLists);
}


//-----------------------------------------------------------------------------
// KimuraOperationEditChangelist::ApplyChangesToStates
//-----------------------------------------------------------------------------
void KimuraOperationEditChangelist::ApplyChangesToStates()
{
	FKimuraSourceControlProvider& provider = FKimuraSourceControlModule::Get().KimuraSourceControlProvider;

	provider.UpdateRepositoryFileOperations(this->Result.RepositoryFileOps);
	provider.UpdateChangeListStates(this->Result.ChangeLists);

}



//-----------------------------------------------------------------------------
// KimuraOperationWhere::Execute
//-----------------------------------------------------------------------------
void KimuraOperationWhere::Execute()
{

	FKimuraSourceControlProvider& provider = FKimuraSourceControlModule::Get().KimuraSourceControlProvider;

	TArray<FWhere::FileInfo> outFileInfos;
	for (const FString& f : this->Files)
	{
		FWhere::FileInfo fileInfo;
		provider.ConvertAbsoluteFilenameToWorkspaceFilename(f, fileInfo.RemotePath);
		fileInfo.LocalPath = f;

		outFileInfos.Add(MoveTemp(fileInfo));
	}
	
	TSharedRef<FWhere, ESPMode::ThreadSafe> whereOp = StaticCastSharedRef<FWhere>(this->Operation);
	whereOp.Get().SetFiles(MoveTemp(outFileInfos));

	this->Result.Code = "0";
	this->Completed = true;
}


//-----------------------------------------------------------------------------
// KimuraOperationSilent::Execute
//-----------------------------------------------------------------------------
void KimuraOperationSilent::Execute()
{
	this->Result.Code = "0";
	this->Completed = true;
}

//-----------------------------------------------------------------------------
// KimuraOperationSilent::ApplyChangesToStates
//-----------------------------------------------------------------------------
void KimuraOperationSilent::ApplyChangesToStates()
{

}
