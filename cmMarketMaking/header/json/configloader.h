#pragma once

#include <iostream>
#include <fstream>
//#include <boost\shared_ptr.hpp>
//#include <vector>
//#include <hash_map>
#include "json\json.h"
using namespace std;

inline Json::Value loadconfig(string path)
{
	ifstream ifs;
	Json::Reader reader;
	Json::Value root;
	try{
		ifs.open(path, ios::out | ios::in);
		reader.parse(ifs, root);
	}
	catch (exception& e)
	{
		throw(e);
	}
	return root;
}