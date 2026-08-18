// Copyright Kimura Software Inc.

#pragma once

#include "CoreMinimal.h"
#include "HAL/CriticalSection.h"
#include "ISourceControlOperation.h"
#include "ISourceControlState.h"
#include "ISourceControlProvider.h"
#include "KimuraSourceControlConnectionState.h"
#include "KimuraSourceControlOperation.h"
#include "KimuraSourceControlShared.h"
#include "KimuraSourceControlState.h"
#include "KimuraSourceControlChangelistState.h"

class FRepositoryLink
{
	public:

		FRepositoryLinkDesc Description;

		TMap<uint64, FKimuraFileOperation>	FileOperations;

};

class FKimuraSourceControlProvider : public ISourceControlProvider
{
	public:
		/** Constructor */
		FKimuraSourceControlProvider()
			: ConnectionState(MakeShared<FKimuraConnectionState, ESPMode::ThreadSafe>())
		{
		}

		/* ISourceControlProvider implementation */
		virtual void Init(bool bForceConnection = true) override;
		virtual void Close() override;
		virtual const FName& GetName(void) const override;
		virtual FText GetStatusText() const override;
		virtual TMap<ISourceControlProvider::EStatus, FString> GetStatus() const override;
		virtual bool IsEnabled() const override;
		virtual bool IsAvailable() const override;
		virtual bool QueryStateBranchConfig(const FString& ConfigSrc, const FString& ConfigDest) override;
		virtual void RegisterStateBranches(const TArray<FString>& BranchNames, const FString& ContentRootIn) override;
		virtual int32 GetStateBranchIndex(const FString& BranchName) const override;
		virtual ECommandResult::Type GetState(const TArray<FString>& InFiles, TArray< TSharedRef<ISourceControlState, ESPMode::ThreadSafe> >& OutState, EStateCacheUsage::Type InStateCacheUsage) override;
		virtual ECommandResult::Type GetState(const TArray<FSourceControlChangelistRef>& InChangelists, TArray<FSourceControlChangelistStateRef>& OutState, EStateCacheUsage::Type InStateCacheUsage) override;
		virtual TArray<FSourceControlStateRef> GetCachedStateByPredicate(TFunctionRef<bool(const FSourceControlStateRef&)> Predicate) const override;
		virtual FDelegateHandle RegisterSourceControlStateChanged_Handle(const FSourceControlStateChanged::FDelegate& SourceControlStateChanged) override;
		virtual void UnregisterSourceControlStateChanged_Handle(FDelegateHandle Handle) override;
		virtual ECommandResult::Type Execute(const FSourceControlOperationRef& InOperation, FSourceControlChangelistPtr InChangelist, const TArray<FString>& InFiles, EConcurrency::Type InConcurrency = EConcurrency::Synchronous, const FSourceControlOperationComplete& InOperationCompleteDelegate = FSourceControlOperationComplete()) override;
		virtual bool CanExecuteOperation(const FSourceControlOperationRef&) const override;
		virtual bool CanCancelOperation(const TSharedRef<ISourceControlOperation, ESPMode::ThreadSafe>& InOperation) const override;
		virtual void CancelOperation(const TSharedRef<ISourceControlOperation, ESPMode::ThreadSafe>& InOperation) override;
		virtual TArray< TSharedRef<class ISourceControlLabel> > GetLabels(const FString& InMatchingSpec) const override;
		virtual TArray<FSourceControlChangelistRef> GetChangelists(EStateCacheUsage::Type InStateCacheUsage) override;
		virtual bool UsesLocalReadOnlyState() const override;
		virtual bool UsesChangelists() const override;
		virtual bool UsesUncontrolledChangelists() const override;
		virtual bool UsesCheckout() const override;
		virtual bool UsesFileRevisions() const override;
		virtual bool UsesSnapshots() const override;
		virtual bool AllowsDiffAgainstDepot() const override;

#if UE_ENGINE_VERSION_LTE(5, 7)
		virtual TOptional<bool> IsAtLatestRevision() const override;
		virtual TOptional<int> GetNumLocalChanges() const override;
#endif 

#if UE_ENGINE_VERSION_GTE(5, 7)
		virtual bool GetStateBranchAtIndex(int32 BranchIndex, FString& OutBranchName) const override { OutBranchName = ""; return false; }
#endif

#if UE_ENGINE_VERSION_GTE(5, 8)
		virtual bool UsesSoftRevertOnDelete() const override { return false; }
		virtual TOptional<bool> HasChangesToCheckIn() const override { return false; }
		virtual TOptional<bool> HasChangesToSync() const override { return false; }

#endif

		virtual void Tick() override;
#if SOURCE_CONTROL_WITH_SLATE
		virtual TSharedRef<class SWidget> MakeSettingsWidget() const override;
#endif

	protected:
		TSharedPtr<KimuraSourceControlOperation, ESPMode::ThreadSafe>	CreateKimuraOperation(TSharedRef<ISourceControlOperation, ESPMode::ThreadSafe> InOperation, FSourceControlChangelistPtr InChangelist, const TArray<FString>& InFiles, const FSourceControlOperationComplete& InDelegate);

