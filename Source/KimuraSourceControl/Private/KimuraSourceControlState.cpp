// Copyright Kimura Software Inc.

#include "KimuraSourceControlState.h"
#include "KimuraSourceControlModule.h"
#include "KimuraSourceControlRevision.h"
#include "RevisionControlStyle/RevisionControlStyle.h"

#define LOCTEXT_NAMESPACE "KimuraSourceControl.State"



//-----------------------------------------------------------------------------
// FKimuraSourceControlState::FKimuraSourceControlState
//-----------------------------------------------------------------------------
FKimuraSourceControlState::FKimuraSourceControlState(const FString& InLocalFilename)
	: LocalFilename(InLocalFilename)
{

	// maps filename to workspace filename
	if (FKimuraSourceControlModule::Get().KimuraSourceControlProvider.ConvertAbsoluteFilenameToWorkspaceFilename(InLocalFilename, this->WorkspaceFilename))
	{
		// calculate WUID
		this->WUID = GetUID(this->WorkspaceFilename);
		this->UnderWorkspace = true;
	}
	else
	{
		// this file is not under the workspace so ignore it
		this->Ignored = true;
	}
}


//-----------------------------------------------------------------------------
// FKimuraSourceControlState::GetHistorySize
//-----------------------------------------------------------------------------
int32 FKimuraSourceControlState::GetHistorySize() const
{
	return this->Revisions.Num();
}


//-----------------------------------------------------------------------------
// FKimuraSourceControlState::GetHistoryItem
//-----------------------------------------------------------------------------
TSharedPtr<class ISourceControlRevision, ESPMode::ThreadSafe> FKimuraSourceControlState::GetHistoryItem(int32 RevisionIndex) const
{
	if (!this->Revisions.IsValidIndex(RevisionIndex))
	{
		return nullptr;
	}

	return this->Revisions[RevisionIndex];
}


//-----------------------------------------------------------------------------
// FKimuraSourceControlState::FindHistoryRevision
//-----------------------------------------------------------------------------
TSharedPtr<class ISourceControlRevision, ESPMode::ThreadSafe> FKimuraSourceControlState::FindHistoryRevision(int32 RevisionNumber) const
{
	for (const auto& Revision : this->Revisions)
	{
		if (Revision->GetRevisionNumber() == RevisionNumber)
		{
			return Revision;
		}
	}

	return nullptr;
}


//-----------------------------------------------------------------------------
// FKimuraSourceControlState::FindHistoryRevision
//-----------------------------------------------------------------------------
TSharedPtr<class ISourceControlRevision, ESPMode::ThreadSafe> FKimuraSourceControlState::FindHistoryRevision(const FString& InRevision) const
{
	for (const auto& Revision : this->Revisions)
	{
		if (Revision->GetRevision() == InRevision)
		{
			return Revision;
		}
	}

	return nullptr;
}


//-----------------------------------------------------------------------------
// FKimuraSourceControlState::GetIconName
//-----------------------------------------------------------------------------
FName FKimuraSourceControlState::GetIconName() const
{
	// Sometimes, unreal will ask about this file before we can even tell.
	if (!this->IsStateKnown())
	{
		return NAME_None;
	}

	if (this->IsIgnored())
	{
		return NAME_None;
	}

	if ((this->LocallyModified || this->MarkedForEdit || this->MarkedForDelete) && !this->IsCurrent())
	{
		return FName("RevisionControl.Conflicted");
	}

	if (this->LockedByOther > 0 || this->MarkedForEditByOthers > 0 || this->MarkedForDeleteByOthers > 0)
	{
		return FName("RevisionControl.CheckedOutByOtherUser");
	}

	if (!this->IsCurrent())
	{
		return FName("RevisionControl.NotAtHeadRevision");
	}

	if (this->MarkedForAdd)
	{
		return FName("RevisionControl.OpenForAdd");
	}
	if (this->MarkedForEdit)
	{
		return FName("RevisionControl.CheckedOut");
	}
	if (this->MarkedForDelete)
	{
		return FName("RevisionControl.MarkedForDelete");
	}

	if (this->Locked)
	{
		return FName("RevisionControl.Locked");
	}

	if (!this->IsSourceControlled())
	{
		return FName("RevisionControl.NotInDepot");
	}

	if (this->LocallyModified)
	{
		return FName("RevisionControl.ModifiedLocally");
	}

	return NAME_None;
}

