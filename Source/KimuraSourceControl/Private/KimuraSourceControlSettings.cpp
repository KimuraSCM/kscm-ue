// Copyright Kimura Software Inc.

#include "KimuraSourceControlSettings.h"
#include "Misc/ScopeLock.h"
#include "Misc/ConfigCacheIni.h"
#include "Modules/ModuleManager.h"
#include "SourceControlHelpers.h"
#include "KimuraSourceControlModule.h"


namespace KimuraSettingsConstants
{

	/** The section of the ini file we load our settings from */
	static const FString SettingsSection = TEXT("KimuraSourceControl.KimuraSourceControlSettings");

}

//-----------------------------------------------------------------------------
// FKimuraSourceControlSettings::Get
//-----------------------------------------------------------------------------
FKimuraSourceControlSettings& FKimuraSourceControlSettings::Get()
{
	FKimuraSourceControlModule& KimuraSourceControlModule = FModuleManager::LoadModuleChecked<FKimuraSourceControlModule>("KimuraSourceControl");
	return KimuraSourceControlModule.AccessSettings();
}

//-----------------------------------------------------------------------------
// FKimuraSourceControlSettings::GetServerAddress
//-----------------------------------------------------------------------------
const FString& FKimuraSourceControlSettings::GetServerAddress() const
{
	FScopeLock ScopeLock(&CriticalSection);
	return this->ServerAddress;
}

//-----------------------------------------------------------------------------
// FKimuraSourceControlSettings::SetServerAddress
//-----------------------------------------------------------------------------
void FKimuraSourceControlSettings::SetServerAddress(const FString& InString)
{
	FScopeLock ScopeLock(&CriticalSection);
	this->ServerAddress = InString;
}

//-----------------------------------------------------------------------------
// FKimuraSourceControlSettings::GetUserName
//-----------------------------------------------------------------------------
const FString& FKimuraSourceControlSettings::GetUserName() const
{
	FScopeLock ScopeLock(&CriticalSection);
	return this->UserName;
}

//-----------------------------------------------------------------------------
// FKimuraSourceControlSettings::SetUserName
//-----------------------------------------------------------------------------
void FKimuraSourceControlSettings::SetUserName(const FString& InString)
{
	FScopeLock ScopeLock(&CriticalSection);
	this->UserName = InString;
}

//-----------------------------------------------------------------------------
// FKimuraSourceControlSettings::SetPassword
//-----------------------------------------------------------------------------
void FKimuraSourceControlSettings::SetPassword(const FString& InString)
{
	FScopeLock ScopeLock(&CriticalSection);
	this->Password = InString;
}

//-----------------------------------------------------------------------------
// FKimuraSourceControlSettings::GetWorkspace
//-----------------------------------------------------------------------------
const FString& FKimuraSourceControlSettings::GetWorkspace() const
{
	FScopeLock ScopeLock(&CriticalSection);
	return this->Workspace;
}

//-----------------------------------------------------------------------------
// FKimuraSourceControlSettings::SetWorkspace
//-----------------------------------------------------------------------------
void FKimuraSourceControlSettings::SetWorkspace(const FString& InString)
{
	FScopeLock ScopeLock(&CriticalSection);
	this->Workspace = InString;
}

//-----------------------------------------------------------------------------
// FKimuraSourceControlSettings::SetServerMustProvideTrustedCertificate
//-----------------------------------------------------------------------------
void FKimuraSourceControlSettings::SetServerMustProvideTrustedCertificate(bool InNewState)
{
	FScopeLock ScopeLock(&CriticalSection);
	this->ServerMustProvideTrustedCertificate = InNewState;
}

//-----------------------------------------------------------------------------
// FKimuraSourceControlSettings::GetServerMustProvideTrustedCertificate
//-----------------------------------------------------------------------------
const bool FKimuraSourceControlSettings::GetServerMustProvideTrustedCertificate() const
{
	FScopeLock ScopeLock(&CriticalSection);
	return this->ServerMustProvideTrustedCertificate;
}

//-----------------------------------------------------------------------------
// FKimuraSourceControlSettings::SetClientCertificateSource
//-----------------------------------------------------------------------------
void FKimuraSourceControlSettings::SetClientCertificateSource(int InSource)
{
	FScopeLock ScopeLock(&CriticalSection);
	this->ClientCertSource = InSource;
}

//-----------------------------------------------------------------------------
// FKimuraSourceControlSettings::GetClientCertificateSource
//-----------------------------------------------------------------------------
int FKimuraSourceControlSettings::GetClientCertificateSource() const
{
	FScopeLock ScopeLock(&CriticalSection);
	return this->ClientCertSource;
}

