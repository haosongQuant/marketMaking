#pragma once
#include <boost\asio.hpp>
#include <boost\thread.hpp>

class athenaUtils
{
public:
	static std::string&  Rtrim(std::string& s);    //删除字符串右端的空格
	static std::string&  Ltrim(std::string& s);    //删除字符串左端的空格
	static std::string&  Trim(std::string& s);     //删除字符串左右两端的空格
	static bool Equals(const double& d1, const double& d2);    //判断d1和d2是否近似相等
	static int  Compare(const double& d1, const double& d2);   //比较大小,-1 d1小于d2; 0 d1等于d2;+1 d1大于d2
	static bool Greater(const double& d1, const double& d2);
	static bool GreaterOrEqual(const double& d1, const double& d2);
	static bool Less(const double& d1, const double& d2);
	static bool LessOrEqual(const double& d1, const double& d2);
	static bool IsInvalid(const double& d);    //判断d为无效值 
	static double GetInvalidValue();     //无效值定义为double类型的最大值1.7976931348623158e+308
	static int  gcd(const int integer1, const int integer2); //计算最大公约数
	///@brief        以指定的分隔字符串劈分一个字符串         
	///@param[in]    src 待处理的字符串对象引用 
	///@param[in]    delimit    指定的分隔字符串
	///@param[in]    null_subst 当两个分隔字符串之间的内容为空，将用 @p null_subst 替代，
	///                       如果 @p null_subst 也是空，则忽略
	///@param[out] v 劈分的结果数组,vector, list, set
	///@return        劈分的结果数组的大小
	template <class resultType>
	static inline unsigned int  Split(const std::string& src, const std::string& delimit, resultType& v, const std::string& null_subst = "")
	{
		v.clear();

		if (src.empty() || delimit.empty())
			return 0;

		bool substIsNull = null_subst.empty() ? true : false;
		std::string::size_type deli_len = delimit.size();
		size_t index = std::string::npos, last_search_position = 0;
		while ((index = src.find(delimit, last_search_position)) != std::string::npos)
		{
			if (index == last_search_position)
			{
				if (!substIsNull)
					v.insert(v.end(), null_subst);
			}
			else
			{
				std::string temp = src.substr(last_search_position, index - last_search_position);
				Trim(temp);
				if (!temp.empty())
					v.insert(v.end(), temp);
				else if (!substIsNull)
					v.insert(v.end(), null_subst);
			}

			last_search_position = index + deli_len;
		}

		std::string last_one = src.substr(last_search_position);
		Trim(last_one);
		if (!last_one.empty())
			v.insert(v.end(), last_one);
		else if (!substIsNull)
			v.insert(v.end(), null_subst);

		return v.size();
	};
};

