// Copyright Kimura Software Inc.

#pragma once

#include "CoreMinimal.h"
#include "Templates/Atomic.h"
#include "JsonObjectConverter.h"
#include <memory>
#include <queue>
#include <list>
#include "ISourceControlProvider.h"
#include "KimuraSourceControlShared.h"
#include "KimuraSourceControlConnectionState.h"
#include "SourceControlOperationBase.h"
#include "KimuraSourceControlOperation.generated.h"

//--------------------------------------------------------------
USTRUCT()
struct FKimuraParams
{
	GENERATED_USTRUCT_BODY()

	virtual ~FKimuraParams() {};

	UPROPERTY()
	FString Request;

};

//--------------------------------------------------------------
USTRUCT()
struct FKimuraFileOpParams : public FKimuraParams
{
	GENERATED_USTRUCT_BODY()

	virtual ~FKimuraFileOpParams() {};

	UPROPERTY()
	TArray<FString> Files;

};

//--------------------------------------------------------------
USTRUCT()
struct FKimuraResult
{
	GENERATED_USTRUCT_BODY()

	virtual ~FKimuraResult() {};

	UPROPERTY()
	FString Code = "0";

	UPROPERTY()
	FString Msg;

	FORCEINLINE bool HasSucceeded() const
	{
		return Code == "0";
	}

	EKimuraResultCodes	ToResultCode();
	virtual void Print();

};

//--------------------------------------------------------------
USTRUCT()
struct FKimuraFileOpError
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	FString	File;

	UPROPERTY()
	FString ErrorCode = "-1";

};

//--------------------------------------------------------------
USTRUCT()
struct FKimuraFileOpResult : public FKimuraResult
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	TArray<FKimuraFileOperation>	FileOpsToAdd;

	UPROPERTY()
	TArray<uint64>					FileOpIDsToRemove;

	UPROPERTY()
	TArray<FKimuraFileOpError>		Errors;

	virtual void Print() override;


};

//-----------------------------------------------------------------------------
// KimuraSourceControlOperation
//-----------------------------------------------------------------------------
class KimuraSourceControlOperation
{
public:		

	KimuraSourceControlOperation(TSharedRef<class ISourceControlOperation, ESPMode::ThreadSafe>	InOperation) 
		: Operation(InOperation)
	{};

	virtual ~KimuraSourceControlOperation() {};

	virtual FString GetName() = 0;

	virtual void Execute() = 0;

	bool BlockingWaitForCompletion(double TimeoutSeconds = 65.0);

	FORCEINLINE bool HasCompleted() const { return this->Completed; };
	FORCEINLINE bool HasSucceeded() const { return this->Completed && !this->Failed; };

	void MarkCanceled()
	{
		this->Canceled = true;
		this->Failed = true;
		this->Completed = true;
	}

	virtual void ApplyChangesToStates() { };

	virtual void PrintInfo();
	virtual void PrintAdditionalInfo() {};

	TSharedRef<class ISourceControlOperation, ESPMode::ThreadSafe>	Operation;
	TArray<FString>	Files;
	FSourceControlOperationComplete	OnOperationCompleteDelegate;


public:

	TAtomic<bool>	Canceled{ false };
	TAtomic<bool>	Completed{ false };
	TAtomic<bool>	Failed{ false };

	volatile double CreationTime = 0.0f;
	volatile double CompletionTime = 0.0f;

};


//-----------------------------------------------------------------------------
// KimuraSourceControlOperationImpl
//-----------------------------------------------------------------------------
template <typename P, typename T>
class KimuraSourceControlOperationImpl : public KimuraSourceControlOperation
{

	static_assert(std::is_base_of<FKimuraResult, T>::value, "KimuraSourceControlOperation's Result must derive from FKimuraResult");

	public:

		KimuraSourceControlOperationImpl(TSharedRef<class ISourceControlOperation, ESPMode::ThreadSafe>	InOperation)
			: KimuraSourceControlOperation(InOperation)
		{
		}

		P Params;
		T Result;

	protected:

