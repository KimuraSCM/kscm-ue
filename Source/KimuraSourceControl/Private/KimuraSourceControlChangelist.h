// Copyright Kimura Software Inc.

#pragma once

#include "CoreMinimal.h"
#include "ISourceControlChangelist.h"

class FKimuraSourceControlChangelist : public ISourceControlChangelist
{
public:
	explicit FKimuraSourceControlChangelist(FString InCLID)
		: CLID(MoveTemp(InCLID))
	{
		if (CLID.Equals(TEXT("default"), ESearchCase::IgnoreCase))
		{
			CLID = TEXT("0");
		}
	}

	virtual bool CanDelete() const override
	{
		return !IsDefault();
	}

	virtual bool IsDefault() const override 
	{ 
		return CLID == TEXT("0");
	}

	FString ToString() const
	{
		if (IsDefault())
		{
			return TEXT("default");
		}

		return FString::Printf(TEXT("CL-%s"), *CLID);
	}

	virtual FString GetIdentifier() const override
	{
		return CLID;
	}

public:
	static const FKimuraSourceControlChangelist DefaultChangelist;

private:
	FString CLID;
};

typedef TSharedRef<class FKimuraSourceControlChangelist, ESPMode::ThreadSafe> FKimuraSourceControlChangelistRef;
