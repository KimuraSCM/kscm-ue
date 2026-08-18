// Copyright Kimura Software Inc.


#pragma once

#include "CoreMinimal.h"
#include "ISourceControlRevision.h"

class FKimuraSourceControlRevision : public ISourceControlRevision
{
public:
	FKimuraSourceControlRevision()
		: RevisionNumber(0)
		, Date(0)
		, CLID(0)
		, FileSize(0)
	{
	}

	// ISourceControlRevision interface
	virtual bool Get(FString& InOutFilename, EConcurrency::Type InConcurrency = EConcurrency::Synchronous) const override;
	virtual bool GetAnnotated( TArray<FAnnotationLine>& OutLines ) const override;
	virtual bool GetAnnotated( FString& InOutFilename ) const override;
	virtual const FString& GetFilename() const override;
	virtual int32 GetRevisionNumber() const override;
	virtual const FString& GetRevision() const override;
	virtual const FString& GetDescription() const override;
	virtual const FString& GetUserName() const override;
	virtual const FString& GetClientSpec() const override;
	virtual const FString& GetAction() const override;
	virtual TSharedPtr<ISourceControlRevision, ESPMode::ThreadSafe> GetBranchSource() const override;
	virtual const FDateTime& GetDate() const override;
	virtual int32 GetCheckInIdentifier() const override;
	virtual int32 GetFileSize() const override;

public:

	// The local filename the this revision refers to
	FString FileName;

	// The revision number of this file
	int32 RevisionNumber;

	// The revision to display to the user
	FString Revision;

	// The changelist description
	FString Description;

	// The user that made the change
	FString User;

	// The workspace the change belonged to
	FString Workspace;

	// The operation (edit, add etc.) that was performed
	FString Operation;

// 	// Source of branch, if any
// 	TSharedPtr<FKimuraSourceControlRevision, ESPMode::ThreadSafe> BranchSource;

	// The date of this revision
	FDateTime Date;

	// The id of the changelist containing this file revision
	int32 CLID;

	// The size of the file at this revision
	int32 FileSize;
};
