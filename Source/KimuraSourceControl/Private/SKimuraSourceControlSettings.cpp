// Copyright Kimura Software Inc.

#include "SKimuraSourceControlSettings.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Input/SComboBox.h"
#include "EditorStyleSet.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "KimuraSourceControlModule.h"
#include "Templates/SharedPointer.h"
#include "EditorDirectories.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SFilePathPicker.h"

#define LOCTEXT_NAMESPACE "SKimuraSourceControlSettings"

//-----------------------------------------------------------------------------
// SKimuraSourceControlSettings::Construct
//-----------------------------------------------------------------------------
void SKimuraSourceControlSettings::Construct(const FArguments& InArgs)
{
	FSlateFontInfo Font = FAppStyle::GetFontStyle(TEXT("SourceControl.LoginWindow.Font"));

	this->ClientCertificateOptions.Reset();
	this->ClientCertificateOptions.Add(MakeShareable<FString>(new FString("None")));
	this->ClientCertificateOptions.Add(MakeShareable<FString>(new FString("From Store (certificate thumbprint)")));
	this->ClientCertificateOptions.Add(MakeShareable<FString>(new FString("From PFX file")));
	
	const FString FileFilterText = "Personal Information Exchange (*.pfx)|*.pfx";

	/** Blocking: fetches the workspaces that can be bound to this project */
	{
		TArray<FKimuraWorkspaceDesc> workspaceDescriptions;
		FKimuraSourceControlModule::Get().KimuraSourceControlProvider.GetAvailableWorkspacesForCurrentProject(workspaceDescriptions);

		WorkspaceOptions.Reset();
		for (const auto& w : workspaceDescriptions)
		{
			this->WorkspaceOptions.Add(MakeShareable<FString>(new FString(w.Name)));
		}

		// By default, assign the first choice to the settings.
		if (workspaceDescriptions.Num() > 0)
		{
			FKimuraSourceControlModule::AccessSettings().SetWorkspace(workspaceDescriptions[0].Name);
			FKimuraSourceControlModule::AccessSettings().Save();
		}
	}

	ChildSlot
	[
		SNew(SVerticalBox)
		+
		SVerticalBox::Slot()
		.Padding(2)
		[
			SNew( SBorder )
			.BorderImage(FAppStyle::GetBrush("DetailsView.CategoryBottom"))
			.Padding( FMargin( 0.0f, 3.0f, 0.0f, 0.0f ) )
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SNew(SVerticalBox)
					+SVerticalBox::Slot()
					.FillHeight(1.0f)
					.Padding(2.0f)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("lblWorkspace", "Workspace"))
						.ToolTipText(LOCTEXT("lblWorkspace_Tooltip", "Workspace"))
						.Font(Font)
					]
					+SVerticalBox::Slot()
					.FillHeight(1.0f)
					.Padding(2.0f)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("lblServerAddress", "Server address"))
						.ToolTipText(LOCTEXT("lblServerAddress_Tooltip", "Address of the Kimura SCM server, including the machine name and port. Example: HOSTMACHINE:8666"))
						.Font(Font)
					]
					+SVerticalBox::Slot()
					.FillHeight(1.0f)
					.Padding(2.0f)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("lblUsername", "User"))
						.ToolTipText(LOCTEXT("lblUsername_Tooltip", "User name"))
						.Font(Font)
					]
					+SVerticalBox::Slot()
					.FillHeight(1.0f)
					.Padding(2.0f)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("lblPassword", "Password"))
						.ToolTipText(LOCTEXT("lblPassword_Tooltip", "Password"))
						.Font(Font)
					]
					+SVerticalBox::Slot()
					.FillHeight(1.0f)
					.Padding(2.0f)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("lblVerifyCert", "Verify server certificate"))
						.ToolTipText(LOCTEXT("lblVerify_Tooltip", "When checked, the connection will only be established if the server can provide a trusted certificate."))
						.Font(Font)
					]
					+SVerticalBox::Slot()
					.FillHeight(1.3f)
					.Padding(2.0f)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("lblClientCertType", "Client certificate"))
						.ToolTipText(LOCTEXT("lblClientCertType_Tooltip", "Select the method by which you would like to provide a client side certificate to the server when connecting."))
						.Font(Font)
					]
				]
				+SHorizontalBox::Slot()
				.FillWidth(2.0f)
				[
					SNew(SVerticalBox)
					+SVerticalBox::Slot()
					.FillHeight(1.0f)
					.Padding(2.0f)
					[
						SAssignNew(this->WorkspaceComboBox, SComboBox<TSharedPtr<FString>>)
						.OptionsSource(&this->WorkspaceOptions)
						.OnGenerateWidget(this, &SKimuraSourceControlSettings::MakeWorkspaceComboButtonItemWidget)
						.OnSelectionChanged(this, &SKimuraSourceControlSettings::OnWorkspaceComboChanged)
						.OnComboBoxOpening(this, &SKimuraSourceControlSettings::OnWorkspaceComboOpened)
						[
							SNew(STextBlock)
							.Text(this,	&SKimuraSourceControlSettings::GetWorkspaceCombo)
						]
					]	
					+SVerticalBox::Slot()
					.FillHeight(1.0f)
					.Padding(2.0f)
					[
						SNew(SEditableTextBox)
						.Text(this, &SKimuraSourceControlSettings::GetServerAddressText)
						.ToolTipText(LOCTEXT("lblServerAddress_Tooltip", "Address of the Kimura SCM server. Includes the name and port of the machine. Example: HOSTMACHINE:8666"))
						.OnTextCommitted(this, &SKimuraSourceControlSettings::OnServerAddressCommitted)
						.OnTextChanged(this, &SKimuraSourceControlSettings::OnServerAddressCommitted, ETextCommit::Default)
						.Font(Font)
					]
					+SVerticalBox::Slot()
					.FillHeight(1.0f)
					.Padding(2.0f)
					[
						SNew(SEditableTextBox)
						.Text(this, &SKimuraSourceControlSettings::GetUserText)
						.ToolTipText(LOCTEXT("lblUsername_Tooltip", "User name"))
						.OnTextCommitted(this, &SKimuraSourceControlSettings::OnUserNameCommitted)
						.OnTextChanged(this, &SKimuraSourceControlSettings::OnUserNameCommitted, ETextCommit::Default)
						.Font(Font)
					]
					+SVerticalBox::Slot()
					.FillHeight(1.0f)
					.Padding(2.0f)
					[
						SAssignNew(PasswordTextBox, SEditableTextBox)
						.IsPassword(true)
						.ToolTipText(LOCTEXT("lblPassword_Tooltip", "Password"))
						.OnTextCommitted(this, &SKimuraSourceControlSettings::OnPasswordChanged)
						.OnTextChanged(this, &SKimuraSourceControlSettings::OnPasswordChanged, ETextCommit::Default)
						.Font(Font)
					]
					+SVerticalBox::Slot()
					.FillHeight(1.0f)
					.Padding(2.0f)
					[
						SNew(SCheckBox)
						.IsChecked(ECheckBoxState::Unchecked)
						.OnCheckStateChanged(this, &SKimuraSourceControlSettings::OnServerMustProvideTrustedCertificateChecked)
					]		
					+SVerticalBox::Slot()
					.FillHeight(1.3f)
					.Padding(2.0f)
					[
						SAssignNew(ClientCertificateComboBox, SComboBox<TSharedPtr<FString>>)
						.OptionsSource(&this->ClientCertificateOptions)
						.OnGenerateWidget(this, &SKimuraSourceControlSettings::MakeClientCertificateComboButtonItemWidget)
						.OnSelectionChanged(this, &SKimuraSourceControlSettings::OnClientCertificateComboChanged)
						.OnComboBoxOpening(this, &SKimuraSourceControlSettings::OnClientCertificateComboOpened)
						[
							SNew(STextBlock)
							.Text(this, &SKimuraSourceControlSettings::GetClientCertificateCombo)
						]
					]		
				] 
			]
		]
		+SVerticalBox::Slot()
		.AutoHeight()
		[
			// Client certificate by PFX file + password

			SNew( SBorder )
			.Padding( FMargin( 2.0f, 0.0f, 0.0f, 0.0f ) )
			.Visibility(this, &SKimuraSourceControlSettings::GetCertPFXOptionsVisibility)
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.AutoWidth()
				.FillWidth(1.0f)
				[
					SNew(SVerticalBox)
					+SVerticalBox::Slot()
					.FillHeight(1.0f)
					.Padding(FMargin(25.0f, 2.0f, 2.0f, 2.0f))
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("lblPFXFile", "PFX file"))
						.ToolTipText(LOCTEXT("lblLabelPFXPath_Tooltip", "Path to the client's Personal Information Exchange (PFX) file."))
						.Font(Font)
					]
					+SVerticalBox::Slot()
					.FillHeight(1.0f)
					.Padding(FMargin(25.0f, 2.0f, 2.0f, 2.0f))
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("lblPFXPassword", "PFX password"))
						.ToolTipText(LOCTEXT("lblPFXPassword_Tooltip", "Password to the client's Personal Information Exchange (PFX) file."))
						.Font(Font)
					]
				]
				+SHorizontalBox::Slot()
				.FillWidth(2.0f)
				[
					SNew(SVerticalBox)
					+SVerticalBox::Slot()
					.FillHeight(1.0f)
					.Padding(2.0f)
					[
						SNew(SFilePathPicker)
						.BrowseButtonImage(FAppStyle::GetBrush("PropertyWindow.Button_Ellipsis"))
						.BrowseButtonStyle(FAppStyle::Get(), "HoverHintOnly")
						.BrowseButtonToolTip(LOCTEXT("lblLabelPFXPath_Tooltip", "Path to the client's Personal Information Exchange (PFX) file."))
						.BrowseDirectory(FEditorDirectories::Get().GetLastDirectory(ELastDirectory::GENERIC_OPEN))
						.BrowseTitle(LOCTEXT("BinaryPathBrowseTitle", "File picker..."))
						.FilePath(this, &SKimuraSourceControlSettings::GetPFXPath)
						.FileTypeFilter(FileFilterText)
						.OnPathPicked(this, &SKimuraSourceControlSettings::OnPFXPathPicked)
					
					
					]
					+SVerticalBox::Slot()
					.FillHeight(1.0f)
					.Padding(2.0f)
					[
						SAssignNew(PFXPasswordTextBox, SEditableTextBox)
						.ToolTipText( LOCTEXT("lblPFXPassword_Tooltip", "Password required to open the Personal Information Exchange (PFX) file.") )
						.Font(Font)
						.OnTextCommitted(this, &SKimuraSourceControlSettings::OnPFXPasswordChanged)
						.OnTextChanged(this, &SKimuraSourceControlSettings::OnPFXPasswordChanged, ETextCommit::Default)
						.IsPassword(true)
					]
				]	
			]
		]
		+SVerticalBox::Slot()
		.AutoHeight()
		[
			// Client certificate from the store (by thumbprint)

			SNew( SBorder )
			.Padding( FMargin( 2.0f, 0.0f, 0.0f, 0.0f ) )
			.Visibility(this, &SKimuraSourceControlSettings::GetCertStoreOptionsVisibility)
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
					.AutoWidth()
					.FillWidth(1.0f)
				[
					SNew(SVerticalBox)
					+SVerticalBox::Slot()
					.FillHeight(1.0f)
					.Padding(FMargin(25.0f, 2.0f, 2.0f, 2.0f))
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("lblStoreCert", "Thumbprint"))
						.ToolTipText(LOCTEXT("lblStoreCert_Tooltip", "Should refer to a certificate in your personal certificate store that includes a private key."))
						.Font(Font)
					]
				]
				+SHorizontalBox::Slot()
				.FillWidth(2.0f)
				[
					SNew(SVerticalBox)
					+SVerticalBox::Slot()
					.FillHeight(1.0f)
					.Padding(2.0f)
					[
						SNew(SEditableTextBox)
						.Text(this, &SKimuraSourceControlSettings::GetCertStoreThumbprintText)
						.ToolTipText(LOCTEXT("lblStoreCert_Tooltip", "Should refer to a certificate in your personal certificate store that includes a private key."))
						.OnTextCommitted(this, &SKimuraSourceControlSettings::OnCertStoreThumbprintCommitted)
						.OnTextChanged(this, &SKimuraSourceControlSettings::OnCertStoreThumbprintCommitted, ETextCommit::Default)
						.Font(Font)
					]
				]	
			]
		]
	];

}


