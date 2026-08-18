// Copyright Kimura Software Inc.

#include "KimuraSourceControlWorkspaceHost.h"
#include "KimuraSourceControlModule.h"
#include "Misc/ScopeLock.h"

#if KIMURA_RECORD_HOST_COMMANDS
	#include "HAL/FileManager.h"
	#include "Misc/DateTime.h"
	#include "Misc/FileHelper.h"
	#include "Misc/Paths.h"
#endif

#if PLATFORM_WINDOWS

#pragma warning(push)
#pragma warning(disable : 4005)

	#include "Windows/AllowWindowsPlatformTypes.h"
	#include <Windows.h>
	#include <shellapi.h>
	#include <winreg.h>
	#include "Windows/HideWindowsPlatformTypes.h"

#pragma warning(pop)

#endif


//-----------------------------------------------------------------------------
// FKimuraSourceControlWorkspaceHost::Init
//-----------------------------------------------------------------------------
void FKimuraSourceControlWorkspaceHost::Init()
{
#if KIMURA_RECORD_HOST_COMMANDS
	this->InitializeCommandRecorder();
#endif

	// Find installation of Kimura SCM and kscm.exe. 
	FString kscmExec;
	{
		FString installPath;

		HKEY hKey;
		LONG Result = RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Kimura Software\\KimuraSCM", 0, KEY_READ, &hKey);
		if (Result != ERROR_SUCCESS)
		{
			UE_LOG(LogKimuraSCM, Error, TEXT("Failed to obtain Kimura SCM install from registry"));
			return;
		}

		WCHAR sSubKeyName[256];
		DWORD dwSubKeyNameLen = _countof(sSubKeyName);

		LONG hResultSubKey = RegQueryValueExW(hKey, L"InstallPath", 0, NULL, (LPBYTE)sSubKeyName, &dwSubKeyNameLen);
		RegCloseKey(hKey);
		if (hResultSubKey != ERROR_SUCCESS)
		{
			UE_LOG(LogKimuraSCM, Error, TEXT("Failed to retrieve Kimura SCM installation details from the registry."));
			return;
		}

		installPath = FString(sSubKeyName);

		if (!installPath.EndsWith("\\"))
		{
			installPath += "\\";
		}

		kscmExec = installPath + FString("kscm.exe");
	}

	// Find kscm executable
	if (!FPaths::FileExists(kscmExec))
	{
		UE_LOG(LogKimuraSCM, Error, TEXT("Failed to retrieve Kimura SCM installation details from the registry."));
		return;
	}

	// Create our pipes for communicating with the kscm process.
	// PluginReadPipe can receive from kscm
	// PluginWritePipe can send to kscm
	if (!FPlatformProcess::CreatePipe(this->PluginReadPipe, this->KscmWritePipe, false) ||
		!FPlatformProcess::CreatePipe(this->KscmReadPipe, this->PluginWritePipe, true))
	{
		UE_LOG(LogKimuraSCM, Error, TEXT("Failed to create pipes for the kscm process."));
		this->CloseProcessAndPipes();
		return;
	}

	// Create a Job Object to kill the kscm process if the editor
	// dies unexpectedly. This ensures we don't end up with orphaned kscm processes.
	HANDLE jobHandle = ::CreateJobObjectW(nullptr, nullptr);
	if (jobHandle)
	{
		JOBOBJECT_EXTENDED_LIMIT_INFORMATION JobInfo{};
		JobInfo.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
		if (::SetInformationJobObject(jobHandle, JobObjectExtendedLimitInformation, &JobInfo, sizeof(JobInfo)))
		{
			this->KscmJobHandle = jobHandle;
		}
		else
		{
			UE_LOG(LogKimuraSCM, Warning, TEXT("Could not configure the kscm Job Object (error %lu)."), ::GetLastError());
			::CloseHandle(jobHandle);
		}
	}
	else
	{
		UE_LOG(LogKimuraSCM, Warning, TEXT("Could not create the kscm Job Object (error %lu)."), ::GetLastError());
	}

	FString params = "host-workspace";

	this->KscmProcessHandle = FPlatformProcess::CreateProc(
		*kscmExec,
		*params,
		false,						// bLaunchDetached
		true,						// bLaunchHidden
		true,						// bLaunchReallyHidden
		nullptr,					// OutProcessID
		0,							// Priority
		nullptr,					// OptionalWorkingDirectory
		this->KscmWritePipe,		// Pipe for child stdin
		this->KscmReadPipe			// Pipe for child stdout
	);

	if (!this->KscmProcessHandle.IsValid())
	{
		UE_LOG(LogKimuraSCM, Error, TEXT("Failed to launch 'kscm' process."));
		this->CloseProcessAndPipes();
		return;
	}

	// Ensure the kscm process is bound to the lifetime of this editor.
	if (this->KscmJobHandle && !::AssignProcessToJobObject(static_cast<HANDLE>(this->KscmJobHandle), this->KscmProcessHandle.Get()))
	{
		UE_LOG(LogKimuraSCM, Warning, TEXT("Could not attach kscm to the editor Job Object (error %lu)."), ::GetLastError());
	}

	// Validate plugin version against installed kscm version
	{
		// Fetch version from kscm.
		FString versionAsJson;
		FString input = "{\"Command\" : \"version\" }";
		bool success = this->SendCommand(input, versionAsJson);

		FKimuraVersion v;
		success &= FJsonObjectConverter::JsonObjectStringToUStruct(versionAsJson, &v, 0, 0);

		// Early beta versions of kscm may introduce major changes, so even minor version numbers must match.
		if ( !success ||
		     (v.Major > this->Version.Major) ||
			 (v.Major == this->Version.Major && v.Minor > this->Version.Minor))
		{
			// The process and pipes are still valid, must close everything
			this->CloseProcessAndPipes();

			UE_LOG(LogKimuraSCM, Error, TEXT("Incompatible kscm version. Please update your plugin"));
			return;
		}
	}

	// Everything in order
	this->Valid = true;

	UE_LOG(LogKimuraSCM, Log, TEXT("Bridge to kscm ready"));

}


