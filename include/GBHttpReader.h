#pragma once

#include <queue>

namespace GenericBoson
{
	struct GBHttpInformation
	{
		virtual ~GBHttpInformation() = default;
	};

	class GBHttpReader
	{
		// Read parsed tokens.
		// \return bool - succeeded? GBRequestLineInformation - The information of this request line.
		virtual bool Read(GBHttpInformation* pOutInfo) = 0;

		virtual void ParseToken() = 0;

	public:

		GBHttpReader(std::queue<std::string>& lines) {};
		virtual ~GBHttpReader() = default;

		// \GBRequestLineInformation - The information of this request line.
		// \return bool - succeeded? 
		virtual bool ParseAndRead(GBHttpInformation* pOutInfo) final;
	};
}