//-----------------------------------------------------------------------------
// SKimuraSourceControlSettings::GetServerAddressText
//-----------------------------------------------------------------------------
FText SKimuraSourceControlSettings::GetServerAddressText() const
{
	return FText::FromString(FKimuraSourceControlModule::AccessSettings().GetServerAddress());
}


//-----------------------------------------------------------------------------
// SKimuraSourceControlSettings::OnServerAddressCommitted
//-----------------------------------------------------------------------------
void SKimuraSourceControlSettings::OnServerAddressCommitted(const FText& InText, ETextCommit::Type InCommitType) const
{
	FKimuraSourceControlModule::AccessSettings().SetServerAddress(InText.ToString());
	FKimuraSourceControlModule::AccessSettings().Save();
}


//-----------------------------------------------------------------------------
// SKimuraSourceControlSettings::GetUserText
//-----------------------------------------------------------------------------
FText SKimuraSourceControlSettings::GetUserText() const
{
	return FText::FromString(FKimuraSourceControlModule::AccessSettings().GetUserName());
}


//-----------------------------------------------------------------------------
// SKimuraSourceControlSettings::OnUserNameCommitted
//-----------------------------------------------------------------------------
void SKimuraSourceControlSettings::OnUserNameCommitted(const FText& InText, ETextCommit::Type InCommitType) const
{
	FKimuraSourceControlModule::AccessSettings().SetUserName(InText.ToString());
	FKimuraSourceControlModule::AccessSettings().Save();
}