#if SOURCE_CONTROL_WITH_SLATE
FSlateIcon FKimuraSourceControlState::GetIcon() const
{
	const FName IconName = this->GetIconName();
	if (IconName.IsNone())
	{
		return FSlateIcon();
	}

	if (IconName == FName("RevisionControl.CheckedOutByOtherUser"))
	{
		return FSlateIcon(FRevisionControlStyleManager::GetStyleSetName(), IconName, NAME_None, "RevisionControl.CheckedOutByOtherUserBadge");
	}

	return FSlateIcon(FRevisionControlStyleManager::GetStyleSetName(), IconName);
}
#endif // #if SOURCE_CONTROL_WITH_SLATE


//-----------------------------------------------------------------------------
// FKimuraSourceControlState::GetDisplayName
//-----------------------------------------------------------------------------
FText FKimuraSourceControlState::GetDisplayName() const
{
	if (!this->IsStateKnown())
	{
		return LOCTEXT("NeedsUpdate", "Needs to be updated");
	}

	if (this->IsIgnored())
	{
		return LOCTEXT("Ignored", "Ignored");
	}

	// Conflicted
	if ((this->LocallyModified || this->MarkedForEdit || this->MarkedForDelete) && !IsCurrent())
	{
		return LOCTEXT("Conflicted", "Conflicted");
	}
	
	// Operation by another user
	if (this->LockedByOther > 0 || this->MarkedForEditByOthers > 0 || this->MarkedForDeleteByOthers > 0)
	{
		if (this->OthersWithOps.Num() > 0)
		{
			if (this->MarkedForEditByOthers > 0)
			{
				return FText::Format(LOCTEXT("MarkedForEditByUser", "User {0} has this file marked for edit"), FText::FromString(this->OthersWithOps[0]));
			}
			else if (this->MarkedForDeleteByOthers > 0)
			{
				return FText::Format(LOCTEXT("MarkedForDeleteByUser", "User {0} has this file marked for delete"), FText::FromString(this->OthersWithOps[0]));
			}
			else if (this->LockedByOther > 0)
			{
				return FText::Format(LOCTEXT("LockedByUser", "User {0} has this file locked"), FText::FromString(this->OthersWithOps[0]));
			}
		}
		else
		{
			if (this->MarkedForEditByOthers > 0)
			{
				return LOCTEXT("MarkedForEditByAnother", "Another user has this file marked for edit");
			}
			else if (this->MarkedForDeleteByOthers > 0)
			{
				return LOCTEXT("MarkedForDeleteByAnother", "Another user has this file marked for delete");
			}
			else if (this->LockedByOther > 0)
			{
				return LOCTEXT("LockedByAnother", "Another user has this file locked");
			}
		}

		return LOCTEXT("CheckedOutOther", "Checked out by another user");
	}

	// Not at latest revision
	if (!IsCurrent())
	{
		return FText::Format(LOCTEXT("NotHeadRevision", "Not at head revision: {0}/{1}"), CurrentRevisionNumber, AvailableRevisions);
	}

	// Operations by local user
	if (this->MarkedForAdd)
	{
		return LOCTEXT("MarkedForAdd", "Marked for add");
	}
	else if (this->MarkedForEdit)
	{
		return LOCTEXT("MarkedForEdit", "Marked for edit");
	}
	else if (this->MarkedForDelete)
	{
		return LOCTEXT("MarkedForDelete", "Marked for delete");
	}
	
	// Locked by local user
	if (this->Locked)
	{
		return LOCTEXT("Locked", "Locked");
	}

	// Server doesn't know about this file
	if (!this->IsSourceControlled())
	{
		return LOCTEXT("NotInDepot", "Not in depot");
	}

	// Locally modified but not marked for edit
	if (this->LocallyModified)
	{
		return LOCTEXT("ModifiedLocally", "Modified locally");
	}

	return LOCTEXT("Unknown", "Unknown");
}