		void ExecuteOnWorkspaceHost();

		virtual bool DeserializeResultFromString(const FString& InJson)
		{
			T tmpResult;

			if (!FJsonObjectConverter::JsonObjectStringToUStruct(InJson, &tmpResult, 0, 0))
			{
				return false;
			}

			this->Result = MoveTemp(tmpResult);
			return true;
		}
	
};


//--------------------------------------------------------------
USTRUCT()
struct FWorkspaceHostCommand
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	FString Command;

	UPROPERTY()
	FString Payload;

};

//-----------------------------------------------------------------------------
// KimuraOperationGetAvailableWorkspaces
//-----------------------------------------------------------------------------

	class FGetKimuraWorkspaces : public FSourceControlOperationBase
	{
		public:
			// ISourceControlOperation interface
			virtual FName GetName() const override
			{
				return "GetWorkspaces";
			}

		public:

			TArray<FKimuraWorkspaceDesc> WorkspaceDescriptions;
	};
	

	//--------------------------------------------------------------
	USTRUCT()
	struct FGetAvailableWorkspacesParams : public FKimuraParams
	{
		GENERATED_USTRUCT_BODY()

		UPROPERTY()
		FString ProjectPath;

	};

	//--------------------------------------------------------------
	USTRUCT()
	struct FGetAvailableWorkspacesResult : public FKimuraResult
	{
		GENERATED_USTRUCT_BODY()

		UPROPERTY()
		TArray<FKimuraWorkspaceDesc> WorkspaceDescriptions;

	};

	//--------------------------------------------------------------
	class KimuraOperationGetAvailableWorkspaces : public KimuraSourceControlOperationImpl<FGetAvailableWorkspacesParams, FGetAvailableWorkspacesResult>
	{
	public:
		KimuraOperationGetAvailableWorkspaces(TSharedRef<class ISourceControlOperation, ESPMode::ThreadSafe> InOperation)
			: KimuraSourceControlOperationImpl(InOperation)
		{
		}

		virtual FString GetName() override { return "GetAvailableWorkspacesByPath"; };

		virtual void Execute() override;

	};


//-----------------------------------------------------------------------------
// KimuraOperationConnect
//-----------------------------------------------------------------------------

	//--------------------------------------------------------------
	USTRUCT()
	struct FKimuraConnectParams : public FKimuraParams
	{
		GENERATED_USTRUCT_BODY()

		UPROPERTY()
		FString Workspace = "";

		UPROPERTY()
		FString ServerAddress = "";

		UPROPERTY()
		FString Username = "";

		UPROPERTY()
		FString Password = "";

		UPROPERTY()
		bool ServerMustProvideTrustedCertificate = false;

		UPROPERTY()
		FString ClientCertificateThumbprint = "";

		UPROPERTY()
		FString ClientCertificatePFX = "";

		UPROPERTY()
		FString ClientCertificatePFXPwd = "";

	};

	//--------------------------------------------------------------
	USTRUCT()
	struct FKimuraConnectResult : public FKimuraResult
	{
		GENERATED_USTRUCT_BODY()

		UPROPERTY()
		FKimuraWorkspaceDesc	Description;

	};

	//--------------------------------------------------------------
	class KimuraOperationConnect : public KimuraSourceControlOperationImpl<FKimuraConnectParams, FKimuraConnectResult>
	{
		public:
			KimuraOperationConnect(TSharedRef<class ISourceControlOperation, ESPMode::ThreadSafe> InOperation, TSharedRef<FKimuraConnectionState, ESPMode::ThreadSafe> InSharedConnectionState)
				: 
				KimuraSourceControlOperationImpl(InOperation),
				SharedConnectionState(InSharedConnectionState)
			{
			}

			virtual FString GetName() override { return "Connect"; };

			virtual void Execute() override;

			virtual void PrintAdditionalInfo() override;

			virtual void ApplyChangesToStates() override;

		private:
			
			// Shared with the KimuraSourceControlProvider's
			TSharedRef<FKimuraConnectionState, ESPMode::ThreadSafe> SharedConnectionState;

	};