//-----------------------------------------------------------------------------
// SKimuraSourceControlSettings::OnPasswordChanged
//-----------------------------------------------------------------------------
void SKimuraSourceControlSettings::OnPasswordChanged(const FText& InText, ETextCommit::Type InCommitType) const
{
	FKimuraSourceControlModule::AccessSettings().SetPassword(InText.ToString());
}

//-----------------------------------------------------------------------------
// SKimuraSourceControlSettings::MakeWorkspaceComboButtonItemWidget
//-----------------------------------------------------------------------------
TSharedRef<SWidget> SKimuraSourceControlSettings::MakeWorkspaceComboButtonItemWidget(TSharedPtr<FString> StringItem)
{
	return SNew(STextBlock).Text(FText::FromString(*StringItem));
}


//-----------------------------------------------------------------------------
// SKimuraSourceControlSettings::OnWorkspaceComboChanged
//-----------------------------------------------------------------------------
void SKimuraSourceControlSettings::OnWorkspaceComboChanged(TSharedPtr<FString> Item, ESelectInfo::Type SelectInfo)
{
	FKimuraSourceControlModule::AccessSettings().SetWorkspace(*Item);
	FKimuraSourceControlModule::AccessSettings().Save();
}


//-----------------------------------------------------------------------------
// SKimuraSourceControlSettings::OnWorkspaceComboOpened
//-----------------------------------------------------------------------------
void SKimuraSourceControlSettings::OnWorkspaceComboOpened()
{
}