//-----------------------------------------------------------------------------
// FKimuraSourceControlSettings::GetPFXFile
//-----------------------------------------------------------------------------
const FString& FKimuraSourceControlSettings::GetPFXFile() const
{
	FScopeLock ScopeLock(&CriticalSection);
	return this->ClientCertificatePFX;
}

//-----------------------------------------------------------------------------
// FKimuraSourceControlSettings::SetPFXFile
//-----------------------------------------------------------------------------
void FKimuraSourceControlSettings::SetPFXFile(const FString& InString)
{
	FScopeLock ScopeLock(&CriticalSection);
	this->ClientCertificatePFX = InString;
}

//-----------------------------------------------------------------------------
// FKimuraSourceControlSettings::GetPFXPassword
//-----------------------------------------------------------------------------
const FString& FKimuraSourceControlSettings::GetPFXPassword() const
{
	FScopeLock ScopeLock(&CriticalSection);
	return this->ClientCertificatePFXPassword;

}

//-----------------------------------------------------------------------------
// FKimuraSourceControlSettings::SetPFXPassword
//-----------------------------------------------------------------------------
void FKimuraSourceControlSettings::SetPFXPassword(const FString& InString)
{
	FScopeLock ScopeLock(&CriticalSection);
	this->ClientCertificatePFXPassword = InString;
}

//-----------------------------------------------------------------------------
// FKimuraSourceControlSettings::GetCertStoreThumbprint
//-----------------------------------------------------------------------------
const FString& FKimuraSourceControlSettings::GetCertStoreThumbprint() const
{
	FScopeLock ScopeLock(&CriticalSection);
	return this->ClientCertificateThumbprint;
}

//-----------------------------------------------------------------------------
// FKimuraSourceControlSettings::SetCertStoreThumbprint
//-----------------------------------------------------------------------------
void FKimuraSourceControlSettings::SetCertStoreThumbprint(const FString& InString)
{
	FScopeLock ScopeLock(&CriticalSection);
	this->ClientCertificateThumbprint = InString;
}

//-----------------------------------------------------------------------------
// FKimuraSourceControlSettings::Load
//-----------------------------------------------------------------------------
void FKimuraSourceControlSettings::Load()
{
	FScopeLock ScopeLock(&CriticalSection);
	const FString& IniFile = SourceControlHelpers::GetSettingsIni();

	// folder where we can exchange files with the process
	this->SavedFolder = FPaths::ProjectSavedDir() / TEXT("KimuraSourceControl");

	GConfig->GetString(*KimuraSettingsConstants::SettingsSection, TEXT("Workspace"), this->Workspace, IniFile);
	GConfig->GetString(*KimuraSettingsConstants::SettingsSection, TEXT("Server"), this->ServerAddress, IniFile);
	GConfig->GetString(*KimuraSettingsConstants::SettingsSection, TEXT("UserName"), this->UserName, IniFile);
	GConfig->GetBool(*KimuraSettingsConstants::SettingsSection, TEXT("ServerMustProvideTrustedCertificate"), this->ServerMustProvideTrustedCertificate, IniFile);
	GConfig->GetInt(*KimuraSettingsConstants::SettingsSection, TEXT("ClientCertificate"), this->ClientCertSource, IniFile);
	GConfig->GetString(*KimuraSettingsConstants::SettingsSection, TEXT("PFXFile"), this->ClientCertificatePFX, IniFile);
	GConfig->GetString(*KimuraSettingsConstants::SettingsSection, TEXT("CertThumbprint"), this->ClientCertificateThumbprint, IniFile);
}

//-----------------------------------------------------------------------------
// FKimuraSourceControlSettings::Save
//-----------------------------------------------------------------------------
void FKimuraSourceControlSettings::Save() const
{

	FScopeLock ScopeLock(&CriticalSection);
	const FString& IniFile = SourceControlHelpers::GetSettingsIni();
	GConfig->SetString(*KimuraSettingsConstants::SettingsSection, TEXT("Workspace"), *this->Workspace, IniFile);
	GConfig->SetString(*KimuraSettingsConstants::SettingsSection, TEXT("Server"), *this->ServerAddress, IniFile);
	GConfig->SetString(*KimuraSettingsConstants::SettingsSection, TEXT("UserName"), *this->UserName, IniFile);
	GConfig->SetBool(*KimuraSettingsConstants::SettingsSection, TEXT("ServerMustProvideTrustedCertificate"), this->ServerMustProvideTrustedCertificate, IniFile);
	GConfig->SetInt(*KimuraSettingsConstants::SettingsSection, TEXT("ClientCertificate"), this->ClientCertSource, IniFile);
	GConfig->SetString(*KimuraSettingsConstants::SettingsSection, TEXT("PFXFile"), *this->ClientCertificatePFX, IniFile);
	GConfig->SetString(*KimuraSettingsConstants::SettingsSection, TEXT("CertThumbprint"), *this->ClientCertificateThumbprint, IniFile);
}