//-----------------------------------------------------------------------------
// KimuraOperationUpdateStatus
//-----------------------------------------------------------------------------

	//--------------------------------------------------------------
	USTRUCT()
	struct FKimuraUpdateStatusParams : public FKimuraParams
	{
		GENERATED_USTRUCT_BODY()

		UPROPERTY()
		FString FileOpTS;

		UPROPERTY()
		FString CLID;

		UPROPERTY()
		TArray<FString>	Files;

	};


	//--------------------------------------------------------------
	USTRUCT()
	struct FKimuraUpdateStatusResult : public FKimuraResult
	{
		GENERATED_USTRUCT_BODY()

		UPROPERTY()
		uint64 LatestCLID = 0;

		UPROPERTY()
		FString LatestCLIDAsString;

		UPROPERTY()
		uint64 FileOpTimeStamp = 0;

		UPROPERTY()
		FString FileOpTimeStampAsString;

		UPROPERTY()
		TArray<FRepositoryFileOperations>	RepositoryFileOps;

		UPROPERTY()
		TMap<FString, int>	AvailableRevisionsPerWUID;

		UPROPERTY()
		TMap<FString, int>	FilesNotAtLatestRevision;

		UPROPERTY()
		TMap<FString, int>	FileStatesPerWUID;

		virtual void Print() override;

	};

	//--------------------------------------------------------------
	class KimuraOperationUpdateStatus : public KimuraSourceControlOperationImpl<FKimuraUpdateStatusParams, FKimuraUpdateStatusResult>
	{
		public:
			KimuraOperationUpdateStatus(TSharedRef<class ISourceControlOperation, ESPMode::ThreadSafe> InOperation)
				: KimuraSourceControlOperationImpl(InOperation)
			{
			}

			virtual FString GetName() override { return "UpdateStatus"; };

			virtual void Execute() override;

			virtual void PrintAdditionalInfo() override;

			virtual void ApplyChangesToStates() override;

	};


//-----------------------------------------------------------------------------
// KimuraOperationMarkForAdd
//-----------------------------------------------------------------------------
	//--------------------------------------------------------------
	USTRUCT()
	struct FKimuraMarkForAddResult : public FKimuraResult
	{
		GENERATED_USTRUCT_BODY()

		UPROPERTY()
		TArray<FRepositoryFileOperations>	RepositoryFileOps;

		UPROPERTY()
		TMap<FString, int>		FileStatesPerWUID;

	};

	//--------------------------------------------------------------
	class KimuraOperationMarkForAdd : public KimuraSourceControlOperationImpl<FKimuraFileOpParams, FKimuraMarkForAddResult>
	{
		public:
			KimuraOperationMarkForAdd(TSharedRef<class ISourceControlOperation, ESPMode::ThreadSafe> InOperation)
				: KimuraSourceControlOperationImpl(InOperation)
			{
			}

			virtual FString GetName() override { return "MarkForAdd"; };

			virtual void Execute() override;

			virtual void ApplyChangesToStates() override;

	};

	
//-----------------------------------------------------------------------------
// KimuraOperationMarkForEdit
//-----------------------------------------------------------------------------

	//--------------------------------------------------------------
	USTRUCT()
	struct FKimuraMarkForEditResult : public FKimuraResult
	{
		GENERATED_USTRUCT_BODY()

		UPROPERTY()
		TArray<FRepositoryFileOperations>	RepositoryFileOps;

		UPROPERTY()
		TMap<FString, int>		FileStatesPerWUID;

	};

	//--------------------------------------------------------------
	class KimuraOperationMarkForEdit : public KimuraSourceControlOperationImpl<FKimuraFileOpParams, FKimuraMarkForEditResult>
	{
		public:
			KimuraOperationMarkForEdit(TSharedRef<class ISourceControlOperation, ESPMode::ThreadSafe> InOperation)
				: KimuraSourceControlOperationImpl(InOperation)
			{
			}

			virtual FString GetName() override { return "MarkForEdit"; };

			virtual void Execute() override;

			virtual void ApplyChangesToStates() override;

	};