//-----------------------------------------------------------------------------
// SKimuraSourceControlSettings::GetWorkspaceCombo
//-----------------------------------------------------------------------------
FText SKimuraSourceControlSettings::GetWorkspaceCombo() const
{
	// only one available workspace? automatically select it
	if (this->WorkspaceOptions.Num() == 1)
	{
		return FText::FromString(*this->WorkspaceOptions[0]);
	}

	// try to find if the list contains the current workspace
	FString currentWorkspace = FKimuraSourceControlModule::AccessSettings().GetWorkspace();
	for (int i = 0; i < this->WorkspaceOptions.Num(); i++)
	{
		if (this->WorkspaceOptions[i].IsValid() && *this->WorkspaceOptions[i] == currentWorkspace)
		{
			return FText::FromString(*this->WorkspaceOptions[i]);
		}
	}

	// default is first option in the list
	if (this->WorkspaceOptions.Num() > 0)
	{
		return FText::FromString(*this->WorkspaceOptions[0]);
	}

	return FText::GetEmpty();
}


//-----------------------------------------------------------------------------
// SKimuraSourceControlSettings::OnServerMustProvideTrustedCertificateChecked
//-----------------------------------------------------------------------------
void SKimuraSourceControlSettings::OnServerMustProvideTrustedCertificateChecked(ECheckBoxState NewCheckedState)
{
	FKimuraSourceControlModule::AccessSettings().SetServerMustProvideTrustedCertificate(NewCheckedState == ECheckBoxState::Checked ? true : false);
	FKimuraSourceControlModule::AccessSettings().Save();
}


//-----------------------------------------------------------------------------
// SKimuraSourceControlSettings::MakeClientCertificateComboButtonItemWidget
//-----------------------------------------------------------------------------
TSharedRef<SWidget> SKimuraSourceControlSettings::MakeClientCertificateComboButtonItemWidget(TSharedPtr<FString> StringItem)
{
	return SNew(STextBlock).Text(FText::FromString(*StringItem));
}


