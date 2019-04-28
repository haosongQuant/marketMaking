#include "baseClass/UTC.h"

#include "boost/date_time/posix_time/posix_time.hpp" 
#include "boost/date_time/local_time_adjustor.hpp"
#include "boost/date_time/c_local_time_adjustor.hpp"

#ifdef WIN32
#include <windows.h>
#endif

using namespace boost::posix_time;

#define ONE_SECOND_IN_MICROSECONDS 1000000

//匿名空间
namespace
{

	//输入是本地时间,返回值为UTC时间
	static int64 PtimeToTimeStamp(const ptime& pt)
	{
		tm t1 = to_tm(pt.date());
		time_t t2 = mktime(&t1);//本地时间转为utc时间
		int64 ms = pt.time_of_day().total_microseconds();
		return	int64(t2) * ONE_SECOND_IN_MICROSECONDS + ms;
	}

}

namespace athenaUTC{

	UTC::UTC(int64 v) :m_Val(v)
	{
	}

	UTC::UTC(const std::string& str)
	{
		ptime pt;
		try
		{
			pt = time_from_string(str);
		}
		catch (...)
		{
			m_Val = 0;
			return;
		}

		typedef boost::date_time::c_local_adjustor<ptime> local_adj;
		pt = local_adj::utc_to_local(pt);
		m_Val = PtimeToTimeStamp(pt);
	}

	UTC::UTC(void)
	{
		m_Val = PtimeToTimeStamp(microsec_clock::local_time());
	}

	UTC& UTC::Now(void)
	{
		m_Val = PtimeToTimeStamp(microsec_clock::local_time());
		return *this;
	}


	std::string UTC::ToString()
	{
		ptime pt = from_time_t(m_Val / 1000000);
		pt += microseconds(m_Val % 1000000);

		char result[32];

		tm tm = boost::posix_time::to_tm(pt);
		time_duration dur = pt - boost::posix_time::ptime_from_tm(tm);
#ifdef WIN32
		_snprintf_s
#else
		snprintf
#endif
			(result, sizeof(result), "%04d-%02d-%02d %02d:%02d:%02d.%06lld",
			tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour,
			tm.tm_min, tm.tm_sec, (long long)dur.total_microseconds());
		result[26] = 0;
		return result;
	}

	std::string UTC::ToBeiJing()
	{
		int64 tmpVal = m_Val + (((int64)8) * 3600 * 1000000);
		ptime pt = from_time_t(tmpVal / 1000000);
		pt += microseconds(tmpVal % 1000000);

		char result[32];

		tm tm = boost::posix_time::to_tm(pt);
		time_duration dur = pt - boost::posix_time::ptime_from_tm(tm);
#ifdef WIN32
		_snprintf_s
#else
		snprintf
#endif
			(result, sizeof(result), "%04d-%02d-%02d %02d:%02d:%02d.%06lld",
			tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour,
			tm.tm_min, tm.tm_sec, (long long)dur.total_microseconds());
		result[26] = 0;
		return result;
	}

	std::string UTC::ToBeiJing1()
	{
		int64 tmpVal = m_Val + (((int64)8) * 3600 * 1000000);
		ptime pt = from_time_t(tmpVal / 1000000);
		pt += microseconds(tmpVal % 1000000);

		char result[32];

		tm tm = boost::posix_time::to_tm(pt);
		time_duration dur = pt - boost::posix_time::ptime_from_tm(tm);
#ifdef WIN32
		_snprintf_s
#else
		snprintf
#endif
			(result, sizeof(result), "%04d%02d%02d%02d%02d%02d",
			tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour,
			tm.tm_min, tm.tm_sec);
		result[14] = 0;
		return result;
	}

#ifdef WIN32
	static LARGE_INTEGER gPerfFreq = { 0 };
#else

#endif

	int UTC::Init()
	{
#ifdef WIN32
		QueryPerformanceFrequency(&gPerfFreq);
#endif
		return 0;
	}

	double UTC::GetMilliSecs()
	{
#ifdef WIN32
		LARGE_INTEGER curTime;
		QueryPerformanceCounter(&curTime);
		return (((double)(curTime.QuadPart)) / gPerfFreq.QuadPart)*1000;
#else     
		return (double)microsec_clock::local_time().time_of_day().total_nanoseconds();
#endif
	}

}