//-----------------------------------------------------------------------------
// KimuraOperationMarkForRemove
//-----------------------------------------------------------------------------

	//--------------------------------------------------------------
	USTRUCT()
	struct FKimuraMarkForRemoveResult : public FKimuraResult
	{
		GENERATED_USTRUCT_BODY()

		UPROPERTY()
		TArray<FRepositoryFileOperations>	RepositoryFileOps;

		UPROPERTY()
		TMap<FString, int>		FileStatesPerWUID;

	};

	//--------------------------------------------------------------
	class KimuraOperationMarkForRemove : public KimuraSourceControlOperationImpl<FKimuraFileOpParams, FKimuraMarkForRemoveResult>
	{
		public:
			KimuraOperationMarkForRemove(TSharedRef<class ISourceControlOperation, ESPMode::ThreadSafe> InOperation)
				: KimuraSourceControlOperationImpl(InOperation)
			{
			}

			virtual FString GetName() override { return "MarkForDelete"; };

			virtual void Execute() override;

			virtual void ApplyChangesToStates() override;

	};

	
//-----------------------------------------------------------------------------
// KimuraOperationRevert
//-----------------------------------------------------------------------------
	
	//--------------------------------------------------------------
	USTRUCT()
	struct FKimuraRevertParams : public FKimuraParams
	{
		GENERATED_USTRUCT_BODY()

		UPROPERTY()
		TArray<FString>	Files;

		UPROPERTY()
		bool				UnchangedOnly = false;

	};

	//--------------------------------------------------------------
	USTRUCT()
	struct FKimuraRevertResult : public FKimuraResult
	{
		GENERATED_USTRUCT_BODY()

		UPROPERTY()
		TArray<FRepositoryFileOperations>	RepositoryFileOps;

		UPROPERTY()
		TMap<FString, int>		FileStatesPerWUID;

	};

	//--------------------------------------------------------------
	class KimuraOperationRevert : public KimuraSourceControlOperationImpl<FKimuraRevertParams, FKimuraRevertResult>
	{
		public:
			KimuraOperationRevert(TSharedRef<class ISourceControlOperation, ESPMode::ThreadSafe> InOperation, bool bInUnchangedOnly = false)
				: KimuraSourceControlOperationImpl(InOperation)
				, bUnchangedOnly(bInUnchangedOnly)
			{
			}

			virtual FString GetName() override { return "Revert"; };

			virtual void Execute() override;

			virtual void ApplyChangesToStates() override;

		private:
			bool bUnchangedOnly = false;

	};


//-----------------------------------------------------------------------------
// KimuraOperationSubmitWorkspaceFiles
//-----------------------------------------------------------------------------
	//--------------------------------------------------------------
	USTRUCT()
	struct FKimuraSubmitWorkspaceFilesParams : public FKimuraParams
	{
		GENERATED_USTRUCT_BODY()

		UPROPERTY()
		TArray<FString>	Files;

		UPROPERTY()
		FString Description;

	};

	//--------------------------------------------------------------
	class KimuraOperationSubmitWorkspaceFiles: public KimuraSourceControlOperationImpl<FKimuraSubmitWorkspaceFilesParams, FKimuraFileOpResult>
	{
		public:
			KimuraOperationSubmitWorkspaceFiles(TSharedRef<class ISourceControlOperation, ESPMode::ThreadSafe> InOperation)
				: KimuraSourceControlOperationImpl(InOperation)
			{
			}

			virtual FString GetName() override { return "SubmitWorkspaceFiles"; };

			virtual void Execute() override;

			virtual void ApplyChangesToStates() override;

	};


