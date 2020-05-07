#pragma once
#include <EuroScopePlugIn.h>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <time.h>
#include <GdiPlus.h>
#define _USE_MATH_DEFINES
#include <math.h>
#include "Constant.hpp"
#include "CallsignLookup.hpp"
#include "Config.hpp"
#include "Rimcas.hpp"
#include "InsetWindow.h"
#include <memory>
#include <asio/io_service.hpp>
#include <thread>
#include "ColorManager.h"
#include "Logger.h"

using namespace std;
using namespace Gdiplus;
using namespace EuroScopePlugIn;


//namespace SMRSharedData {
	//static vector<string> ReleasedTracks;	
//};

#define PATATOIDES_NUM_POINTS 64
#define ACT_TYPE_EMPTY_SPACES "      "
#define GATE_EMPTY_SPACES "           "
#define SCRATCHPAD_EMPTY_SPACES "                          "

namespace SMRPluginSharedData {
	static asio::io_service io_service;
}

//using namespace SMRSharedData;

class CSMRRadar :
	public EuroScopePlugIn::CRadarScreen
{
public:
	CSMRRadar();
	virtual ~CSMRRadar();

	static set<string> manuallyCorrelated;
	static map<string, string> vStripsStands;

	bool BLINK = false;

	map<string, POINT> TagsOffsets;

	//vector<string> Active_Arrivals;

	clock_t clock_init, clock_final;

	COLORREF SMR_TARGET_COLOR = RGB(255, 242, 73);
	COLORREF SMR_H1_COLOR = RGB(0, 255, 255);
	COLORREF SMR_H2_COLOR = RGB(0, 219, 219);
	COLORREF SMR_H3_COLOR = RGB(0, 183, 183);

	struct TagItem
	{
		string value;
		int function;
	};

	typedef struct tagPOINT2
	{
		double x;
		double y;
	} POINT2;

	/*
	struct Patatoide_Points
	{
		POINT2 points[PATATOIDES_NUM_POINTS];
		POINT2 History_one_points[PATATOIDES_NUM_POINTS];
		POINT2 History_two_points[PATATOIDES_NUM_POINTS];
		POINT2 History_three_points[PATATOIDES_NUM_POINTS];
	};
	*/

	struct Patatoide_Points
	{
		map<int, POINT2> points;
		map<int, POINT2> History_one_points;
		map<int, POINT2> History_two_points;
		map<int, POINT2> History_three_points;
	};

	map<const char *, Patatoide_Points> Patatoides;

	map<string, bool> ClosedRunway;

	char DllPathFile[_MAX_PATH];
	string DllPath;
	string ConfigPath;
	CCallsignLookup * Callsigns;
	CColorManager * ColorManager;

	map<string, bool> ShowLists;
	map<string, RECT> ListAreas;

	map<int, bool> appWindowDisplays;

	set<string> tagDetailed;
	map<string, CRect> tagAreas;
	map<string, double> TagAngles;
	map<string, int> TagLeaderLineLength;

	bool QDMenabled = false;
	bool QDMSelectEnabled = false;
	POINT QDMSelectPt;
	POINT QDMmousePt;

	bool ColorSettingsDay = true;
	bool isLVP = false;

	map<string, RECT> TimePopupAreas;

	map<int, string> TimePopupData;
	//multimap<string, string> AcOnRunway;
	map<string, bool> ColorAC;

	//map<string, CRimcas::RunwayAreaType> RunwayAreas;

	map<string, RECT> MenuPositions;
	map<string, bool> DisplayMenu;

	map<string, clock_t> RecentlyAutoMovedTags;

	CRimcas * RimcasInstance = nullptr;
	CConfig * CurrentConfig = nullptr;

	Gdiplus::Font* customFont;
	int currentFontSize = 15;

	map<string, CPosition> AirportPositions;

	bool isProMode = false;
	bool useAutoDeconfliction = false;
	bool Afterglow = true;

	int Trail_Gnd = 4;
	int Trail_App = 4;
	int PredictedLenght = 0;

	bool NeedCorrelateCursor = false;
	bool ReleaseInProgress = false;
	bool AcquireInProgress = false;

	multimap<string, string> DistanceTools;
	bool DistanceToolActive = false;
	pair<string, string> ActiveDistance;

	//----
	// Tag types
	//---

	enum TagTypes { Departure, Arrival, Airborne, Uncorrelated };

	string ActiveAirport = "LSZH";


	//---GenerateTagData--------------------------------------------

	static map<string, TagItem> GenerateTagData(CRadarTarget rt, CFlightPlan fp, CSMRRadar* radar, string ActiveAirport);

	//---IsCorrelatedFuncs---------------------------------------------

	bool IsCorrelated(CFlightPlan fp, CRadarTarget rt)
	{		
		if (isProMode) {
			if (fp.IsValid() && fp.GetFlightPlanData().IsReceived()) {				
				if (strcmp(fp.GetControllerAssignedData().GetSquawk(), rt.GetPosition().GetSquawk()) == 0) {
					return true;
				}

				/*if (CurrentConfig->getActiveProfile()["filters"]["pro_mode"]["accept_pilot_squawk"].GetBool()) {
					return true;
				}*/

				ASSERT(strlen(rt.GetPosition().GetSquawk()) == 4);
				if (strcmp(rt.GetPosition().GetSquawk(), "1000") == 0) { // squawk 1000
					return true;
				}

				if (strcmp(rt.GetPosition().GetSquawk() - 2, "00") == 0) { // are the last 2 chars of the squawk 00
					return false;
				}

				/*const Value& sqs = CurrentConfig->getActiveProfile()["filters"]["pro_mode"]["do_not_autocorrelate_squawks"];
				for (SizeType i = 0; i < sqs.Size(); i++) {
					if (strcmp(rt.GetPosition().GetSquawk(), sqs[i].GetString()) == 0) {
						isCorr = false;
						break;
					}
				}*/
				

				if (manuallyCorrelated.count(rt.GetSystemID()) > 0) {
					return true;
				}

				/*if (std::find(ReleasedTracks.begin(), ReleasedTracks.end(), rt.GetSystemID()) != ReleasedTracks.end()) {
					return false;
				}*/
			}

			return false;
		}
		else {
			// If the pro mode is not used, then the AC is always correlated
			return true;
		}
	};

	static string GetStandNumber(CFlightPlan fp)
	{
		if (!fp.IsValid())
			return "";

		const char* remarks = fp.GetFlightPlanData().GetRemarks();
		const char* pGate = strstr(remarks, " STAND/");
		if (pGate != nullptr) {
			auto pEnd = strpbrk(pGate, " \r\n\0");
			char stand[16];
			strncpy_s(stand, pGate + 7, pEnd - (pGate + 7));
			return stand;
		}
		return "";
	}

	static void SetStandNumber(CFlightPlan fp, string stand)
	{
		if (!fp.IsValid())
			return;

		string remarks = fp.GetFlightPlanData().GetRemarks();

		size_t pos1 = remarks.find(" STAND/");
		if (pos1 < remarks.length()) { // contains a stand already
			size_t pos2 = remarks.find_first_of(" \r\n\0", pos1+1);

			if (stand == "") { // remove it
				remarks.erase(pos1, pos2 - pos1 + 1);
			}
			else { // update it
				remarks.replace(pos1, pos2 - pos1 + 1, " STAND/" + stand);
			}
		}

		else { // ne entry -> add it
			remarks += " STAND/" + stand;
		}

		fp.GetFlightPlanData().SetRemarks(remarks.c_str());
		fp.GetFlightPlanData().AmendFlightPlan();
	}

	bool IsAcOnRunway(CRadarTarget Ac)
	{
		int AltitudeDif = Ac.GetPosition().GetFlightLevel() - Ac.GetPreviousPosition(Ac.GetPosition()).GetFlightLevel();
		if (!Ac.GetPosition().GetTransponderC())
			AltitudeDif = 0;

		if (Ac.GetGS() > 160 || AltitudeDif > 200)
			return false;

		POINT AcPosPix = ConvertCoordFromPositionToPixel(Ac.GetPosition().GetPosition());

		for (const auto &rwy : RimcasInstance->Runways) {
			if (!rwy.monitor_dep && !rwy.monitor_arr)
				continue;

			vector<POINT> RunwayOnScreen;
			for (const auto &Point : rwy.rimcas_path) {
				RunwayOnScreen.push_back(ConvertCoordFromPositionToPixel(Point));
			}

			if (Is_Inside(AcPosPix, RunwayOnScreen)) {
				return true;
			}
		}

		return false;
	}


	void SMRSetCursor(HCURSOR targetCursor);

	void CorrelateCursor();
	void LoadCustomFont();
	void LoadProfile(string profileName);

	virtual void OnAsrContentLoaded(bool Loaded);
	virtual void OnAsrContentToBeSaved();

	virtual void OnRefresh(HDC hDC, int Phase);

	virtual void OnClickScreenObject(int ObjectType, const char * sObjectId, POINT Pt, RECT Area, int Button);
	virtual void OnDoubleClickScreenObject(int ObjectType, const char * sObjectId, POINT Pt, RECT Area, int Button);
	virtual void OnMoveScreenObject(int ObjectType, const char * sObjectId, POINT Pt, RECT Area, bool Released);
	virtual void OnOverScreenObject(int ObjectType, const char * sObjectId, POINT Pt, RECT Area);

	virtual bool OnCompileCommand(const char * sCommandLine);
	
	virtual void OnRadarTargetPositionUpdate(CRadarTarget RadarTarget);
	virtual void OnFlightPlanDisconnect(CFlightPlan FlightPlan);

	bool isVisible(CRadarTarget rt)
	{
		CRadarTargetPositionData RtPos = rt.GetPosition();
		int radarRange = CurrentConfig->getActiveProfile()["filters"]["radar_range_nm"].GetInt();
		int altitudeFilter = CurrentConfig->getActiveProfile()["filters"]["hide_above_alt"].GetInt();
		int speedFilter = CurrentConfig->getActiveProfile()["filters"]["hide_above_spd"].GetInt();
		bool isAcDisplayed = true;

		if (AirportPositions[ActiveAirport].DistanceTo(RtPos.GetPosition()) > radarRange)
			isAcDisplayed = false;

		if (altitudeFilter != 0) {
			if (RtPos.GetPressureAltitude() > altitudeFilter)
				isAcDisplayed = false;
		}

		if (speedFilter != 0) {
			if (RtPos.GetReportedGS() > speedFilter)
				isAcDisplayed = false;
		}

		return isAcDisplayed;
	}

	//---Haversine---------------------------------------------
	// Heading in deg, distance in m
	const double PI = (double)M_PI;

	inline CPosition Haversine(CPosition origin, double heading, double distance)
	{

		CPosition newPos;

		double d = (distance*0.00053996) / 60 * PI / 180;
		double trk = DegToRad(heading);
		double lat0 = DegToRad(origin.m_Latitude);
		double lon0 = DegToRad(origin.m_Longitude);

		double lat = asin(sin(lat0) * cos(d) + cos(lat0) * sin(d) * cos(trk));
		double lon = cos(lat) == 0 ? lon0 : fmod(lon0 + asin(sin(trk) * sin(d) / cos(lat)) + PI, 2 * PI) - PI;

		newPos.m_Latitude = RadToDeg(lat);
		newPos.m_Longitude = RadToDeg(lon);

		return newPos;
	}

	inline float randomizeHeading(float originHead)
	{
		return float(fmod(originHead + float((rand() % 5) - 2), 360));
	}

	//---GetBottomLine---------------------------------------------

	virtual string GetBottomLine(const char * Callsign);

	void ReloadActiveRunways();

	//---OnFunctionCall-------------------------------------------------

	virtual void OnFunctionCall(int FunctionId, const char * sItemString, POINT Pt, RECT Area);

	//---OnAsrContentToBeClosed-----------------------------------------

	void CSMRRadar::EuroScopePlugInExitCustom();

	//  This gets called before OnAsrContentToBeSaved()
	// -> we can't delete CurrentConfig just yet otherwise we can't save the active profile
	inline virtual void OnAsrContentToBeClosed(void)
	{
		delete RimcasInstance;
		//delete CurrentConfig;
		delete this;
	};
};
