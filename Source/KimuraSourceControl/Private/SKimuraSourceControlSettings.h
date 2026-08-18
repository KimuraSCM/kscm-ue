// Copyright Kimura Software Inc.

#pragma once

#include "CoreMinimal.h"
#include "Layout/Visibility.h"
#include "Input/Reply.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SWidget.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Views/STableViewBase.h"
#include "SourceControlOperationBase.h"
#include "ISourceControlProvider.h"

class SButton;

class SKimuraSourceControlSettings : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SKimuraSourceControlSettings) {}

	SLATE_END_ARGS()

public:

	void Construct(const FArguments& InArgs);
private:
	
	FText	GetServerAddressText() const;
	void	OnServerAddressCommitted(const FText& InText, ETextCommit::Type InCommitType) const;

	FText	GetUserText() const;
	void	OnUserNameCommitted(const FText& InText, ETextCommit::Type InCommitType) const;
	void	OnPasswordChanged(const FText& InText, ETextCommit::Type InCommitType) const;

	TSharedRef<SWidget> MakeWorkspaceComboButtonItemWidget(TSharedPtr<FString> StringItem);
	void OnWorkspaceComboChanged(TSharedPtr<FString> Item, ESelectInfo::Type SelectInfo);
	void OnWorkspaceComboOpened();
	FText GetWorkspaceCombo() const;

	void	OnServerMustProvideTrustedCertificateChecked(ECheckBoxState NewCheckedState);

	TSharedRef<SWidget> MakeClientCertificateComboButtonItemWidget(TSharedPtr<FString> StringItem);
	void OnClientCertificateComboChanged(TSharedPtr<FString> Item, ESelectInfo::Type SelectInfo);
	void OnClientCertificateComboOpened();
	FText GetClientCertificateCombo() const;

	EVisibility GetCertStoreOptionsVisibility() const;
	EVisibility GetCertPFXOptionsVisibility() const;

	FString GetPFXPath() const;
	void OnPFXPathPicked(const FString& PickedPath) const;

	void	OnPFXPasswordChanged(const FText& PickedPath, ETextCommit::Type InCommitType) const;


	FText	GetCertStoreThumbprintText() const;
	void	OnCertStoreThumbprintCommitted(const FText& InText, ETextCommit::Type InCommitType) const;

	TWeakPtr<class SEditableTextBox>			PasswordTextBox;

	TSharedPtr<SComboBox<TSharedPtr<FString>>>	WorkspaceComboBox;
	TArray<TSharedPtr<FString>>					WorkspaceOptions;

	TSharedPtr<SComboBox<TSharedPtr<FString>>>	ClientCertificateComboBox;
	TArray<TSharedPtr<FString>>					ClientCertificateOptions;

	TWeakPtr<class SEditableTextBox>			PFXPasswordTextBox;

};