//-----------------------------------------------------------------------------
// KimuraOperationSubmitWorkspaceChangelist
//-----------------------------------------------------------------------------
	//--------------------------------------------------------------
	USTRUCT()
	struct FKimuraSubmitWorkspaceChangelistParams : public FKimuraParams
	{
		GENERATED_USTRUCT_BODY()

		UPROPERTY()
		FString CLID;

		UPROPERTY()
		FString Description;

	};

	//--------------------------------------------------------------
	class KimuraOperationSubmitWorkspaceChangelist : public KimuraSourceControlOperationImpl<FKimuraSubmitWorkspaceChangelistParams, FKimuraFileOpResult>
	{
		public:
			KimuraOperationSubmitWorkspaceChangelist(TSharedRef<class ISourceControlOperation, ESPMode::ThreadSafe> InOperation, FSourceControlChangelistPtr InChangelist)
				: KimuraSourceControlOperationImpl(InOperation)
			{
				CLID = InChangelist->GetIdentifier();
			}

			FString CLID;

			virtual FString GetName() override { return "SubmitWorkspaceChangelist"; };

			virtual void Execute() override;

			virtual void ApplyChangesToStates() override;

	};

//-----------------------------------------------------------------------------
// KimuraOperationSync
//-----------------------------------------------------------------------------

	// Used when we need to sync an individual file at a specific revision.
	class FSyncFileRevision : public FSourceControlOperationBase
	{
	public:
		virtual FName GetName() const override
		{
			return "SyncFileRevision";
		}

		FString CLID;
		FString DestinationPath;
	};

	//--------------------------------------------------------------
	USTRUCT()
	struct FSyncParams : public FKimuraParams
	{
		GENERATED_USTRUCT_BODY()

		UPROPERTY()
		TArray<FString> Files;

		UPROPERTY()
		TArray<FString> Directories;

		UPROPERTY()
		FString CLID;

		UPROPERTY()
		bool Force = false;

		UPROPERTY()
		bool SyncCurrent = false;

		UPROPERTY()
		bool SyncLatest = false;

		UPROPERTY()
		FString DestinationPath;

	};
	//--------------------------------------------------------------
	class KimuraOperationSync : public KimuraSourceControlOperationImpl<FSyncParams, FKimuraFileOpResult>
	{
		public:
			KimuraOperationSync(TSharedRef<class ISourceControlOperation, ESPMode::ThreadSafe> InOperation)
				: KimuraSourceControlOperationImpl(InOperation)
			{
			}

			virtual FString GetName() override { return "Sync"; };

			virtual void Execute() override;

			virtual void ApplyChangesToStates() override;

	};


//-----------------------------------------------------------------------------
// KimuraOperationGetHistory
//-----------------------------------------------------------------------------

	//--------------------------------------------------------------
	USTRUCT()
	struct FGetHistoryParams : public FKimuraParams
	{
		GENERATED_USTRUCT_BODY()

		UPROPERTY()
		TArray<FString> Files;

	};

	//--------------------------------------------------------------
	USTRUCT()
	struct FKimuraGetHistoryResult : public FKimuraResult
	{
		GENERATED_USTRUCT_BODY()

		UPROPERTY()
		FString TargetType;

		UPROPERTY()
		TArray<FKimuraFileHistory>	FileHistories;

		virtual void Print() override;

	};

	//--------------------------------------------------------------
	class KimuraOperationGetHistory : public KimuraSourceControlOperationImpl<FGetHistoryParams, FKimuraGetHistoryResult>
	{
		public:
			KimuraOperationGetHistory(TSharedRef<class ISourceControlOperation, ESPMode::ThreadSafe> InOperation)
				: KimuraSourceControlOperationImpl(InOperation)
			{
			}

			virtual FString GetName() override { return "GetHistory"; };

			virtual void Execute() override;

			virtual void ApplyChangesToStates() override;

	};


