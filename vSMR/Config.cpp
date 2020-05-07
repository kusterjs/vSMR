#include "stdafx.h"
#include "Config.hpp"
#include <algorithm>

CConfig::CConfig(CBString configPath)
{
	config_path = configPath;
	loadConfig();

	setActiveProfile("Default");
}

void CConfig::loadConfig() {

	stringstream ss;
	ifstream ifs;
	ifs.open(config_path, std::ios::binary);
	ss << ifs.rdbuf();
	ifs.close();

	if (document.Parse<0>(ss.str().c_str()).HasParseError()) {
		AfxMessageBox("An error parsing vSMR configuration occurred.\nOnce fixed, reload the config by typing '.smr reload'", MB_OK);
	
		ASSERT(AfxGetMainWnd() != NULL);
		AfxGetMainWnd()->SendMessage(WM_CLOSE);
	}
	
	profiles.clear();

	assert(document.IsArray());

	for (SizeType i = 0; i < document.Size(); i++) {
		const Value& profile = document[i];
		CBString profile_name = profile["name"].GetString();

		profiles.insert(pair<CBString, rapidjson::SizeType>(profile_name, i));
	}
}

const Value& CConfig::getActiveProfile() {
	return document[active_profile];
}

bool CConfig::isSidColorAvail(CBString sid, const char* airport) {
	if (getActiveProfile().HasMember("maps"))
	{
		if (getActiveProfile()["maps"].HasMember(airport))
		{
			if (getActiveProfile()["maps"][airport].HasMember("sids") && getActiveProfile()["maps"][airport]["sids"].IsArray())
			{
				const Value& SIDs = getActiveProfile()["maps"][airport]["sids"];
				for (SizeType i = 0; i < SIDs.Size(); i++)
				{
					const Value& SIDNames = SIDs[i]["names"];
					for (SizeType s = 0; s < SIDNames.Size(); s++) {
						CBString currentsid = SIDNames[s].GetString();
						
						if (sid.caselessEqual(currentsid))
						{
							return true;
						}
					}
				}
			}
		}
	}
	return false;
}

Gdiplus::Color CConfig::getSidColor(CBString sid, const char* airport)
{
	if (getActiveProfile().HasMember("maps"))
	{
		if (getActiveProfile()["maps"].HasMember(airport))
		{
			if (getActiveProfile()["maps"][airport].HasMember("sids") && getActiveProfile()["maps"][airport]["sids"].IsArray())
			{
				const Value& SIDs = getActiveProfile()["maps"][airport]["sids"];
				for (SizeType i = 0; i < SIDs.Size(); i++)
				{
					const Value& SIDNames = SIDs[i]["names"];
					for (SizeType s = 0; s < SIDNames.Size(); s++) {
						CBString currentsid = SIDNames[s].GetString();						
						if (sid.caselessEqual(currentsid))
						{
							return getConfigColor(SIDs[i]["color"]);
						}
					}
				}
			}
		}
	}
	return Gdiplus::Color(0, 0, 0);
}

Gdiplus::Color CConfig::getConfigColor(const Value& config_path) {
	int r = config_path["r"].GetInt();
	int g = config_path["g"].GetInt();
	int b = config_path["b"].GetInt();
	int a = 255;
	if (config_path.HasMember("a"))
		a = config_path["a"].GetInt();

	Gdiplus::Color Color(a, r, g, b);
	return Color;
}

COLORREF CConfig::getConfigColorRef(const Value& config_path) {
	int r = config_path["r"].GetInt();
	int g = config_path["g"].GetInt();
	int b = config_path["b"].GetInt();

	COLORREF Color(RGB(r, g, b));
	return Color;
}

const Value& CConfig::getAirportMapIfAny(const char* airport) {
	if (getActiveProfile().HasMember("maps")) {
		const Value& map_data = getActiveProfile()["maps"];
		if (map_data.HasMember(airport)) {
			const Value& airport_map = map_data[airport];
			return airport_map;
		}
	}
	return getActiveProfile();
}

bool CConfig::isAirportMapAvail(CBString airport) {
	if (getActiveProfile().HasMember("maps")) {
		if (getActiveProfile()["maps"].HasMember(airport)) {
			return true;
		}
	}
	return false;
}


bool CConfig::isCustomRunwayAvail(const char* airport, CBString name1, CBString name2) {
	if (getActiveProfile().HasMember("maps")) {
		if (getActiveProfile()["maps"].HasMember(airport)) {
			if (getActiveProfile()["maps"][airport].HasMember("runways") 
				&& getActiveProfile()["maps"][airport]["runways"].IsArray()) {
				const Value& Runways = getActiveProfile()["maps"][airport]["runways"];
				for (SizeType i = 0; i < Runways.Size(); i++) {
					if (StartsWith(name1, Runways[i]["runway_name"].GetString()) ||
						StartsWith(name2, Runways[i]["runway_name"].GetString())) {
						return true;
					}
				}
			}
		}
	}
	return false;
}

vector<CBString> CConfig::getAllProfiles() {
	vector<CBString> toR;
	for (std::map<CBString, rapidjson::SizeType>::iterator it = profiles.begin(); it != profiles.end(); ++it)
	{
		toR.push_back(it->first);
	}
	return toR;
}

CConfig::~CConfig()
{
}