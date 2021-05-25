#include "../include/stdafx.h"
#include "../include/GBHttpRouter.h"

namespace GenericBoson
{
	bool GBHttpRouterBase::Route(PathSegment& rootPath, const std::string_view targetPath, const std::string_view methodName)
	{
		std::vector<std::string> pathSegmentArray;
		bool parseResult = ParseUrlString(targetPath, pathSegmentArray);

		if (false == parseResult)
		{
			// #ToDo
			// The path in request line does not start with '/'.
			return false;
		}

		// 원래 true == m_supportMethodMap.contains(methodName.data()) 였는데,
		// travis가 VS2017까지만 지원해서 아래와 같이 바꿈.
		if (m_supportMethodMap.end() == m_supportMethodMap.find(methodName.data()))
		{
			// #ToDo
			// Invalid request : The method name is not matched with the version.
			return false;
		}

		PathSegment* pNode = &rootPath;
		for (auto& iPathSegment : pathSegmentArray)
		{
			// 원래 true == pNode->m_subTreeMap.contains(iPathSegment) 였는데,
			// travis가 VS2017까지만 지원해서 아래와 같이 바꿈.
			if (pNode->m_subTreeMap.end() == pNode->m_subTreeMap.find(iPathSegment))
			{
				// #ToDo ActionMethod not found.
				return false;
			}

			pNode = pNode->m_subTreeMap[iPathSegment].get();
		}

		if ("GET" == methodName)
		{
			if (nullptr == pNode->m_pGetMethod)
			{
				// #ToDo
				// The method not exist.
				return false;
			}

			pNode->m_pGetMethod->m_method(0);

			return true;
		}
		else if ("HEAD" == methodName)
		{
			if (nullptr == pNode->m_pHeadMethod)
			{
				// #ToDo
				// The method not exist.
				return false;
			}

			pNode->m_pHeadMethod->m_method(0);

			return true;
		}
		else if ("POST" == methodName)
		{
			if (nullptr == pNode->m_pPostMethod)
			{
				// #ToDo
				// The method not exist.
				return false;
			}

			pNode->m_pPostMethod->m_method(0);

			return true;
		}

		// #ToDo
		// Internal logic error
		return false;
	}
}