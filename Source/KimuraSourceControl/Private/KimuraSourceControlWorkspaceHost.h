// Copyright Kimura Software Inc.

#pragma once

#include "CoreMinimal.h"
#include "HAL/CriticalSection.h"
#include "KimuraSourceControlShared.h"

class FKimuraSourceControlWorkspaceHost
{
public:

	void Init();
	void Shutdown();

	bool IsValid() { return this->Valid; }

	bool SendCommand(const FString& InCommand, FString& OutResponse, double TimeoutSeconds = 60.0);

	FKimuraVersion Version = FKimuraVersion(0, 8, 3);

protected:

	void CloseProcessAndPipes();

#if KIMURA_RECORD_HOST_COMMANDS
	// For debugging
	void InitializeCommandRecorder();
	void RecordCommand(const FString& InCommand);

	FString				CommandRecordPath;
#endif

	static bool			TryCompleteResponse(FString& InOutResponse);

	bool				Valid = false;

	// kscm process + pipes to exchange command/responses with it
	FProcHandle			KscmProcessHandle;
	void*				KscmWritePipe = nullptr;
	void*				KscmReadPipe = nullptr;
	void*				PluginWritePipe = nullptr;
	void*				PluginReadPipe = nullptr;

	// Ensures only one command is exchanged with the process at any time
	FCriticalSection 	CommandLock;

	// Job Object to kill the kscm process if the editor exits unexpectedly
	void*				KscmJobHandle = nullptr;

};
