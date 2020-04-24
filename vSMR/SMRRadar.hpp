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


namespace SMRSharedData {
	//static vector<string> ReleasedTracks;
	static vector<string> ManuallyCorrelated;
};


namespace SMRPluginSharedData {
	static asio::io_service io_service;
}

using namespace SMRSharedData;

class CSMRRadar :
	public EuroScopePlugIn::CRadarScreen
{
public:
	CSMRRadar();
	virtual ~CSMRRadar();

	static map<string, string> vStripsStands;

	bool BLINK = false;

	map<string, POINT> TagsOffsets;

	//vector<string> Active_Arrivals;

	clock_t clock_init, clock_final;

	COLORREF SMR_TARGET_COLOR = RGB(255, 242, 73);
	COLORREF SMR_H1_COLOR = RGB(0, 255, 255);
	COLORREF SMR_H2_COLOR = RGB(0, 219, 219);
	COLORREF SMR_H3_COLOR = RGB(0, 183, 183);

	typedef struct tagPOINT2
	{
		double x;
		double y;
	} POINT2;

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
	multimap<string, string> AcOnRunway;
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

	static map<string, string> GenerateTagData(CRadarTarget rt, CFlightPlan fp, CSMRRadar* radar, string ActiveAirport);

	//---IsCorrelatedFuncs---------------------------------------------

	inline virtual bool IsCorrelated(CFlightPlan fp, CRadarTarget rt)
	{
		if (CurrentConfig->getActiveProfile()["filters"]["pro_mode"]["enable"].GetBool()) {
			if (fp.IsValid() && fp.GetFlightPlanData().IsReceived()) {				
				if (strcmp(fp.GetControllerAssignedData().GetSquawk(), rt.GetPosition().GetSquawk()) == 0) {
					return true;
				}

				if (CurrentConfig->getActiveProfile()["filters"]["pro_mode"]["accept_pilot_squawk"].GetBool()) {
					return true;
				}

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
				

				if (std::find(ManuallyCorrelated.begin(), ManuallyCorrelated.end(), rt.GetSystemID()) != ManuallyCorrelated.end()) {
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

	void SMRSetCursor(HCURSOR targetCursor);

	virtual void CorrelateCursor();
	virtual void LoadCustomFont();
	virtual void LoadProfile(string profileName);

	virtual void OnAsrContentLoaded(bool Loaded);
	virtual void OnAsrContentToBeSaved();

	virtual void OnRefresh(HDC hDC, int Phase);

	virtual void OnClickScreenObject(int ObjectType, const char * sObjectId, POINT Pt, RECT Area, int Button);
	virtual void OnMoveScreenObject(int ObjectType, const char * sObjectId, POINT Pt, RECT Area, bool Released);
	virtual void OnOverScreenObject(int ObjectType, const char * sObjectId, POINT Pt, RECT Area);

	virtual bool OnCompileCommand(const char * sCommandLine);
	
	virtual void OnRadarTargetPositionUpdate(CRadarTarget RadarTarget);
	virtual void OnFlightPlanDisconnect(CFlightPlan FlightPlan);

	virtual bool isVisible(CRadarTarget rt)
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

	inline virtual CPosition Haversine(CPosition origin, double heading, double distance)
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

	inline virtual float randomizeHeading(float originHead)
	{
		return float(fmod(originHead + float((rand() % 5) - 2), 360));
	}

	//---GetBottomLine---------------------------------------------

	virtual string GetBottomLine(const char * Callsign);

	//---LineIntersect---------------------------------------------

	/*inline virtual POINT getIntersectionPoint(POINT lineA, POINT lineB, POINT lineC, POINT lineD) {

		double x1 = lineA.x;
		double y1 = lineA.y;
		double x2 = lineB.x;
		double y2 = lineB.y;

		double x3 = lineC.x;
		double y3 = lineC.y;
		double x4 = lineD.x;
		double y4 = lineD.y;

		POINT p = { 0, 0 };

		double d = (x1 - x2) * (y3 - y4) - (y1 - y2) * (x3 - x4);
		if (d != 0) {
			double xi = ((x3 - x4) * (x1 * y2 - y1 * x2) - (x1 - x2) * (x3 * y4 - y3 * x4)) / d;
			double yi = ((y3 - y4) * (x1 * y2 - y1 * x2) - (y1 - y2) * (x3 * y4 - y3 * x4)) / d;

			p = { (int)xi, (int)yi };

		}
		return p;
	}*/

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
