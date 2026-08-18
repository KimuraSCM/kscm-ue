// Copyright Kimura Software Inc.

#pragma once

#include "CoreMinimal.h"
#include "KimuraSourceControlShared.generated.h"

#define UE_ENGINE_VERSION_LTE(Major, Minor) \
    ( (ENGINE_MAJOR_VERSION < (Major)) || \
      (ENGINE_MAJOR_VERSION == (Major) && ENGINE_MINOR_VERSION <= (Minor)) )

#define UE_ENGINE_VERSION_GTE(Major, Minor) \
    ( (ENGINE_MAJOR_VERSION > (Major)) || \
      (ENGINE_MAJOR_VERSION == (Major) && ENGINE_MINOR_VERSION >= (Minor)) )

//--------------------------------------------------------------
USTRUCT()
struct FKimuraVersion
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	FString ReleaseName = "BetaRelease";

	UPROPERTY()
	int Major = 0;

	UPROPERTY()
	int Minor = 0;

	UPROPERTY()
	int Patch = 0;

	FKimuraVersion()
	{
	}

	FKimuraVersion(int InMajor, int InMinor, int InPatch)
	{
		this->Major = InMajor;
		this->Minor = InMinor;
		this->Patch = InPatch;
	}
};


//--------------------------------------------------------------
USTRUCT()
struct FRepositoryLinkDesc
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	FString UID;

	UPROPERTY()
	FString WorkspacePath;

	UPROPERTY()
	FString Repository;

	UPROPERTY()
	FString Branch;
};

//--------------------------------------------------------------
USTRUCT()
struct FKimuraWorkspaceDesc
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	FString Name;

	UPROPERTY()
	FString Username;

	UPROPERTY()
	FString WorkspacePath;

	UPROPERTY()
	FString ServerName;

	UPROPERTY()
	FString ServerGUID;

	UPROPERTY()
	TArray<FRepositoryLinkDesc>	RepositoryLinks;

};

//--------------------------------------------------------------
USTRUCT()
struct FKimuraFileRevision
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	FString Description;

	UPROPERTY()
	int Revision = 0;

	UPROPERTY()
	FString Operation;

	UPROPERTY()
	FString User;

	UPROPERTY()
	FString Workspace;

	UPROPERTY()
	FString Branch;

	UPROPERTY()
	uint64 CLID = 0;

	UPROPERTY()
	uint64 Timestamp = 0;

	UPROPERTY()
	uint64 Filesize = 0;

	UPROPERTY()
	FString MD5;

};

//--------------------------------------------------------------
USTRUCT()
struct FKimuraFileHistory
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	uint64 WUID = 0;

	UPROPERTY()
	FString Name;

	UPROPERTY()
	TArray<FKimuraFileRevision>	Revisions;

};


//--------------------------------------------------------------
USTRUCT()
struct FKimuraFileOperation
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	uint64 SID = 0;

	UPROPERTY()
	FString U;

	UPROPERTY()
	FString W;

	UPROPERTY()
	FString Repo;

	UPROPERTY()
	FString B;

	UPROPERTY()
	uint64 RL = 0;

	UPROPERTY()
	FString Op;

	UPROPERTY()
	FString F;

	UPROPERTY()
	uint64 RUID = 0;

	UPROPERTY()
	uint64 R = 0;

	UPROPERTY()
	uint64 FS = 0;

	UPROPERTY()
	FString MD5;

	// Since Unreal JSON deserialization cannot handle `ulong`, we need to pass it as a string and convert it manually.
	UPROPERTY()
	FString SID_AsString;

	// Since Unreal JSON deserialization cannot handle `ulong`, we need to pass it as a string and convert it manually.
	UPROPERTY()
	FString RL_AsString;

	// Since Unreal JSON deserialization cannot handle `ulong`, we need to pass it as a string and convert it manually.
	UPROPERTY()
	FString RUID_AsString;

};

//--------------------------------------------------------------
USTRUCT()
struct FRepositoryFileOperations
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	FString UID;

	UPROPERTY()
	TArray<FKimuraFileOperation> Ops;

};

//--------------------------------------------------------------
USTRUCT()
struct FKimuraChangeListDescription
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	FString		Id;

	UPROPERTY()
	FString		Id_AsString;

	UPROPERTY()
	FString		Name;

	UPROPERTY()
	FString		Description;

	UPROPERTY()
	FString		Date;

	UPROPERTY()
	FString		User;

	UPROPERTY()
	TArray<uint64>	FilesByWUID;

	UPROPERTY()
	TArray<FString>	FilesByWUID_AsStrings;

};


//--------------------------------------------------------------
USTRUCT()
struct FKimuraStashDescription
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	bool 		IsLocal = false;

	UPROPERTY()
	bool 		IsChangeListRevision = false;

	UPROPERTY()
	uint64		Id = 0;

	UPROPERTY()
	FString		Name;

	UPROPERTY()
	FString		Description;

	UPROPERTY()
	FString		User;

	UPROPERTY()
	FString		Workspace;

	UPROPERTY()
	uint64		Timestamp = 0;

	UPROPERTY()
	FString		GUID;

	UPROPERTY()
	FString		StashBinding;

};

//--------------------------------------------------------------
UENUM()
enum class EKimuraResultCodes : uint32
{
	Ok = 0,
	Failed = 0xffffffff,
	Exception = 0xfffffffe,
	CryptoError = 0xfffffffd,
	LicenseExpired = 0xfffffffc,
	IncompatibleVersion = 0xfffffffb,
	Timeout = 0xfffffffa,
	ConnectionRefused = 0xffffffef,