//-----------------------------------------------------------------------------
// KimuraOperationUpdateChangelistsStatus
//-----------------------------------------------------------------------------

	//--------------------------------------------------------------
	USTRUCT()
	struct FKimuraUpdateChangelistsParams : public FKimuraParams
	{
		GENERATED_USTRUCT_BODY()

		/* Might not be necessary if a non-empty 'ChangeListsToUpdate' means we only wish to retrieve specific change lists */
		UPROPERTY()
		bool UpdateAllChangelists = false;

		UPROPERTY()
		bool UpdateFilesStates = false;

		UPROPERTY()
		bool UpdateShelvedFilesStates = false;

		UPROPERTY()
		TArray<FString>	ChangeListsToUpdate;

	};


	//--------------------------------------------------------------
	USTRUCT()
	struct FKimuraUpdateChangelistsResult : public FKimuraResult
	{
		GENERATED_USTRUCT_BODY()

		UPROPERTY()
		TArray<FKimuraChangeListDescription>		ChangeLists;

		UPROPERTY()
		TArray<FKimuraStashDescription>				Stashes;

		UPROPERTY()
		TArray<FRepositoryFileOperations>			RepositoryFileOps;

	};

	//--------------------------------------------------------------
	class KimuraOperationUpdateChangelistsStatus : public KimuraSourceControlOperationImpl<FKimuraUpdateChangelistsParams, FKimuraUpdateChangelistsResult>
	{
		public:
			KimuraOperationUpdateChangelistsStatus(TSharedRef<class ISourceControlOperation, ESPMode::ThreadSafe> InOperation)
				: KimuraSourceControlOperationImpl(InOperation)
			{
			}

			virtual FString GetName() override { return "UpdateChangelistsStatus"; };

			virtual void Execute() override;

			virtual void ApplyChangesToStates() override;

	};



//-----------------------------------------------------------------------------
// KimuraOperationCreateChangelist
//-----------------------------------------------------------------------------

	//--------------------------------------------------------------
	USTRUCT()
	struct FKimuraCreateChangelistParams : public FKimuraParams
	{
		GENERATED_USTRUCT_BODY()

		UPROPERTY()
		FString Description;

	};


	//--------------------------------------------------------------
	class KimuraOperationCreateChangelist : public KimuraSourceControlOperationImpl<FKimuraCreateChangelistParams, FKimuraUpdateChangelistsResult>
	{
		public:
			KimuraOperationCreateChangelist(TSharedRef<class ISourceControlOperation, ESPMode::ThreadSafe> InOperation)
				: KimuraSourceControlOperationImpl(InOperation)
			{
			}

			virtual FString GetName() override { return "CreateChangelist"; };

			virtual void Execute() override;

			virtual void ApplyChangesToStates() override;

	};



//-----------------------------------------------------------------------------
// KimuraOperationDeleteChangelist
//-----------------------------------------------------------------------------

	//--------------------------------------------------------------
	USTRUCT()
	struct FKimuraDeleteChangelistParams : public FKimuraParams
	{
		GENERATED_USTRUCT_BODY()

		UPROPERTY()
		FString CLID = "";

		UPROPERTY()
		bool RevertFiles = false;

	};


	//--------------------------------------------------------------
	class KimuraOperationDeleteChangelist : public KimuraSourceControlOperationImpl<FKimuraDeleteChangelistParams, FKimuraUpdateChangelistsResult>
	{
		public:
			KimuraOperationDeleteChangelist(TSharedRef<class ISourceControlOperation, ESPMode::ThreadSafe> InOperation, FSourceControlChangelistPtr InChangelist)
				: KimuraSourceControlOperationImpl(InOperation)
			{
				CLID = InChangelist->GetIdentifier();
			}

			FString CLID;

			virtual FString GetName() override { return "DeleteChangelist"; };

			virtual void Execute() override;

			virtual void ApplyChangesToStates() override;

	};



