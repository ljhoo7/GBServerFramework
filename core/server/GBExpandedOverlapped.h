#pragma once

#include "../shared/GB"

#include "boost/lockfree/queue.hpp"
#include "winsock2.h"

#include <queue>

namespace GenericBoson
{
	struct GBExpandedOverlapped : public WSAOVERLAPPED
	{
		SOCKET m_socket = INVALID_SOCKET;
		IO_TYPE m_type = IO_TYPE::ACCEPT;

		VectoredIO m_gatherInput, m_scatterOutput;
	};
}