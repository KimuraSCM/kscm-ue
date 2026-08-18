// Copyright Kimura Software Inc.

#pragma once

#include "CoreMinimal.h"
#include "ISourceControlRevision.h"
#include "ISourceControlState.h"
#include "KimuraSourceControlRevision.h"
#include "KimuraSourceControlShared.h"

class FKimuraSourceControlState : public ISourceControlState
{
public:
	FKimuraSourceControlState( const FString& InLocalFilename );


	/** ISourceControlState interface */
	virtual int32 GetHistorySize() const override;
	virtual TSharedPtr<class ISourceControlRevision, ESPMode::ThreadSafe> GetHistoryItem( int32 HistoryIndex ) const override;
	virtual TSharedPtr<class ISourceControlRevision, ESPMode::ThreadSafe> FindHistoryRevision( int32 RevisionNumber ) const override;
	virtual TSharedPtr<class ISourceControlRevision, ESPMode::ThreadSafe> FindHistoryRevision( const FString& InRevision ) const override;
	virtual FName GetIconName() const override;
#if SOURCE_CONTROL_WITH_SLATE
	virtual FSlateIcon GetIcon() const override;
#endif
	virtual FText GetDisplayName() const override;
	virtual FText GetDisplayTooltip() const override;
	virtual const FString& GetFilename() const override;
	virtual const FDateTime& GetTimeStamp() const override;
	virtual bool CanCheckIn() const override;
	virtual bool CanCheckout() const override;
	virtual bool IsCheckedOut() const override;
	virtual bool IsCheckedOutOther(FString* Who = NULL) const override;
	virtual bool IsCheckedOutInOtherBranch(const FString& CurrentBranch = FString()) const override { return false; }
	virtual bool IsModifiedInOtherBranch(const FString& CurrentBranch = FString()) const override { return false; }
	virtual bool IsCheckedOutOrModifiedInOtherBranch(const FString& CurrentBranch = FString()) const override { return IsCheckedOutInOtherBranch(CurrentBranch) || IsModifiedInOtherBranch(CurrentBranch); }
	virtual TArray<FString> GetCheckedOutBranches() const override { return TArray<FString>(); }
	virtual FString GetOtherUserBranchCheckedOuts() const override { return FString(); }
	virtual bool GetOtherBranchHeadModification(FString& HeadBranchOut, FString& ActionOut, int32& HeadChangeListOut) const override { return false;  }
	virtual bool IsCurrent() const override;
	virtual bool IsSourceControlled() const override;
	virtual bool IsAdded() const override;
	virtual bool IsDeleted() const override;
	virtual bool IsIgnored() const override;
	virtual bool CanEdit() const override;
	virtual bool IsUnknown() const override;
	virtual bool IsModified() const override;
	virtual bool CanAdd() const override;
	virtual bool CanDelete() const override;
	virtual bool IsConflicted() const override;
	virtual bool CanRevert() const override;

	virtual TSharedPtr<class ISourceControlRevision, ESPMode::ThreadSafe> GetCurrentRevision() const override;

	void UpdateHistory(const FKimuraFileHistory& InNewHistory);

	// Whether this state has received a complete status refresh.
	FORCEINLINE bool IsStateKnown() const
	{
		return this->StateKnown;
	}

public:

	// Filename on disk
	FString LocalFilename;

	// Filename relative to workspace (not necessarily to repository)
	FString WorkspaceFilename;

	// The timestamp of the last update.
	//		NOTE: We tried to use timestamp as a way of knowing when a state was known, and tell 
	//		the editor when a file was last updated, but that would introduce long delays between 
	//		refreshes. Instead, we simply introduced 'StateKnown'
	// FDateTime TimeStamp;

	// Flipped the first time we know for sure what's the status on this file.
	bool StateKnown = false;

	// Workspace Unique Id. Only valid when the file is under the workspace path.
	uint64 WUID = 0;

	bool UnderWorkspace = false;

	int CurrentRevisionNumber = 0;
	int AvailableRevisions = 0;

	bool Ignored = false;
	bool LocallyModified = false;

	bool MarkedForEdit = false;
	bool MarkedForAdd = false;
	bool MarkedForDelete = false;

	bool Locked = false;
	
	int LockedByOther = 0;

	int MarkedForEditByOthers = 0;
	int MarkedForDeleteByOthers = 0;
	TArray<FString> OthersWithOps;

	// History
	TArray<TSharedRef<FKimuraSourceControlRevision, ESPMode::ThreadSafe>> Revisions;



};
