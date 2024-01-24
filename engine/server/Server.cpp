#include "stdafx.h"

#include "Server.h"
#include "../Shared/HeartBeat.h"
#include "../flatbufferschema/internal_generated.h"

namespace GenericBoson
{
	std::string Server::Start()
	{
		bool result;
		std::string errorMsg;
		std::tie(result, errorMsg) = SetListeningSocket();

		m_sessions.resize(ISSUED_ACCEPTEX_COUNT);

		// AcceptEx 이슈
		for (int k = 0; k < ISSUED_ACCEPTEX_COUNT; ++k)
		{
			// AcceptEx 소켓만들기
			m_sessions[k].m_socket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, NULL, WSA_FLAG_OVERLAPPED);
			if (INVALID_SOCKET == m_sessions[k].m_socket)
			{
				return GetWSALastErrorString();
			}

			m_sessions[k].m_type = IO_TYPE::ACCEPT;

			// Posting an accept operation.
			DWORD returnedBytes;
			BOOL result = m_lpfnAcceptEx(m_listeningSocket, m_sessions[k].m_socket, m_listenBuffer, 0,
				sizeof(sockaddr_in) + 16, sizeof(sockaddr_in) + 16,
				&returnedBytes, &m_sessions[k]);
			int lastSocketError = WSAGetLastError();
			if (FALSE == result && ERROR_IO_PENDING != lastSocketError)
			{
				return GetWSALastErrorString(lastSocketError);
			}

			// Associate this accept socket withd IOCP.
			HANDLE associateAcceptSocketResult = CreateIoCompletionPort((HANDLE)m_sessions[k].m_socket, m_IOCP, (u_long)0, 0);
			if (NULL == associateAcceptSocketResult)
			{
				return GetWSALastErrorString();
			}
		}

		return {};
	}

	Server::~Server()
	{
		using namespace std::chrono_literals;

		// winsock2 종료 처리
		closesocket(m_listeningSocket);
		WSACleanup();

		m_keepLooping = false;

		for (int k = 0; k < m_threadPoolSize; ++k)
		{
			if (!m_threadPool[k].joinable())
			{
				continue;
			}

			m_threadPool[k].join();
		}

		m_sendTask.get();

		for (const auto& [_, itSendQueue] : m_sendQueues)
		{
			bool isEmpty = false;
			{
				std::scoped_lock lock(m_sendLock);
				isEmpty = itSendQueue.empty();
			}

			while (!isEmpty)
			{
				// preventing busy waiting
				std::this_thread::sleep_for(1ms);
			}
		}
	}

	std::pair<bool, std::string> Server::SetListeningSocket()
	{
#pragma region [1] Prepare and start listening port and IOCP
		// [1] - 1. WinSock 2.2 초기화
		if (NO_ERROR != WSAStartup(MAKEWORD(2, 2), &m_wsaData))
		{
			return { false, "WSAStartup failed\n" };
		}

		// [1] - 2.  IOCP 커널 오브젝트 만들기.
		m_IOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, (u_long)0, 0);
		if (NULL == m_IOCP)
		{
			return { false, GetWSALastErrorString() };
		}

		m_threadPoolSize = 2 * std::thread::hardware_concurrency() - 1; // 1 is send task
		for (int k = 0; k < m_threadPoolSize; ++k)
		{
			m_threadPool.emplace_back(&Server::ThreadFunction, this);
		}

		m_sendTask = boost::async(boost::bind(&Server::SendThreadFunction, this));

		// [1] - 3.  소켓 만들기
		m_listeningSocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, NULL, WSA_FLAG_OVERLAPPED);
		if (INVALID_SOCKET == m_listeningSocket)
		{
			return { false, GetWSALastErrorString() };
		}

		// [1] - 4.  Associate the listening socket with the IOCP.
		HANDLE ret1 = CreateIoCompletionPort((HANDLE)m_listeningSocket, m_IOCP, (u_long)0, 0);

		if (NULL == ret1)
		{
			return { false, GetWSALastErrorString() };
		}

		// [1] - 5.  소켓 설정
		m_addr.sin_family = AF_INET;
		m_addr.sin_port = htons(m_port);
		m_addr.sin_addr.S_un.S_addr = INADDR_ANY;

		// [1] - 6.  소켓 바인드
		int ret2 = bind(m_listeningSocket, (struct sockaddr*)&m_addr, sizeof(m_addr));
		if (SOCKET_ERROR == ret2)
		{
			return { false, GetWSALastErrorString() };
		}

		// [1] - 7.  리스닝 포트 가동
		ret2 = listen(m_listeningSocket, SOMAXCONN);
		if (SOCKET_ERROR == ret2)
		{
			return { false, GetWSALastErrorString() };
		}
#pragma endregion [1] Prepare and start listening port and IOCP

#pragma region [2] Prepare AcceptEx and associate accept I/O requests to IOCP
		GUID GuidAcceptEx = WSAID_ACCEPTEX;
		DWORD returnedBytes;

		// [2] - 1. AcceptEx 함수 가져오기
		ret2 = WSAIoctl(m_listeningSocket, SIO_GET_EXTENSION_FUNCTION_POINTER,
			&GuidAcceptEx, sizeof(GuidAcceptEx),
			&m_lpfnAcceptEx, sizeof(m_lpfnAcceptEx),
			&returnedBytes, NULL, NULL);
		if (SOCKET_ERROR == ret2)
		{
			return { false, GetWSALastErrorString() };
		}
