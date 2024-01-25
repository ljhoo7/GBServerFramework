#pragma once

#include "StubAdaptor.h"
#include "Constant.h"

#include <string_view>
#include <format>
#include <cassert>
#include <unordered_map>

namespace GenericBoson
{
	struct ExpandedOverlapped;

	typedef size_t BUFFER_SIZE_TYPE;
	constexpr BUFFER_SIZE_TYPE BUFFER_SIZE = 8 * 1024 + 200;

	enum class IO_TYPE : int32_t
	{
		ACCEPT = 1,
		RECEIVE,
		SEND,
	};

	class VectoredIO
	{
	public:
		enum STATE : char
		{
			ID = 0,
			LENGTH,
			PAYLOAD,
			MAX_STATE
		};
	public:
		void AdvanceState()
		{
			m_state = STATE{ (char{ m_state } + 1) % STATE::MAX_STATE };
		}

		STATE GetState() const
		{
			return m_state;
		}

	public:
		char m_buffer[BUFFER_SIZE] = { 1, };

		int32_t m_messageID = 0;
		int32_t m_length = 0, m_offset = 0;

	private:
		STATE m_state = STATE::ID;
	};

	class Common
	{
	public:
		bool ErrorLog(const std::string_view msg);
		bool WarningLog(const std::string_view msg);
		bool InfoLog(const std::string_view msg);
	
		bool ReadWholePartialMessages(VectoredIO& inputData, const unsigned long transferredBytes);
		bool OnReceived(VectoredIO& inputData, const unsigned long transferredBytes);
		bool OnSent(VectoredIO& outputData, const unsigned long transferredBytes);

		template<typename FLATBUFFER_TABLE>
		bool AddStub(const int messageID, void(*Stub)(const FLATBUFFER_TABLE& table))
		{
			assert(ENGINE_RESERVED_PROTOCOL_NUMBER_RANGE_END < messageID 
				|| messageID < ENGINE_RESERVED_PROTOCOL_NUMBER_RANGE_START);

			return AddStubInternal(messageID, Stub);
		}

		virtual void SendPing(ExpandedOverlapped& pEol) = 0;
	protected:
		template<typename FLATBUFFER_TABLE>
		bool AddStubInternal(const int messageID, void(*Stub)(const FLATBUFFER_TABLE& table))
		{
			const auto [_, isInserted] = m_stubs.emplace(messageID,
				std::make_shared<StubAdaptor<FLATBUFFER_TABLE>>(Stub));

			if (!isInserted)
			{
				throw std::format("Add stub failed. Message ID - {}", messageID);
			}
		}
	protected:
		std::unordered_map<int, std::shared_ptr<IStubAdaptor>> m_stubs;

		bool Gather(VectoredIO& vectoredIO, const unsigned long transferredBytes);
		bool OnGatheringCompleted(VectoredIO& inputData);
	};
}