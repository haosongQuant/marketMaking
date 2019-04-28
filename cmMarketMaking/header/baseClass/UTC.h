#pragma once
#include <string>

typedef unsigned long long uint64;
typedef long long int64;

namespace athenaUTC
{

	class UTC
		{
		public:
			//自1970年1月1日零点起至伦敦当前时间的微秒数(us)
			explicit UTC(int64 v);

			//UTC时间的字符串形式，格式为"YYYY-MM-DD HH:MM:SS.xxxxxx"
			explicit UTC(const std::string& str);

			//取当前UTC时间
			UTC(void);

			//重新获取当前时间
			UTC& Now(void);

			//UTC时间转字符串,格式为"YYYY-MM-DD HH:MM:SS.xxxxxx"
			std::string ToString(void);

			//UTC时间转北京时间的字符串,格式为"YYYY-MM-DD HH:MM:SS.xxxxxx"
			std::string ToBeiJing(void);

			//UTC时间转北京时间的字符串,格式为"YYYYMMDDHHMMSS"
			std::string ToBeiJing1(void);

			static int Init();
			static double GetMilliSecs();

		public:
			int64 m_Val;       //自1970年1月1日零点起至伦敦当前时间的微秒数(us)
		};

	}