//-----------------------------------------------------------------------------
// FKimuraSourceControlState::GetDisplayTooltip
//-----------------------------------------------------------------------------
FText FKimuraSourceControlState::GetDisplayTooltip() const
{
	if (this->IsIgnored())
	{
		return LOCTEXT("Ignored", "Ignored");
	}

	// Conflicted
	if ((this->LocallyModified || this->MarkedForEdit || this->MarkedForDelete) && !IsCurrent())
	{
		return LOCTEXT("Conflicted", "Conflicted");
	}

	// Operation by another user
	if (this->LockedByOther > 0 || this->MarkedForEditByOthers > 0 || this->MarkedForDeleteByOthers > 0)
	{
		if (this->OthersWithOps.Num() > 0)
		{
			if (this->MarkedForEditByOthers > 0)
			{
				return FText::Format(LOCTEXT("MarkedForEditByUser", "User {0} has this file marked for edit"), FText::FromString(this->OthersWithOps[0]));
			}
			else if (this->MarkedForDeleteByOthers > 0)
			{
				return FText::Format(LOCTEXT("MarkedForDeleteByUser", "User {0} has this file marked for delete"), FText::FromString(this->OthersWithOps[0]));
			}
			else if (this->LockedByOther > 0)
			{
				return FText::Format(LOCTEXT("LockedByUser", "User {0} has this file locked"), FText::FromString(this->OthersWithOps[0]));
			}
		}
		else
		{
			if (this->MarkedForEditByOthers > 0)
			{
				return LOCTEXT("MarkedForEditByAnother", "Another user has this file marked for edit");
			}
			else if (this->MarkedForDeleteByOthers > 0)
			{
				return LOCTEXT("MarkedForDeleteByAnother", "Another user has this file marked for delete");
			}
			else if (this->LockedByOther > 0)
			{
				return LOCTEXT("LockedByAnother", "Another user has this file locked");
			}
		}

		return LOCTEXT("CheckedOutOther", "Checked out by another user");
	}

	// Not at latest revision
	if (!IsCurrent())
	{
		return FText::Format(LOCTEXT("NotHeadRevision", "Not at head revision: {0}/{1}"), CurrentRevisionNumber, AvailableRevisions);
	}

	// Operations by local user
	if (this->MarkedForAdd)
	{
		return LOCTEXT("MarkedForAdd", "Marked for add");
	}
	else if (this->MarkedForEdit)
	{
		return LOCTEXT("MarkedForEdit", "Marked for edit");
	}
	else if (this->MarkedForDelete)
	{
		return LOCTEXT("MarkedForDelete", "Marked for delete");
	}

	// Locked by local user
	if (this->Locked)
	{
		return LOCTEXT("Locked", "Locked");
	}

	// Server doesn't know about this file
	if (!this->IsSourceControlled())
	{
		return LOCTEXT("NotInDepot", "Not in depot");
	}

	// Locally modified but not marked for edit
	if (this->LocallyModified)
	{
		return LOCTEXT("ModifiedLocally", "Modified locally");
	}

	return LOCTEXT("Unknown", "Unknown");

}


//-----------------------------------------------------------------------------
// FKimuraSourceControlState::GetFilename
//-----------------------------------------------------------------------------
const FString& FKimuraSourceControlState::GetFilename() const
{
	return this->LocalFilename;
}

static FDateTime KimuraDefaultTimeStamp;

//-----------------------------------------------------------------------------
// FKimuraSourceControlState::GetTimeStamp
//-----------------------------------------------------------------------------
const FDateTime& FKimuraSourceControlState::GetTimeStamp() const
{
	// see note in definition
	return KimuraDefaultTimeStamp;//this->TimeStamp;
}


//-----------------------------------------------------------------------------
// FKimuraSourceControlState::CanCheckIn
//-----------------------------------------------------------------------------
bool FKimuraSourceControlState::CanCheckIn() const
{
	return this->IsStateKnown() &&
		(this->MarkedForEdit || this->MarkedForAdd || this->MarkedForDelete) &&
		this->LockedByOther == 0;
}


//-----------------------------------------------------------------------------
// FKimuraSourceControlState::CanCheckout
//-----------------------------------------------------------------------------
bool FKimuraSourceControlState::CanCheckout() const
{
	return	this->IsStateKnown() &&
			this->IsSourceControlled() &&
			this->LockedByOther == 0 && 
			!this->IsIgnored() && 
			(!this->MarkedForEdit && !this->MarkedForAdd && !this->MarkedForDelete);
}


//-----------------------------------------------------------------------------
// FKimuraSourceControlState::IsCheckedOut
//-----------------------------------------------------------------------------
bool FKimuraSourceControlState::IsCheckedOut() const
{
	return this->MarkedForEdit;
}


//-----------------------------------------------------------------------------
// FKimuraSourceControlState::IsCheckedOutOther
//-----------------------------------------------------------------------------
bool FKimuraSourceControlState::IsCheckedOutOther(FString* Who /*= NULL*/) const
{
	bool b = this->MarkedForEditByOthers > 0 || this->MarkedForDeleteByOthers > 0 || this->LockedByOther > 0;

	if (b)
	{
		if (this->OthersWithOps.Num() > 0 && Who != nullptr)
		{
			*Who = "";

			for (int i=0; i < this->OthersWithOps.Num(); i++)
			{
				*Who += this->OthersWithOps[i];
				if (i < this->OthersWithOps.Num()-1)
				{
					*Who += ", ";
				}
			}
		}
	}

	return b;
}


//-----------------------------------------------------------------------------
// FKimuraSourceControlState::IsCurrent
//-----------------------------------------------------------------------------
bool FKimuraSourceControlState::IsCurrent() const
{
	return this->IsStateKnown() && this->CurrentRevisionNumber == this->AvailableRevisions;
}


