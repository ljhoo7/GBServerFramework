#include "Shared.h"
#include "TimerManager.h"
#include "HeartBeat.h"

namespace GenericBoson
{
	void HeartBeat::OnTime()
	{
		auto pOwner = m_wpOwner.lock();
		if (!pOwner)
		{
			return;
		}

		//pOwner->SendPing()
		
		TimerManager::GetInstance()->AddTimer(shared_from_this());
	}
}

