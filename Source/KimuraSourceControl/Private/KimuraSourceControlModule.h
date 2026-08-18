// Copyright Kimura Software Inc.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "KimuraSourceControlProvider.h"
#include "KimuraSourceControlSettings.h"
#include "KimuraSourceControlWorkspaceHost.h"

DECLARE_LOG_CATEGORY_EXTERN(LogKimuraSCM, Log, All);

class FKimuraSourceControlModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	static inline FKimuraSourceControlModule& Get()
	{
		return FModuleManager::LoadModuleChecked<FKimuraSourceControlModule>("KimuraSourceControl");
	}

	static FKimuraSourceControlSettings&		AccessSettings();
	static FKimuraSourceControlWorkspaceHost&	AccessWorkspaceHost();
	FKimuraSourceControlProvider				KimuraSourceControlProvider;
	FKimuraSourceControlSettings				Settings;

	FKimuraSourceControlWorkspaceHost			WorkspaceHost;

};
