#include "stdafx.h"

#include "Client.h"
#include "../server/Util.h"
#include "../flatbufferschema/internal_generated.h"

#include <string_view>
#include <iostream>

namespace GenericBoson
{
	Client::~Client()
	{
		m_keepLooping.store(false);

		const auto result = closesocket(m_socket);
		if (result == SOCKET_ERROR)
		{
			// #ToDo logging error string
		}

		WSACleanup();
	}

	int Client::InitializeWinSock()
	{
		return WSAStartup(MAKEWORD(2, 2), &m_wsaData);
	}

	int Client::CreateSocket()
	{
		m_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (m_socket == INVALID_SOCKET) {
			int errorCode = WSAGetLastError();
			// # ToDo error logging
			WSACleanup();

			return errorCode;
		}

		return NO_ERROR;
	}

	int Client::ConnectInternal(const std::string_view address, const int port)
	{
		sockaddr_in addr;
		addr.sin_family = AF_INET;
		addr.sin_addr.s_addr = inet_addr(address.data());
		addr.sin_port = htons(port);

		if (connect(m_socket, (SOCKADDR*)&addr, sizeof(addr))
			== SOCKET_ERROR)
		{
			int errorCode = WSAGetLastError();
			if (closesocket(m_socket) == SOCKET_ERROR)
			{
				// #ToDo error logging
			}

			WSACleanup();
			return errorCode;
		}

		return NO_ERROR;
	}

	bool Client::GetKeepLooping()
	{
		timeval peekInterval;

		peekInterval.tv_sec = 0;
		peekInterval.tv_usec = 0;

		fd_set readsCopy = m_reads, writesCopy = m_writes;
		int retval = select(m_socket + 1, &readsCopy, &writesCopy, 0, &peekInterval);

		if (retval == -1)
		{
			int errorCode = WSAGetLastError();
			ErrorLog("select failed. error code - {}");
		}
		else if (retval)
		{
			// Data is available now.
			for (int k = 0; k < m_socket + 1; ++k)
			{
				if (FD_ISSET(k, &readsCopy))
				{
					int readBytes = recv(k, m_inputData.m_buffer, BUFFER_SIZE, 0);
					bool ret = OnReceived(m_inputData, readBytes);

					FD_CLR(m_socket, &readsCopy);
				}

				if (FD_ISSET(k, &writesCopy))
				{
					FD_CLR(m_socket, &writesCopy);
				}
			}
		}

		FD_ZERO(&m_reads);
		return m_keepLooping.load();
	}

	int Client::Connect(const std::string_view address, const int port)
	{
		FD_ZERO(&m_reads);
		FD_ZERO(&m_writes);

		int result = NO_ERROR;
		result = InitializeWinSock();
		if (result)
		{
			return result;
		}

		result = CreateSocket();
		if (result)
		{
			// #ToDo error logging
			return result;
		}

		result = ConnectInternal(address, port);
		if (result)
		{
			// #ToDo error logging
		}

		FD_SET(m_socket, &m_reads);
		FD_SET(m_socket, &m_writes);

		OnConnected();
		
		return 0;
	}

	void PongStub(const GenericBoson::GameInternal::PingPong& ping)
	{
		std::cout << "pong received : " << ping.num() << std::endl;
	}

	void Client::OnConnected()
	{
		AddStubInternal(ENGINE_RESERVED_PROTOCOL_NUMBER_RANGE_START, PongStub);
	}

	void Client::SendPing(ExpandedOverlapped& pEol)
	{

	}
}