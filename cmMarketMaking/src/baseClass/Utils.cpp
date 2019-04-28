// This is the main DLL file.
#pragma once

#include "baseClass/Utils.h"
#include <limits>

std::string& athenaUtils::Rtrim(std::string& s)
	{
		std::string::size_type index = s.size();
		for (; index >= 1; --index)
			if (s[index - 1] != ' ')
				break;

		if (index != s.size())
			s.erase(index);
		return s;
	}

	std::string& athenaUtils::Ltrim(std::string& s)
	{
		std::string::size_type index = 0;
		for (; index < s.size(); ++index)
			if (s[index] != ' ')
				break;

		if (index != 0)
			s.erase(0, index);

		return s;
	}

	std::string& athenaUtils::Trim(std::string& s)
	{
		return Ltrim(Rtrim(s));
	}


	static int GetExpoBase2(const double& d)
	{
		int i = 0;
		((short *)(&i))[0] = (((short *)(&d))[3] & (short)0x7ff0);
		return (i >> 4) - 1023;
	}

	bool athenaUtils::Equals(const double& d1, const double& d2)
	{
		if (d1 == d2)
			return true;
		int e1 = GetExpoBase2(d1);
		int e2 = GetExpoBase2(d2);
		double tmp = d1 - d2;
		int e3 = GetExpoBase2(tmp);
		if ((e3 - e2 < -48) && (e3 - e1 < -48))
			return true;
		return false;
	}

	int athenaUtils::Compare(const double& d1, const double& d2)
	{
		if (Equals(d1, d2) == true)
			return 0;
		if (d1 > d2)
			return 1;
		return -1;
	}

	bool athenaUtils::Greater(const double& d1, const double& d2)
	{
		if (Equals(d1, d2) == true)
			return false;
		if (d1 > d2)
			return true;
		return false;
	}

	bool athenaUtils::GreaterOrEqual(const double& d1, const double& d2)
	{
		if (Equals(d1, d2))
			return true;
		if (d1 > d2)
			return true;
		return false;
	}

	bool athenaUtils::Less(const double& d1, const double& d2)
	{
		if (Equals(d1, d2))
			return false;
		if (d1 < d2)
			return true;
		return false;
	}

	bool athenaUtils::LessOrEqual(const double& d1, const double& d2)
	{
		if (Equals(d1, d2) == true)
			return true;
		if (d1 < d2)
			return true;
		return false;
	}

	bool athenaUtils::IsInvalid(const double& d)
	{
		double tmp = std::numeric_limits<double>::max();
		return Equals(d, tmp);
		//return Equals(d, 1.7976931348623158e+308);
	}

	double athenaUtils::GetInvalidValue()
	{
		return std::numeric_limits<double>::max();
	}


	int athenaUtils::gcd(const int integer1, const int integer2)
	{
		int a = integer1;
		int b = integer2;
		do {
			if (a > b) a = a % b;
			else if (a < b) b = b % a;
			else if (a == b) break;
		} while (a != 0 && b != 0);

		return ((a == 0) ? b : a);
	}