//-----------------------------------------------------------------------------
// SKimuraSourceControlSettings::OnClientCertificateComboChanged
//-----------------------------------------------------------------------------
void SKimuraSourceControlSettings::OnClientCertificateComboChanged(TSharedPtr<FString> Item, ESelectInfo::Type SelectInfo)
{
	FKimuraSourceControlModule::AccessSettings().SetClientCertificateSource(this->ClientCertificateOptions.IndexOfByKey(Item));
	FKimuraSourceControlModule::AccessSettings().Save();
}


//-----------------------------------------------------------------------------
// SKimuraSourceControlSettings::OnClientCertificateComboOpened
//-----------------------------------------------------------------------------
void SKimuraSourceControlSettings::OnClientCertificateComboOpened()
{

}


//-----------------------------------------------------------------------------
// SKimuraSourceControlSettings::GetClientCertificateCombo
//-----------------------------------------------------------------------------
FText SKimuraSourceControlSettings::GetClientCertificateCombo() const
{
	int source = FKimuraSourceControlModule::AccessSettings().GetClientCertificateSource();
	
	return FText::FromString(*this->ClientCertificateOptions[source]);
}


//-----------------------------------------------------------------------------
// SKimuraSourceControlSettings::GetCertStoreOptionsVisibility
//-----------------------------------------------------------------------------
EVisibility SKimuraSourceControlSettings::GetCertStoreOptionsVisibility() const
{
	int source = FKimuraSourceControlModule::AccessSettings().GetClientCertificateSource();

	return source == 1 ? EVisibility::Visible : EVisibility::Collapsed;

}


//-----------------------------------------------------------------------------
// SKimuraSourceControlSettings::GetCertPFXOptionsVisibility
//-----------------------------------------------------------------------------
EVisibility SKimuraSourceControlSettings::GetCertPFXOptionsVisibility() const
{
	int source = FKimuraSourceControlModule::AccessSettings().GetClientCertificateSource();

	return source == 2 ? EVisibility::Visible : EVisibility::Collapsed;

}


//-----------------------------------------------------------------------------
// SKimuraSourceControlSettings::GetPFXPath
//-----------------------------------------------------------------------------
FString SKimuraSourceControlSettings::GetPFXPath() const
{
	return FKimuraSourceControlModule::AccessSettings().GetPFXFile();

}


//-----------------------------------------------------------------------------
// SKimuraSourceControlSettings::OnPFXPathPicked
//-----------------------------------------------------------------------------
void SKimuraSourceControlSettings::OnPFXPathPicked(const FString& PickedPath) const
{
	FString realPath = FPaths::ConvertRelativePathToFull("Z:\\KimuraSC\\KSCMCmdLineTool\\bin\\Debug\\net5.0\\", PickedPath);
// 	settings.ClientCertificatePFX = realPath;


	FKimuraSourceControlModule::AccessSettings().SetPFXFile(realPath);
	FKimuraSourceControlModule::AccessSettings().Save();
}


// ---------------------------------------------------------------------------- -
// SKimuraSourceControlSettings::OnPFXPasswordChanged
//-----------------------------------------------------------------------------
void SKimuraSourceControlSettings::OnPFXPasswordChanged(const FText& InPassword, ETextCommit::Type InCommitType) const
{
	FKimuraSourceControlModule::AccessSettings().SetPFXPassword(InPassword.ToString());
}

//-----------------------------------------------------------------------------
// SKimuraSourceControlSettings::GetCertStoreThumbprintText
//-----------------------------------------------------------------------------
FText SKimuraSourceControlSettings::GetCertStoreThumbprintText() const
{
	return FText::FromString(FKimuraSourceControlModule::AccessSettings().GetCertStoreThumbprint());
}


//-----------------------------------------------------------------------------
// SKimuraSourceControlSettings::OnCertStoreThumbprintCommitted
//-----------------------------------------------------------------------------
void SKimuraSourceControlSettings::OnCertStoreThumbprintCommitted(const FText& InText, ETextCommit::Type InCommitType) const
{
	FKimuraSourceControlModule::AccessSettings().SetCertStoreThumbprint(InText.ToString());
	FKimuraSourceControlModule::AccessSettings().Save();
}

#undef LOCTEXT_NAMESPACE
