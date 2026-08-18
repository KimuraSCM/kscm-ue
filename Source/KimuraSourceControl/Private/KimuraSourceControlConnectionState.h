// Copyright Kimura Software Inc.

#pragma once

#include "CoreMinimal.h"
#include "Templates/Atomic.h"

//-----------------------------------------------------------------------------
// FKimuraConnectionState
//-----------------------------------------------------------------------------
class FKimuraConnectionState
{
	public:

		enum class EState : uint8
		{
			Disconnected,
			Connecting,
			Connected
		};

		bool IsConnected() const
		{
			return this->State.Load() == static_cast<uint8>(EState::Connected);
		}

		void MarkConnecting()
		{
			this->State = static_cast<uint8>(EState::Connecting);
		}

		void MarkConnected()
		{
			this->State = static_cast<uint8>(EState::Connected);
		}

		void MarkDisconnected()
		{
			this->State = static_cast<uint8>(EState::Disconnected);
		}

	private:

		TAtomic<uint8> State{ static_cast<uint8>(EState::Disconnected) };

};
