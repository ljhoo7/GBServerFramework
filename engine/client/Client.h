#pragma once

#include "../Shared/Shared.h"

#include "winsock2.h"

#include <string_view>

namespace GenericBoson
{
	class Client : public Common
	{
	public:
		virtual ~Client();

		bool GetKeepLooping();

		int Connect(const std::string_view address, const int port);

		virtual void SendPing(ExpandedOverlapped& pEol) override;
	private:
		int InitializeWinSock();
		int CreateSocket();
		int ConnectInternal(const std::string_view address, const int port);

		void OnConnected();
	private:

		WSADATA m_wsaData;
		SOCKET m_socket;

		std::atomic_bool	m_keepLooping = true;

		VectoredIO			m_inputData;
		VectoredIO			m_outputData;

		fd_set				m_reads, m_writes;
	};
}