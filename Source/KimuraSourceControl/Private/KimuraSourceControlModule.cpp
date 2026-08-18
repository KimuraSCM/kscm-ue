// Copyright Kimura Software Inc.

#include "KimuraSourceControlModule.h"
#include "Features/IModularFeatures.h"
#include "HAL/FileManager.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/App.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#define LOCTEXT_NAMESPACE "KimuraSourceControl"

DEFINE_LOG_CATEGORY(LogKimuraSCM)

//-----------------------------------------------------------------------------
// FKimuraSourceControlModule::StartupModule
//-----------------------------------------------------------------------------
void FKimuraSourceControlModule::StartupModule()
{
	// Bind our source control provider to the editor
	IModularFeatures::Get().RegisterModularFeature("SourceControl", &KimuraSourceControlProvider);

	// load our settings
	this->Settings.Load();

	// Set up interoperability with kscm
	this->WorkspaceHost.Init();

}


//-----------------------------------------------------------------------------
// FKimuraSourceControlModule::ShutdownModule
//-----------------------------------------------------------------------------
void FKimuraSourceControlModule::ShutdownModule()
{
	// The worker can be blocked in SendCommand, so it must be stopped before the
	// process and its communication pipes are destroyed.
	this->KimuraSourceControlProvider.Close();
	
	this->WorkspaceHost.Shutdown();

	IModularFeatures::Get().UnregisterModularFeature("SourceControl", &KimuraSourceControlProvider);
}


//-----------------------------------------------------------------------------
// FKimuraSourceControlModule::AccessSettings
//-----------------------------------------------------------------------------
FKimuraSourceControlSettings& FKimuraSourceControlModule::AccessSettings()
{
	return FKimuraSourceControlModule::Get().Settings;
}


//-----------------------------------------------------------------------------
// FKimuraSourceControlModule::AccessWorkspaceHost
//-----------------------------------------------------------------------------
FKimuraSourceControlWorkspaceHost& FKimuraSourceControlModule::AccessWorkspaceHost()
{
	return FKimuraSourceControlModule::Get().WorkspaceHost;
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FKimuraSourceControlModule, KimuraSourceControl);