	AdminError = 0x80000000,
	UserGroupAlreadyExists = 0x80000001,
	UserAlreadyExists = 0x80000002,
	InvalidUserGroup = 0x80000003,
	InvalidUser = 0x80000004,
	UserGroupNotEmpty = 0x80000005,
	UserGroupCannotBeDeleted = 0x80000006,
	FileServerAlreadyExists = 0x80000007,
	FileServerDoesNotExist = 0x80000008,
	FileServerPrimaryAccessAlreadyTaken = 0x80000009,
	FileServerCannotBeDeleted = 0x8000000a,
	RepositoryMapAlreadyExists = 0x8000000b,

	//RepositoryError                             = 0x81000000,
	InvalidRepository = 0x81000001,
	BranchAlreadyExists = 0x81000002,
	RepositoryAlreadyExists = 0x81000003,
	BranchCannotBeDeleted = 0x81000004,
	BranchHasChildBranches = 0x81000005,

	//BranchError                                 = 0x82000000,
	InvalidBranch = 0x82000001,
	InvalidFileOperation = 0x82000002,
	FileLockedByOtherUser = 0x82000003,
	FileLockedOutForSubmit = 0x82000004,
	FileNotAtHeadRevision = 0x82000005,
	FileAlreadyPresentAtHead = 0x82000006,
	FileDoesNotExist = 0x82000007,
	FileServerNotAvailable = 0x82000008,
	RequiresReadAccess = 0x82000009,
	RequiresWriteAccess = 0x8200000a,
	RequiresSuperUserAccess = 0x8200000b,
	FileStillPresent = 0x8200000c,
	NoFilesToSubmit = 0x8200000d,
	FileAlreadyDeletedAtHead = 0x8200000e,
	InvalidMoveDestination = 0x8200000f,
	InvalidMoveSource = 0x82000010,
	MissingMatchingMoveOp = 0x82000011,
	FileIsIgnored = 0x82000012,
	FileCannotBeMoved = 0x82000013,
	NoFilesToRevert = 0x82000014,
	InvalidMoveRepository = 0x82000015,
	NoFilesToPull = 0x82000016,


	UserError = 0x83000000,
	UserNotLoggedIn = 0x83000001,
	UserLoggedInFromAnotherMachine = 0x83000002,
	InvalidCredentials = 0x83000003,
	CannotResolveHost = 0x83000004,
	ConnectionLost = 0x83000005,
	InvalidCertificate = 0x83000006,
	InvalidServerCertificate = 0x83000007,
	BackupServerAccessDenied = 0x83000008,

	InvalidParameters = 0x84000000,
	NotImplemented = 0x84000001,
	FailedParameterValidation = 0x84000002,
	FailedParameterValidation_Severe = 0x84000003,

	// file server errors
	//InvalidFSAccess                             = 0x85000000,
	InvalidFTContractId = 0x85000001,
	FileAlreadyTransferred = 0x85000002,
	InvalidFTFileId = 0x85000003,
	FileTransferSizeDoesNotMatch = 0x85000004,
	FileTransferError = 0x85000005,
	FileTransferOutOfOrderData = 0x85000007,
	FileTransferFailedToOpenFile = 0x85000008,
	FileTransferFailedToReadFile = 0x85000009,
	FileTransferCanceled = 0x8500000a,
	FileTransferContinue = 0x8500000b,
	InvalidPullSessionId = 0x8500000c,
	FileTransferWaitForFS = 0x8500000d,
	InvalidMD5OnFile = 0x8500000e,
	ConnectionToFileServerFailed = 0x8500000f,

	// license server
	AuthenticationError = 0x86000000,
	FailedToCreateLicenseOrder = 0x86000001,
	InvalidLicenseSignature = 0x86000002,
	InvalidSignature = 0x86000003,

	// compare/merge tools
	CannotFindDiffTool = 0x87000001,
	FailedToRunDiffTool = 0x87000002,

	// stash
	FailedToBackupFilesToLocalStash = 0x89000000,
	NoStashAccess = 0x89000001,

	// attachments
	NoAttachmentsOnDefaultCL = 0x8a000000,

};


//-----------------------------------------------------------------------------
// FString_To_uint64
//-----------------------------------------------------------------------------
FORCEINLINE uint64 FString_To_uint64(const FString& InString)
{
	// Note: JSON deserialization in Unreal Engine has difficulty handling very large numbers. 
	// kscm's host-workspace bridge serializes these as strings, which are then converted back 
	// to uint64 by the plugin.

	uint64 u = FCString::Strtoui64(*InString, NULL, 10);
	return u;
}


//-----------------------------------------------------------------------------
// KimuraHash
//-----------------------------------------------------------------------------
inline uint32 KimuraHash(const FString& InString)
{
	int hash1 = (5381 << 16) + 5381;
	int hash2 = hash1;

	int32 strlen = InString.Len();

	for (int i = 0; i < strlen; i += 2)
	{
		hash1 = ((hash1 << 5) + hash1) ^ InString[i];
		if (i == strlen - 1)
			break;
		hash2 = ((hash2 << 5) + hash2) ^ InString[i + 1];
	}

	return hash1 + (hash2 * 1566083941);
}


//-----------------------------------------------------------------------------
// GetUID
//-----------------------------------------------------------------------------
inline uint64 GetUID(const FString& InFilename)
{

	FString full = InFilename.ToLower();
	FString filenameOnly = FPaths::GetCleanFilename(full);

	// generate hashes of full path and filename separately into unsigned values
	uint32 hash1 = (uint32)KimuraHash(full);
	uint32 hash2 = (uint32)KimuraHash(filenameOnly);

	uint64 lquad1 = (uint64)hash1;
	uint64 lquad2 = (uint64)hash2;

	// pack both separate hashes into a ulong
	uint64 uid = (lquad1 << 32) ^ lquad2;

	return uid;
}