//-----------------------------------------------------------------------------
// FKimuraSourceControlState::IsSourceControlled
//-----------------------------------------------------------------------------
bool FKimuraSourceControlState::IsSourceControlled() const
{
	return this->IsStateKnown() && this->AvailableRevisions > 0;
}


//-----------------------------------------------------------------------------
// FKimuraSourceControlState::IsAdded
//-----------------------------------------------------------------------------
bool FKimuraSourceControlState::IsAdded() const
{
	return this->MarkedForAdd;
}


//-----------------------------------------------------------------------------
// FKimuraSourceControlState::IsDeleted
//-----------------------------------------------------------------------------
bool FKimuraSourceControlState::IsDeleted() const
{
	return this->MarkedForDelete;
}


//-----------------------------------------------------------------------------
// FKimuraSourceControlState::IsIgnored
//-----------------------------------------------------------------------------
bool FKimuraSourceControlState::IsIgnored() const
{
	// ignored, or not a valid workspace file
	return this->Ignored || this->WUID == 0;
}


//-----------------------------------------------------------------------------
// FKimuraSourceControlState::CanEdit
//-----------------------------------------------------------------------------
bool FKimuraSourceControlState::CanEdit() const
{
	return	this->IsStateKnown() &&
			this->LockedByOther == 0 &&
			this->IsSourceControlled() && 
			this->MarkedForDelete == false && 
			this->MarkedForEdit == false;
}


//-----------------------------------------------------------------------------
// FKimuraSourceControlState::IsUnknown
//-----------------------------------------------------------------------------
bool FKimuraSourceControlState::IsUnknown() const
{
	return !this->IsStateKnown();
}


//-----------------------------------------------------------------------------
// FKimuraSourceControlState::IsModified
//-----------------------------------------------------------------------------
bool FKimuraSourceControlState::IsModified() const
{
	return this->LocallyModified;
}


//-----------------------------------------------------------------------------
// FKimuraSourceControlState::CanAdd
//-----------------------------------------------------------------------------
bool FKimuraSourceControlState::CanAdd() const
{
	return this->IsStateKnown() &&
			!this->IsSourceControlled() && !this->IsIgnored() && !this->IsAdded();
}


//-----------------------------------------------------------------------------
// FKimuraSourceControlState::CanDelete
//-----------------------------------------------------------------------------
bool FKimuraSourceControlState::CanDelete() const
{
	return this->IsStateKnown() && !IsCheckedOutOther() && IsSourceControlled() && IsCurrent();
}


//-----------------------------------------------------------------------------
// FKimuraSourceControlState::IsConflicted
//-----------------------------------------------------------------------------
bool FKimuraSourceControlState::IsConflicted() const
{
	return (MarkedForEdit || MarkedForAdd || MarkedForDelete) && !this->IsCurrent();
}


//-----------------------------------------------------------------------------
// FKimuraSourceControlState::CanRevert
//-----------------------------------------------------------------------------
bool FKimuraSourceControlState::CanRevert() const
{
	return this->IsStateKnown() && (MarkedForEdit || MarkedForDelete || MarkedForAdd);
}


//-----------------------------------------------------------------------------
// FKimuraSourceControlState::GetCurrentRevision
//-----------------------------------------------------------------------------
TSharedPtr<class ISourceControlRevision, ESPMode::ThreadSafe> FKimuraSourceControlState::GetCurrentRevision() const
{
	if (this->CurrentRevisionNumber <= 0)
	{
		return nullptr;
	}

	return this->FindHistoryRevision(this->CurrentRevisionNumber);
}


//-----------------------------------------------------------------------------
// FKimuraSourceControlState::UpdateHistory
//-----------------------------------------------------------------------------
void FKimuraSourceControlState::UpdateHistory(const FKimuraFileHistory& InNewHistory)
{
	this->Revisions.Empty();

	for (const FKimuraFileRevision& r : InNewHistory.Revisions)
	{
		TSharedRef<FKimuraSourceControlRevision, ESPMode::ThreadSafe> revision = MakeShareable(new FKimuraSourceControlRevision());
		revision->FileName = InNewHistory.Name;
		revision->RevisionNumber = r.Revision;
		revision->Revision = FString::Printf(TEXT("%d"), r.Revision);
		revision->Description = r.Description;
		revision->User = r.User;
		revision->Workspace = r.Workspace;
		revision->Operation = r.Operation;
		revision->Date = FDateTime(r.Timestamp);
		revision->CLID = r.CLID;
		revision->FileSize = r.Filesize;

		this->Revisions.Add(revision);
	}
}

#undef LOCTEXT_NAMESPACE
