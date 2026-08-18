// Copyright Kimura Software Inc.

#include "KimuraSourceControlChangelistState.h"

#define LOCTEXT_NAMESPACE "KimuraSourceControl.ChangelistState"

//-----------------------------------------------------------------------------
// FKimuraSourceControlChangelistState::FKimuraSourceControlChangelistState
//-----------------------------------------------------------------------------
FKimuraSourceControlChangelistState::FKimuraSourceControlChangelistState(const FKimuraSourceControlChangelist& InChangelist)

	: Changelist(InChangelist)
	, bHasShelvedFiles(false)
{
}

//-----------------------------------------------------------------------------
// FKimuraSourceControlChangelistState::GetIconName
//-----------------------------------------------------------------------------
FName FKimuraSourceControlChangelistState::GetIconName() const
{
	return this->bHasShelvedFiles && this->ShelvedFiles.Num() > 0
		? FName("SourceControl.ShelvedCHangelist")
		: FName("SourceControl.Changelist");
}

//-----------------------------------------------------------------------------
// FKimuraSourceControlChangelistState::GetSmallIconName
//-----------------------------------------------------------------------------
FName FKimuraSourceControlChangelistState::GetSmallIconName() const
{
	return GetIconName();
}

//-----------------------------------------------------------------------------
// FKimuraSourceControlChangelistState::GetDisplayText
//-----------------------------------------------------------------------------
FText FKimuraSourceControlChangelistState::GetDisplayText() const
{
	return FText::FromString(Changelist.ToString());
}

//-----------------------------------------------------------------------------
// FKimuraSourceControlChangelistState::GetDescriptionText
//-----------------------------------------------------------------------------
FText FKimuraSourceControlChangelistState::GetDescriptionText() const
{
	return FText::FromString(Description);
}

//-----------------------------------------------------------------------------
// FKimuraSourceControlChangelistState::GetDisplayTooltip
//-----------------------------------------------------------------------------
FText FKimuraSourceControlChangelistState::GetDisplayTooltip() const
{
	return LOCTEXT("Tooltip", "Tooltip");
}

//-----------------------------------------------------------------------------
// FKimuraSourceControlChangelistState::GetTimeStamp
//-----------------------------------------------------------------------------
const FDateTime& FKimuraSourceControlChangelistState::GetTimeStamp() const
{
	return TimeStamp;
}

//-----------------------------------------------------------------------------
// FKimuraSourceControlChangelistState::GetFilesStates
//-----------------------------------------------------------------------------
const TArray<FSourceControlStateRef> FKimuraSourceControlChangelistState::GetFilesStates() const
{
	return Files;
}

//-----------------------------------------------------------------------------
// FKimuraSourceControlChangelistState::GetFilesStatesNum
//-----------------------------------------------------------------------------
int32 FKimuraSourceControlChangelistState::GetFilesStatesNum() const
{
	return Files.Num();
}

//-----------------------------------------------------------------------------
// FKimuraSourceControlChangelistState::GetShelvedFilesStates
//-----------------------------------------------------------------------------
const TArray<FSourceControlStateRef> FKimuraSourceControlChangelistState::GetShelvedFilesStates() const
{
	return ShelvedFiles;
}

//-----------------------------------------------------------------------------
// FKimuraSourceControlChangelistState::GetShelvedFilesStatesNum
//-----------------------------------------------------------------------------
int32 FKimuraSourceControlChangelistState::GetShelvedFilesStatesNum() const
{
	return ShelvedFiles.Num();
}

//-----------------------------------------------------------------------------
// FKimuraSourceControlChangelistState::GetChangelist
//-----------------------------------------------------------------------------
FSourceControlChangelistRef FKimuraSourceControlChangelistState::GetChangelist() const
{
	FKimuraSourceControlChangelistRef ChangelistCopy = MakeShareable( new FKimuraSourceControlChangelist(Changelist));
	return StaticCastSharedRef<ISourceControlChangelist>(ChangelistCopy);
}

#undef LOCTEXT_NAMESPACE