#pragma endregion [2] Prepare AcceptEx and associate accept I/O requests to IOCP

		return { true, {} };
	}

	void Server::ThreadFunction()
	{
		DWORD transferredBytes;
		u_long completionKey;
		ExpandedOverlapped* pEol = nullptr;

		while (true == m_keepLooping)
		{
			BOOL result = GetQueuedCompletionStatus(m_IOCP, &transferredBytes, (PULONG_PTR)&completionKey, (OVERLAPPED**)&pEol, INFINITE);

			switch (pEol->m_type)
			{
			case IO_TYPE::ACCEPT:
			{
				OnConnected(pEol);

				int issueRecvResult = IssueRecv(pEol, BUFFER_SIZE);
				int lastError = WSAGetLastError();

				if (SOCKET_ERROR == issueRecvResult && WSA_IO_PENDING != lastError)
				{
					// #ToDo
					// Issue receiving failed.k
				}
			}
			break;
			case IO_TYPE::RECEIVE:
			{
				bool ret = OnReceived(pEol->m_inputData, transferredBytes);
				if (false == ret)
				{
					continue;
				}

				if (pEol->m_inputData.GetState() == VectoredIO::STATE::ID
					|| transferredBytes == 0)
				{
					int issueRecvResult = IssueRecv(pEol, pEol->m_inputData.m_length - pEol->m_inputData.m_offset);
					int lastError = WSAGetLastError();

					if (SOCKET_ERROR == issueRecvResult && WSA_IO_PENDING != lastError)
					{
						// #ToDo logging
						// Issue receiving failed.
					}
				}
			}
			break;
			case IO_TYPE::SEND:
			{
				bool ret = OnSent(pEol->m_outputData, transferredBytes);

				// 소켓 닫기
				//closesocket(pEol->m_socket);
			}
			break;
			}
		}
	}

	int Server::IssueRecv(ExpandedOverlapped* pEol, ULONG lengthToReceive)
	{
		pEol->m_type = IO_TYPE::RECEIVE;
		DWORD flag = 0;
		WSABUF wsaBuffer;
		wsaBuffer.len = lengthToReceive;
		wsaBuffer.buf = &pEol->m_inputData.m_buffer[pEol->m_inputData.m_offset];
		int recvResult = WSARecv(pEol->m_socket, &wsaBuffer, 1, nullptr, &flag, pEol, nullptr);

		return recvResult;
	}

	int Server::IssueSend(ExpandedOverlapped* pEol, const unsigned long throttling
	/* = std::numeric_limits<unsigned long>::max()*/)
	{
		WSABUF bufToSend;
		int sendResult = 0, totalSentBytes = 0;
		std::decay_t<decltype(throttling)> bytesToSend = pEol->m_outputData.m_offset;

		while(0 < bytesToSend)
		{
			bufToSend.buf = pEol->m_outputData.m_buffer + totalSentBytes;
			bufToSend.len = (std::min)(bytesToSend, throttling);

			DWORD sentBytes;
			sendResult = WSASend(pEol->m_socket, &bufToSend, 1, &sentBytes, 0, pEol, nullptr);
			bytesToSend -= sentBytes;
			totalSentBytes += sentBytes;

			if (sendResult)
			{
				break;
			}

#ifdef _DEBUG
			if(bufToSend.len == throttling)
			{
				// #test
				using namespace std::literals::chrono_literals;
				// prevent de-fragmenting for test
				std::this_thread::sleep_for(1s);
			}
		}
#endif // _DEBUG

		return sendResult;
	}

	void Server::SendThreadFunction()
	{
		using namespace std::chrono_literals;

		int focusIndex = 0;
		while (m_keepLooping)
		{
			for (auto& [_, itSendQueue] : m_sendQueues)
			{
				ExpandedOverlapped* pEol = nullptr;
				{
					std::scoped_lock lock(m_sendLock);

					if(!itSendQueue.empty())
					{
						pEol = itSendQueue.front();
						itSendQueue.pop();
					}
				}

				if (pEol)
				{
					IssueSend(pEol);
				}
			}

			std::this_thread::sleep_for(1ms);
		}
	}

	void PongStub(const GenericBoson::GameInternal::PingPong& ping)
	{
		std::cout << "pong received : " << ping.num() << std::endl;
	}

	void Server::OnConnected(ExpandedOverlapped* pEol)
	{
		AddStub(1, PongStub);

		const auto pTimer = std::make_shared<HeartBeat>(1000);
		TimerManager::GetInstance()->AddTimer(pTimer);

		if (m_connectedTask)
		{
			m_connectedTask(pEol);
		}
	}

	void Server::SetConnectedTask(
		const std::function<void(ExpandedOverlapped* pEol)>& task
	)
	{
		m_connectedTask = task;
	}

	void Server::SendPing(ExpandedOverlapped& pEol)
	{
		Send(&pEol, 1, [](auto& fbb) {
				return CreatePingPong(fbb, 777);
			});
	}

	ThreadSafeBufferAllocator Server::g_bufferAllocator;
}