	public:

		void AddRepositoryFileOperation(FRepositoryLink& InRepositoryLink, const FKimuraFileOperation& InOperation);
		bool ConvertAbsoluteFilenameToWorkspaceFilename(const FString& InAbsFilename, FString& OutWorkspacefilename) const;
		void ConvertWorkspaceFilenameToAbsoluteFilename(const FString& InWorkspacefilename, FString& OutAbsFilename) const;
		void GetAvailableWorkspacesForCurrentProject(TArray<FKimuraWorkspaceDesc>& OutWorkspaceDescriptions);
		void GetFilenamesFromChangelist(FString& InCLID, TArray<FString>& OutFiles);
		TSharedRef<FKimuraSourceControlChangelistState, ESPMode::ThreadSafe> GetOrCreateChangeListState(const FString& InCLID);
		TSharedRef<FKimuraSourceControlState, ESPMode::ThreadSafe> GetOrCreateState(const FString& InAbsFilename);
		FORCEINLINE bool IsFileOperationByLocalUser(const FRepositoryLink& InRepositoryLink, const FKimuraFileOperation& InOperation)
		{
			return (InOperation.U == this->WorkspaceDescription.Username && 
					InOperation.W == this->WorkspaceDescription.Name && 
					InOperation.RL_AsString == InRepositoryLink.Description.UID);
		}
		bool IsFileUnderWorkspace(const FString& InFilename) const;
		void RemoveRepositoryFileOperation(FRepositoryLink& InRepositoryLink, uint64 InOpId);
		void RequestStatusUpdateOnFiles(const TArray<FString>& InFiles);
		void RefreshWorldOutliner();
		void RequestUpdatePendingChanglistsStatus();
		void ResetConnectionState();
		void SetAvailable(bool InAvailable);
		void ClearRepositoryLinks();
		void UpdateWorkspaceDescriptionAndSetupRepositoryLinks(const FKimuraWorkspaceDesc& InWorkspaceDescription);
		void UpdateChangeListStates(const TArray<FKimuraChangeListDescription>& InNewChangeListDescriptions);
		void UpdateFileHistories(const TArray<FKimuraFileHistory>& InFileHistories);
		void UpdateFileRevisionsAvailable(const TMap<FString, int>& InNewRevisions);
		void UpdateFileStates(const TMap<FString, int>& InNewFileStates);
		void UpdateFilesNotAtLatestRevision(const TMap<FString, int>& InNewRevisions);
		void UpdateRepositoryFileOperations(const TArray<FRepositoryFileOperations>& InNewRepoFileOperations);

	public:
		const FKimuraWorkspaceDesc& GetWorkspaceDescription() { return this->WorkspaceDescription; }

	protected:

		// Enabled => Kimura SCM present and bridge to kscm.exe available
		bool									Enabled = false;	

		// Available => Workspace loaded and signed in to server
		bool									Available = false;	

		// Shareable connection state across provider and connect operations running on threads
		TSharedRef<FKimuraConnectionState, ESPMode::ThreadSafe> ConnectionState;

		// When the source control states need to be refreshed
		bool									Dirty = false;

		// Set of operations currently supported by the plugin
		TSet<FName>								SupportedOperations;

		TArray<TSharedPtr<KimuraSourceControlOperation, ESPMode::ThreadSafe>>	OngoingSourceControlOperations;
		FCriticalSection														OperationAdmissionMutex;

		// This thread executes our operations sequentially
		class FRunnableThread*					RunnableThread = nullptr;
		class KimuraSourceControlRunnable*		KimuraRunnableInstance = nullptr;

		FSourceControlStateChanged				OnSourceControlStateChanged;

		// Current workspace description
		FKimuraWorkspaceDesc					WorkspaceDescription;

		// Current revisions
		TMap<uint64, int>						AvailableRevisionsPerWUID;
		TMap<uint64, int>						FilesNotAtLatestRevision;

		// Files
		TMap<FString, TSharedRef<class FKimuraSourceControlState, ESPMode::ThreadSafe>> StateCache;
		TMap<uint64, TSharedRef<class FKimuraSourceControlState, ESPMode::ThreadSafe>>	StateCachePerWUID;

		// Pending files; these are files that were added to the StateCache and which might require being updated.
		TSet<FString>							PendingStatusFiles;

		// Change lists
		TMap<FString, TSharedRef<class FKimuraSourceControlChangelistState, ESPMode::ThreadSafe>> ChangeListStateCache;

		FORCEINLINE static FString MakeChangeListCacheKey(const FString& InCLID)
		{
			return InCLID.ToLower();
		}

		// Repository Links
		TMap<FString, FRepositoryLink>	RepositoryLinks;

	public:
		uint64								FileOpTimeStamp = 0;
		uint64								LatestKnownCLID = 0;
};