#if KIMURA_RECORD_HOST_COMMANDS
//-----------------------------------------------------------------------------
// FKimuraSourceControlWorkspaceHost::InitializeCommandRecorder
//-----------------------------------------------------------------------------
void FKimuraSourceControlWorkspaceHost::InitializeCommandRecorder()
{
	const FString recordDirectory = FPaths::ProjectSavedDir() / TEXT("KimuraSourceControl") / TEXT("HostTraces");
	this->CommandRecordPath = recordDirectory / FString::Printf(TEXT("host-%s.input.jsonl"), *FDateTime::UtcNow().ToString(TEXT("%Y%m%d-%H%M%S")));

	// make sure the directory exists
	const FString recordPathDirectory = FPaths::GetPath(this->CommandRecordPath);
	if (!IFileManager::Get().MakeDirectory(*recordPathDirectory, true))
	{
		UE_LOG(LogKimuraSCM, Warning, TEXT("Could not create host-command recording directory '%s'. Recording disabled."), *recordPathDirectory);
		this->CommandRecordPath.Empty();
		return;
	}

	UE_LOG(LogKimuraSCM, Log, TEXT("Host-workspace command recording enabled: %s"), *this->CommandRecordPath);
}


//-----------------------------------------------------------------------------
// FKimuraSourceControlWorkspaceHost::RecordCommand
//-----------------------------------------------------------------------------
void FKimuraSourceControlWorkspaceHost::RecordCommand(const FString& InCommand)
{
	if (this->CommandRecordPath.IsEmpty())
	{
		return;
	}

	// append command as a full line to the recording document
	const FString commandLine = InCommand + TEXT("\n");
	if (!FFileHelper::SaveStringToFile(commandLine, *this->CommandRecordPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM, &IFileManager::Get(), FILEWRITE_Append))
	{
		UE_LOG(LogKimuraSCM, Warning, TEXT("Failed to append host command recording '%s'."), *this->CommandRecordPath);
	}
}
#endif


