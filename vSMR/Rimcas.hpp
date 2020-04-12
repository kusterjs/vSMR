#pragma once
#include <EuroScopePlugIn.h>
#include <iostream>
#include <vector>
#include <set>
#include <queue>
#include <map>
#include <string>
#include <algorithm>
#define _USE_MATH_DEFINES
#include <math.h>
#include "Constant.hpp"
#include <functional>
#include "Logger.h"

class CSMRRadar;
using namespace std;
using namespace Gdiplus;
using namespace EuroScopePlugIn;


class CRimcas {
public:
	CRimcas();
	virtual ~CRimcas();

	const string string_false = "!NO";

	/*struct RunwayAreaType {
		string Name = "";
		vector<CPosition> Definition;
		bool set = false;
	};*/


	/*struct RunwaySide
	{
		string number;
		bool monitor_dep;
		bool monitor_arr;
	};*/

	struct Runway {		
		string name;
		bool closed = false;
		string rwyInUse; // which axis is in use
		bool monitor_dep;
		bool monitor_arr;
		vector<CPosition> path;
		vector<CPosition> rimcas_path;
		vector<CPosition> lvp_path;		
	};

	struct IAW_Aircraft {
		string callsign;
		double time;
		double distance;
		pair<COLORREF, COLORREF> colors;
		bool operator<(const IAW_Aircraft& p) const
		{
			return time < p.time;
		}
	};
	
	deque<pair<COLORREF, COLORREF>> IAWColors = {
		{ RGB(84, 122, 44),    RGB(97, 144, 49) },
		{ RGB(104, 122, 90), RGB(123, 144, 108) },
		{ RGB(84, 161, 44), RGB(97, 192, 49) },
		{ RGB(104, 161, 90), RGB(123, 192, 108) },
		{ RGB(84, 200, 44), RGB(97, 232, 49) },
		{ RGB(104, 200, 90), RGB(123, 232, 108) }
	};

	COLORREF WarningColor = RGB(160, 90, 30); //RGB(180, 100, 50)
	COLORREF AlertColor = RGB(150, 0, 0);

	enum RimcasAlertTypes { 
		NoAlert, 
		StageOne, 
		StageTwo 
	};

	//map<string, RunwayAreaType> RunwayAreas;
	multimap<string, string> AcOnRunway;
	vector<int> CountdownDefinition;
	vector<int> CountdownDefinitionLVP;
	multimap<string, string> ApproachingAircrafts;
	map<string, map<int, string>> _TimeTable;
	map<string, set<IAW_Aircraft>> IAWQueue;
	map<string, pair<COLORREF, COLORREF>> IAWQueueColors; // maps callsign to IAW color, I don't have any better idea so far...
	vector<Runway> Runways;
	//map<string, bool> MonitoredRunwayDep;
	//map<string, bool> MonitoredRunwayArr;
	map<string, RimcasAlertTypes> AcColor;

	bool IsLVP = false;

	int Is_Left(const POINT &p0, const POINT &p1, const POINT &point)
	{
		return ((p1.x - p0.x) * (point.y - p0.y) -
			(point.x - p0.x) * (p1.y - p0.y));
	}

	inline double NauticalMilesToMeters(double nm) {
		return nm * 1852;
	}

	bool Is_Inside(const POINT &point, const std::vector<POINT> &points_list)
	{
		// The winding number counter.
		int winding_number = 0;

		// Loop through all edges of the polygon.
		typedef std::vector<POINT>::size_type size_type;

		size_type size = points_list.size();

		for (size_type i = 0; i < size; ++i)             // Edge from point1 to points_list[i+1]
		{
			POINT point1(points_list[i]);
			POINT point2;

			// Wrap?
			if (i == (size - 1))
			{
				point2 = points_list[0];
			}
			else
			{
				point2 = points_list[i + 1];
			}

			if (point1.y <= point.y)                                    // start y <= point.y
			{
				if (point2.y > point.y)                                 // An upward crossing
				{
					if (Is_Left(point1, point2, point) > 0)             // Point left of edge
					{
						++winding_number;                               // Have a valid up intersect
					}
				}
			}
			else
			{
				// start y > point.y (no test needed)
				if (point2.y <= point.y)                                // A downward crossing
				{
					if (Is_Left(point1, point2, point) < 0)             // Point right of edge
					{
						--winding_number;                               // Have a valid down intersect
					}
				}
			}
		}

		return (winding_number != 0);
	}

	void GetAcInRunwayArea(CRadarTarget Ac, CRadarScreen *instance);
	//string _GetAcInRunwayAreaSoon(CRadarTarget Ac, CRadarScreen *instance, bool isCorrelated);
	void GetAcInRunwayAreaSoonDistance(CRadarTarget Ac, CRadarScreen *instance);
	//void AddRunwayArea(CRadarScreen *instance, string Name, vector<CPosition> Definition);
	Color GetAircraftColor(string AcCallsign, Color StandardColor, Color OnRunwayColor, Color RimcasStageOne, Color RimcasStageTwo);
	Color GetAircraftColor(string AcCallsign, Color StandardColor, Color OnRunwayColor);

	bool isAcOnRunway(string callsign);

	vector<CPosition> GetRunwayArea(CPosition Left, CPosition Right, float hwidth = 92.5f);

	void OnRefreshBegin(bool isLVP);
	void OnRefresh(CRadarTarget Rt, CRadarScreen *instance, bool isCorrelated);
	void OnRefreshEnd(CRadarScreen *instance, int threshold);
	void Reset();

	RimcasAlertTypes getAlert(string callsign);

	void setCountdownDefinition(vector<int> data, vector<int> dataLVP)
	{
		CountdownDefinition = data;
		std::sort(CountdownDefinition.begin(), CountdownDefinition.end(), std::greater<int>());

		CountdownDefinitionLVP = dataLVP;
		std::sort(CountdownDefinitionLVP.begin(), CountdownDefinitionLVP.end(), std::greater<int>());
	}

	void ToggleClosedRunway(string name) {
		for (auto &runway : Runways) {
			if (runway.name == name) {
				runway.closed = !runway.closed;
				return;
			}
		}
	}

	void ToggleMonitoredRunwayDep(string name) {
		for (auto &runway : Runways) {
			if (runway.name == name) {
				runway.monitor_dep = !runway.monitor_dep;
				return;
			}
		}
	}

	void ToggleMonitoredRunwayArr(string name) {
		for (auto &runway : Runways) {
			if (runway.name == name) {
				runway.monitor_arr = !runway.monitor_arr;
				return;
			}
		}
	}

	//map<string, bool> ClosedRunway;
};
