// Copyright Kimura Software Inc.

#pragma once

#include "CoreMinimal.h"

class FKimuraSourceControlSettings
{
	public:

		static FKimuraSourceControlSettings& Get();

		// Get the Kimura server address & port
		const FString& GetServerAddress() const;
		void SetServerAddress(const FString& InString);

		// Get the Kimura username
		const FString& GetUserName() const;
		void SetUserName(const FString& InString);

		// Set the Kimura password
		void SetPassword(const FString& InString);

		// Get the workspace
		const FString& GetWorkspace() const;
		void SetWorkspace(const FString& InString);

		const bool GetServerMustProvideTrustedCertificate() const;
		void SetServerMustProvideTrustedCertificate(bool InNewState);

		void SetClientCertificateSource(int InSource);
		int GetClientCertificateSource() const;

		const FString& GetPFXFile() const;
		void SetPFXFile(const FString& InString);

		const FString& GetPFXPassword() const;
		void SetPFXPassword(const FString& InString);

		const FString& GetCertStoreThumbprint() const;
		void SetCertStoreThumbprint(const FString& InString);

		// Load settings from ini file
		void Load();

		// Save settings to ini file
		void Save() const;

	public:

		// A critical section for settings access
		mutable FCriticalSection CriticalSection;

		FString		ServerAddress;

		FString		UserName;

		FString		Password;

		FString		Workspace;

		bool		ServerMustProvideTrustedCertificate = false;

		int			ClientCertSource = 0;
		FString		ClientCertificateThumbprint;
		FString		ClientCertificatePFX;
		FString		ClientCertificatePFXPassword;

		FString		SavedFolder;

};