//-----------------------------------------------------------------------------
// KimuraOperationEditChangelist
//-----------------------------------------------------------------------------

	//--------------------------------------------------------------
	USTRUCT()
	struct FKimuraEditChangelistParams : public FKimuraParams
	{
		GENERATED_USTRUCT_BODY()

		UPROPERTY()
		FString CLID = "";

		UPROPERTY()
		FString Description = "";

	};


	//--------------------------------------------------------------
	class KimuraOperationEditChangelist : public KimuraSourceControlOperationImpl<FKimuraEditChangelistParams, FKimuraUpdateChangelistsResult>
	{
		public:
			KimuraOperationEditChangelist(TSharedRef<class ISourceControlOperation, ESPMode::ThreadSafe> InOperation, FSourceControlChangelistPtr InChangelist)
				: KimuraSourceControlOperationImpl(InOperation)
			{
				CLID = InChangelist->GetIdentifier();
			}

			FString CLID;

			virtual FString GetName() override { return "EditChangelist"; };

			virtual void Execute() override;

			virtual void ApplyChangesToStates() override;

	};



//-----------------------------------------------------------------------------
// KimuraOperationMoveToChangelist
//-----------------------------------------------------------------------------

	//--------------------------------------------------------------
	USTRUCT()
	struct FKimuraMoveToChangelistParams : public FKimuraParams
	{
		GENERATED_USTRUCT_BODY()

		UPROPERTY()
		FString CLID = "";

		UPROPERTY()
		TArray<FString> Files;

		UPROPERTY()
		FString Description = "";

	};


	//--------------------------------------------------------------
	class KimuraOperationMoveToChangelist : public KimuraSourceControlOperationImpl<FKimuraMoveToChangelistParams, FKimuraUpdateChangelistsResult>
	{
		public:
			KimuraOperationMoveToChangelist(TSharedRef<class ISourceControlOperation, ESPMode::ThreadSafe> InOperation, FSourceControlChangelistPtr InChangelist)
				: KimuraSourceControlOperationImpl(InOperation)
			{
				CLID = InChangelist->GetIdentifier();
			}

			FString CLID;

			virtual FString GetName() override { return "MoveToChangelist"; };

			virtual void Execute() override;

			virtual void ApplyChangesToStates() override;

	};

//-----------------------------------------------------------------------------
// KimuraOperationSave
//-----------------------------------------------------------------------------
	//--------------------------------------------------------------
	USTRUCT()
	struct FKimuraSaveResult : public FKimuraResult
	{
		GENERATED_USTRUCT_BODY()

		UPROPERTY()
		TArray<FRepositoryFileOperations>	RepositoryFileOps;

		UPROPERTY()
		TMap<FString, int>		FileStatesPerWUID;

	};
	//--------------------------------------------------------------
	class KimuraOperationSave : public KimuraSourceControlOperationImpl<FKimuraFileOpParams, FKimuraSaveResult>
	{
		public:
			KimuraOperationSave(TSharedRef<class ISourceControlOperation, ESPMode::ThreadSafe> InOperation)
				: KimuraSourceControlOperationImpl(InOperation)
			{
			}

			FString CLID;

			virtual FString GetName() override { return "Save"; };

			virtual void Execute() override;

			virtual void ApplyChangesToStates() override;

	};


//-----------------------------------------------------------------------------
// KimuraOperationWhere
//-----------------------------------------------------------------------------

	//--------------------------------------------------------------
	class KimuraOperationWhere : public KimuraSourceControlOperationImpl<FKimuraParams, FKimuraResult>
	{
		public:
			KimuraOperationWhere(TSharedRef<class ISourceControlOperation, ESPMode::ThreadSafe> InOperation)
				: KimuraSourceControlOperationImpl(InOperation)
			{
			}

			virtual FString GetName() override { return "Where"; };

			virtual void Execute() override;

			virtual void ApplyChangesToStates() override {};

	};

//-----------------------------------------------------------------------------
// KimuraOperationSilent
//-----------------------------------------------------------------------------


	//--------------------------------------------------------------
	class KimuraOperationSilent : public KimuraSourceControlOperationImpl<FKimuraParams, FKimuraResult>
	{
		public:
			KimuraOperationSilent(TSharedRef<class ISourceControlOperation, ESPMode::ThreadSafe> InOperation)
				: KimuraSourceControlOperationImpl(InOperation)
			{
			}

			FString CLID;

			virtual FString GetName() override { return "Silent"; };

			virtual void Execute() override;

			virtual void ApplyChangesToStates() override;

	};