//-----------------------------------------------------------------------------
// FKimuraSourceControlWorkspaceHost::SendCommand
//-----------------------------------------------------------------------------
bool FKimuraSourceControlWorkspaceHost::SendCommand(const FString& InCommand, FString& OutResponse, double TimeoutSeconds)
{
	FScopeLock ScopeLock(&this->CommandLock);

	OutResponse.Empty();

	auto OnFail = [this](const TCHAR* Reason) -> bool
	{
		UE_LOG(LogKimuraSCM, Error, TEXT("Communication with the kscm process failed: %s"), Reason);
		this->CloseProcessAndPipes();
		return false;
	};

	if (!this->Valid && !this->KscmProcessHandle.IsValid())
	{
		return OnFail(TEXT("the process is not running"));
	}

	if (!this->KscmProcessHandle.IsValid() || !this->PluginWritePipe || !this->PluginReadPipe)
	{
		return OnFail(TEXT("the process or communication pipes are invalid"));
	}

	if (!FPlatformProcess::IsProcRunning(this->KscmProcessHandle))
	{
		return OnFail(TEXT("the kscm process is no longer running"));
	}

	if (TimeoutSeconds <= 0.0)
	{
		return OnFail(TEXT("an invalid command timeout was supplied"));
	}

	// Ensure the command doesn't contain a delimited.
	if (InCommand.Contains(TEXT("\n")) || InCommand.Contains(TEXT("\r")))
	{
		return OnFail(TEXT("the command contains a newline delimiter"));
	}

	// Send our command to the kscm process
	const FString ToSend = InCommand + TEXT("\n");
	if (!FPlatformProcess::WritePipe(this->PluginWritePipe, ToSend, nullptr))
	{
		return OnFail(TEXT("writing the command failed"));
	}

#if KIMURA_RECORD_HOST_COMMANDS
	this->RecordCommand(InCommand);
#endif

	constexpr int32 MaxResponseCharacters = 16 * 1024 * 1024;
	const double Deadline = FPlatformTime::Seconds() + TimeoutSeconds;

	// Read the single response for this command. ReadPipe() may return fragments,
	// so accumulate the command over multiple calls.
	while (FPlatformTime::Seconds() < Deadline)
	{
		// Does the response end with a newline delimiter? If so, we're done here
		if (TryCompleteResponse(OutResponse))
		{
			return true;
		}

#if PLATFORM_WINDOWS && 0
		// On Windows, it is possible to peek at the pipe
		DWORD AvailableBytes = 0;
		if (!::PeekNamedPipe(static_cast<HANDLE>(this->PluginReadPipe), nullptr, 0, nullptr, &AvailableBytes, nullptr))
		{
			return OnFail(TEXT("peeking at the kscm pipe failed"));
		}
#endif

		const FString Chunk = FPlatformProcess::ReadPipe(this->PluginReadPipe);
		if (!Chunk.IsEmpty())
		{
			OutResponse += Chunk;
			if (OutResponse.Len() > MaxResponseCharacters)
			{
				return OnFail(TEXT("the response exceeded the maximum size"));
			}

			if (TryCompleteResponse(OutResponse))
			{
				return true;
			}
		}
		else
		{
			if (!FPlatformProcess::IsProcRunning(this->KscmProcessHandle))
			{
				return OnFail(TEXT("the kscm process exited before responding"));
			}

			// Sleep only when we couldn't obtain anything from the pipe
			FPlatformProcess::Sleep(0.01f);
		}
	}

	return OnFail(TEXT("timed out waiting for a response"));
}


//-----------------------------------------------------------------------------
// FKimuraSourceControlWorkspaceHost::TryCompleteResponse
//-----------------------------------------------------------------------------
bool FKimuraSourceControlWorkspaceHost::TryCompleteResponse(FString& InOutResponse)
{
	int32 NewlineIndex = INDEX_NONE;
	if (!InOutResponse.FindChar(TEXT('\n'), NewlineIndex))
	{
		return false;
	}

	// KSCM's host-workspace guarantees one response per command. 
	InOutResponse.LeftInline(NewlineIndex, EAllowShrinking::No);

	// Accept either LF or CRLF framing, but do not include the CR in JSON.
	if (InOutResponse.EndsWith(TEXT("\r")))
	{
		InOutResponse.LeftChopInline(1);
	}

	return !InOutResponse.IsEmpty();
}


//-----------------------------------------------------------------------------
// FKimuraSourceControlWorkspaceHost::CloseProcessAndPipes
//-----------------------------------------------------------------------------
void FKimuraSourceControlWorkspaceHost::CloseProcessAndPipes()
{
	this->Valid = false;

	if (this->KscmProcessHandle.IsValid())
	{
		// Optionally send "exit\n" or similar over stdin first
		FPlatformProcess::TerminateProc(this->KscmProcessHandle, true);
		FPlatformProcess::CloseProc(this->KscmProcessHandle);
		this->KscmProcessHandle.Reset();
	}

	if (KscmJobHandle)
	{
		// This also terminates any child processes kscm placed in the job.
		::CloseHandle(static_cast<HANDLE>(KscmJobHandle));
		KscmJobHandle = nullptr;
	}

	if (this->PluginReadPipe)
	{
		FPlatformProcess::ClosePipe(this->PluginReadPipe, KscmWritePipe);
		this->PluginReadPipe = this->KscmWritePipe = nullptr;
	}

	if (this->KscmReadPipe)
	{
		FPlatformProcess::ClosePipe(this->KscmReadPipe, this->PluginWritePipe);
		this->KscmReadPipe = this->PluginWritePipe = nullptr;
	}
}


//-----------------------------------------------------------------------------
// FKimuraSourceControlWorkspaceHost::Shutdown
//-----------------------------------------------------------------------------
void FKimuraSourceControlWorkspaceHost::Shutdown()
{
	FScopeLock ScopeLock(&this->CommandLock);
	this->CloseProcessAndPipes();
}
