#pragma once
#include "EuroScopePlugIn.h"
#include "Mmsystem.h"

#include <chrono>
#include <thread>
#include <algorithm>

#include "HttpHelper.hpp"
#include "CPDLCSettingsDialog.hpp"
#include "DataLinkDialog.hpp"
#include "Constant.hpp"
#include "SMRRadar.hpp"
#include "Logger.h"


#define MY_PLUGIN_NAME      "vSMR"
#define MY_PLUGIN_VERSION   "1.4.4"
#define MY_PLUGIN_DEVELOPER "Pierre Ferran, Even Rognlien, Lionel Bischof, Daniel Lange, Juha Holopainen, Keanu Czirjak"
#define MY_PLUGIN_COPYRIGHT "GPL v3"
#define MY_PLUGIN_VIEW_AVISO  "SMR radar display"

using namespace std;
using namespace EuroScopePlugIn;

class CSMRPlugin :
	public EuroScopePlugIn::CPlugIn
{
public:
	CSMRPlugin();
	virtual ~CSMRPlugin();

	virtual bool OnCompileCommand(const char * sCommandLine);
	virtual void OnFunctionCall(int FunctionId, const char * sItemString, POINT Pt, RECT Area);

	virtual void OnGetTagItem(CFlightPlan FlightPlan, CRadarTarget RadarTarget, int ItemCode, int TagData, char sItemString[16], int * pColorCode, COLORREF * pRGB, double * pFontSize);

	virtual void OnAirportRunwayActivityChanged(void);
	virtual void OnControllerDisconnect(CController Controller);
	virtual void OnFlightPlanDisconnect(CFlightPlan FlightPlan);

	virtual void OnTimer(int Counter);

	virtual CRadarScreen * OnRadarScreenCreated(const char * sDisplayName, bool NeedRadarContent, bool GeoReferenced, bool CanBeSaved, bool CanBeCreated);
};

