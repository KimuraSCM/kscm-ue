// Copyright Kimura Software Inc.

#include "KimuraSourceControlRevision.h"
#include "KimuraSourceControlModule.h"
#include "KimuraSourceControlOperation.h"
#include "HAL/FileManager.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"

#define LOCTEXT_NAMESPACE "KimuraSourceControl.Revision"

//-----------------------------------------------------------------------------
// FKimuraSourceControlRevision::Get
//-----------------------------------------------------------------------------
bool FKimuraSourceControlRevision::Get(FString& InOutFilename, EConcurrency::Type InConcurrency) const
{
	if (FileName.IsEmpty() || CLID <= 0)
	{
		return false;
	}

	// Execute through the source control provider
	FKimuraSourceControlProvider& Provider = FKimuraSourceControlModule::Get().KimuraSourceControlProvider;

	// Resolve full path to the desired file
	FString FullFilename = Provider.GetWorkspaceDescription().WorkspacePath + this->FileName;

	// And path in the owning workspace
	FString WorkspaceFilename;
	Provider.ConvertAbsoluteFilenameToWorkspaceFilename(FullFilename, WorkspaceFilename);

	// 
	if (InOutFilename.IsEmpty())
	{
		// combine the WUID, CLID and filename into a unique filename at the root of Diff folder
		uint64 UID = GetUID(WorkspaceFilename);

		const FString UniqueFileName = FString::Printf(
			TEXT("%llu-%d-%s"),
			UID,
			CLID,
			*FPaths::GetCleanFilename(FileName));
		InOutFilename = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::DiffDir(), UniqueFileName));
	}

	// Have we already downloaded this revision?
	const FString OutputFilename = FPaths::ConvertRelativePathToFull(InOutFilename);
	if (FPaths::FileExists(OutputFilename))
	{
		// InOutFilename already points to a valid file
		UE_LOG(LogKimuraSCM, Log, TEXT("Revision already present: %s"), *InOutFilename);
		return true;
	}

	// kscm's Sync request can download to an isolated directory. 
	const FString SyncRoot = FPaths::Combine(
		FPaths::DiffDir(),
		FString::Printf(TEXT("KimuraSync-%s"), *FGuid::NewGuid().ToString()));
	IFileManager::Get().MakeDirectory(*SyncRoot, true);

	// This is a custom sync operation that targets a specific file revision and stores it at a destination
	TSharedRef<FSyncFileRevision, ESPMode::ThreadSafe> SyncFileRevision = ISourceControlOperation::Create<FSyncFileRevision>();
	SyncFileRevision->CLID = FString::FromInt(CLID);
	SyncFileRevision->DestinationPath = FPaths::ConvertRelativePathToFull(SyncRoot);

	const bool bRequestSucceeded = Provider.Execute(
		SyncFileRevision,
		nullptr,
		{ FullFilename },
		EConcurrency::Synchronous,
		FSourceControlOperationComplete()) == ECommandResult::Succeeded;


	bool bSucceeded = false;
	if (bRequestSucceeded)
	{
		WorkspaceFilename.RemoveFromStart(TEXT("/"));
		WorkspaceFilename.RemoveFromStart(TEXT("\\"));
		const FString DownloadedFilename = FPaths::Combine(SyncRoot, WorkspaceFilename);

		if (FPaths::FileExists(DownloadedFilename))
		{
			UE_LOG(LogKimuraSCM, Log, TEXT("Sync operation successful, found the downloaded file: %s"), *DownloadedFilename);
			IFileManager::Get().MakeDirectory(*FPaths::GetPath(OutputFilename), true);
			bSucceeded = IFileManager::Get().Move(*OutputFilename, *DownloadedFilename, true, true, true);

			if (!bSucceeded)
			{
				UE_LOG(LogKimuraSCM, Error, TEXT("Sync operation successful, failed to move the downloaded file to %s"), *OutputFilename);
			}
		}
		else
		{
			UE_LOG(LogKimuraSCM, Error, TEXT("Sync operation successful, but missing file: %s"), *DownloadedFilename);
		}
	}

	// we're done with the Sync folder, delete it
	IFileManager::Get().DeleteDirectory(*SyncRoot, false, true);

	return bSucceeded;
}

//-----------------------------------------------------------------------------
// FKimuraSourceControlRevision::GetAnnotated
//-----------------------------------------------------------------------------
bool FKimuraSourceControlRevision::GetAnnotated( TArray<FAnnotationLine>& OutLines ) const
{
	// not supported yet
	return false;
}

//-----------------------------------------------------------------------------
// FKimuraSourceControlRevision::GetAnnotated
//-----------------------------------------------------------------------------
bool FKimuraSourceControlRevision::GetAnnotated( FString& InOutFilename ) const
{
	// not supported yet
	return false;
}

//-----------------------------------------------------------------------------
// FKimuraSourceControlRevision::GetFilename
//-----------------------------------------------------------------------------
const FString& FKimuraSourceControlRevision::GetFilename() const
{
	return FileName;
}

//-----------------------------------------------------------------------------
// FKimuraSourceControlRevision::GetRevisionNumber
//-----------------------------------------------------------------------------
int32 FKimuraSourceControlRevision::GetRevisionNumber() const
{
	return RevisionNumber;
}

//-----------------------------------------------------------------------------
// FKimuraSourceControlRevision::GetRevision
//-----------------------------------------------------------------------------
const FString& FKimuraSourceControlRevision::GetRevision() const
{
	return Revision;
}

//-----------------------------------------------------------------------------
// FKimuraSourceControlRevision::GetDescription
//-----------------------------------------------------------------------------
const FString& FKimuraSourceControlRevision::GetDescription() const
{
	return Description;
}

//-----------------------------------------------------------------------------
// FKimuraSourceControlRevision::GetUserName
//-----------------------------------------------------------------------------
const FString& FKimuraSourceControlRevision::GetUserName() const
{
	return User;
}

//-----------------------------------------------------------------------------
// FKimuraSourceControlRevision::GetClientSpec
//-----------------------------------------------------------------------------
const FString& FKimuraSourceControlRevision::GetClientSpec() const
{
	return Workspace;
}

//-----------------------------------------------------------------------------
// FKimuraSourceControlRevision::GetAction
//-----------------------------------------------------------------------------
const FString& FKimuraSourceControlRevision::GetAction() const
{
	return Operation;
}

//-----------------------------------------------------------------------------
// FKimuraSourceControlRevision::GetBranchSource
//-----------------------------------------------------------------------------
TSharedPtr<ISourceControlRevision, ESPMode::ThreadSafe> FKimuraSourceControlRevision::GetBranchSource() const
{
	return nullptr;
}

//-----------------------------------------------------------------------------
// FKimuraSourceControlRevision::GetDate
//-----------------------------------------------------------------------------
const FDateTime& FKimuraSourceControlRevision::GetDate() const
{
	return Date;
}

//-----------------------------------------------------------------------------
// FKimuraSourceControlRevision::GetCheckInIdentifier
//-----------------------------------------------------------------------------
int32 FKimuraSourceControlRevision::GetCheckInIdentifier() const
{
	return CLID;
}

//-----------------------------------------------------------------------------
// FKimuraSourceControlRevision::GetFileSize
//-----------------------------------------------------------------------------
int32 FKimuraSourceControlRevision::GetFileSize() const
{
	return FileSize;
}

#undef LOCTEXT_NAMESPACE
