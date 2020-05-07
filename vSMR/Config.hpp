#pragma once
#include <fstream>
#include <sstream>
#include <iostream>
#include <map>
#include <vector>
#include <Gdiplus.h>

#include "bstrlib\bstrwrap.h"
#include "rapidjson/document.h"
#include "rapidjson/stringbuffer.h"
#include "Constant.hpp"

using namespace std;
using namespace rapidjson;

class CConfig
{
public:
	CConfig(CBString configPath);
	virtual ~CConfig();

	const Value& getActiveProfile();
	bool isSidColorAvail(CBString sid, const char* airport);
	Gdiplus::Color getSidColor(CBString sid, const char* airport);
	const Value& getAirportMapIfAny(const char* airport);
	bool isAirportMapAvail(CBString airport);
	bool isCustomRunwayAvail(const char* airport, CBString name1, CBString name2);

	Gdiplus::Color getConfigColor(const Value& config_path);
	COLORREF getConfigColorRef(const Value& config_path);

	vector<CBString> getAllProfiles();

	inline int isItActiveProfile(CBString toTest) {
		if (active_profile == profiles[toTest])
			return 1;
		return 0;
	};

	inline void setActiveProfile(CBString newProfile) {
		active_profile = profiles[newProfile];
	};

	inline CBString getActiveProfileName() {
		CBString name;
		for (std::map<CBString, rapidjson::SizeType>::iterator it = profiles.begin(); it != profiles.end(); ++it)
		{
			if (it->second == active_profile) {
				name = it->first;
				break;
			}
		}
		return name;
	};

	Document document;

protected:
	CBString config_path;
	rapidjson::SizeType active_profile;
	map<CBString, rapidjson::SizeType> profiles;

	void loadConfig();
};
