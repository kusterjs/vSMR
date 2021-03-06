#pragma once

#include "stdafx.h"
#include "EuroScopePlugIn.h"
#define _USE_MATH_DEFINES
// ReSharper disable once CppUnusedIncludeDirective
#include <math.h>
#include <vector>
#include <sstream>
#include <iomanip>

#include "bstrlib\bstrwrap.h"

#define VSTRIPS_PORT 53487

using namespace std;
using namespace EuroScopePlugIn;

const int TAG_ITEM_DATALINK_STS = 444;
const int TAG_FUNC_DATALINK_MENU = 544;

const int TAG_FUNC_DATALINK_CONFIRM = 545;
const int TAG_FUNC_DATALINK_STBY = 546;
const int TAG_FUNC_DATALINK_VOICE = 547;
const int TAG_FUNC_DATALINK_RESET = 548;
const int TAG_FUNC_DATALINK_MESSAGE = 549;

const int TAG_FUNC_STAND_EDIT = 600;
const int TAG_FUNC_STAND_EDITOR = 601;
const int TAG_FUNC_SCRATCHPAD_EDITOR = 602;



inline static bool StartsWith(const char *pre, const char *str)
{
	size_t lenpre = strlen(pre), lenstr = strlen(str);
	return lenstr < lenpre ? false : strncmp(pre, str, lenpre) == 0;
};

inline static std::wstring ToWString(CBString source) {	
	std::wstring wide(&source.data[0], &source.data[source.slen]);
	return wide;
}



inline static Gdiplus::Rect CopyRect(CRect &rect)
{
	return Gdiplus::Rect(rect.left, rect.top, rect.Width(), rect.Height());
};

/*
inline static std::vector<CBString> &split(const CBString &s, char delim, std::vector<CBString> &elems) {
	std::stringstream ss(s);
	std::string item;
	while (std::getline(ss, item, delim)) {
		elems.push_back(item);
	}
	return elems;
};
inline static std::vector<std::string> split(const std::string &s, char delim) {
	std::vector<std::string> elems;
	split(s, delim, elems);
	return elems;
};
*/

inline static double TrueBearing(CPosition pos1, CPosition pos2)
{
	const float PI = float(atan2(0, -1));

	// returns the true bearing from pos1 to pos2
	double lat1 = pos1.m_Latitude / 180 * PI;
	double lat2 = pos2.m_Latitude / 180 * PI;
	double lon1 = pos1.m_Longitude / 180 * PI;
	double lon2 = pos2.m_Longitude / 180 * PI;
	double dir = fmod(atan2(sin(lon2 - lon1) * cos(lat2), cos(lat1) * sin(lat2) - sin(lat1) * cos(lat2) * cos(lon2 - lon1)), 2 * PI) * 180 / PI;

	return dir / 180 * PI;
};

inline static POINT rotate_point(POINT p, double angle, POINT c)
{
	double sine = sin(angle * M_PI / 180);
	double cosi = cos(angle * M_PI / 180);

	// translate point back to origin:
	p.x -= c.x;
	p.y -= c.y;

	// rotate point
	double xnew = p.x * cosi - p.y * sine;
	double ynew = p.x * sine + p.y * cosi;

	// translate point back:
	p.x = LONG(xnew + c.x);
	p.y = LONG(ynew + c.y);
	return p;
}

inline static bool RectIntersect(RECT RectA, RECT RectB)
{
	if (RectA.left < RectB.right && RectA.right > RectB.left &&
		RectA.bottom < RectB.top && RectA.top > RectB.bottom)
	{
		return true;
	}
	return false;
}

inline static double DistancePts(POINT p0, POINT p1)
{
	return sqrt((p1.x - p0.x)*(p1.x - p0.x) + (p1.y - p0.y)*(p1.y - p0.y));
}

inline static int Is_Left(const POINT &p0, const POINT &p1, const POINT &point)
{
	return ((p1.x - p0.x) * (point.y - p0.y) -
		(point.x - p0.x) * (p1.y - p0.y));
};

inline static bool Is_Inside(const POINT &point, const std::vector<POINT> &points_list)
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
};

// Liang-Barsky function by Daniel White @ http://www.skytopia.com/project/articles/compsci/clipping.html
// This function inputs 8 numbers, and outputs 4 new numbers (plus a boolean value to say whether the clipped line is drawn at all).
//
inline static bool LiangBarsky(RECT Area, POINT fromSrc, POINT toSrc, POINT &ClipFrom, POINT &ClipTo)         // The output values, so declare these outside.
{

	double edgeLeft, edgeRight, edgeBottom, edgeTop, x0src, y0src, x1src, y1src;

	edgeLeft = Area.left;
	edgeRight = Area.right;
	edgeBottom = Area.top;
	edgeTop = Area.bottom;

	x0src = fromSrc.x;
	y0src = fromSrc.y;
	x1src = toSrc.x;
	y1src = toSrc.y;

	double t0 = 0.0;    double t1 = 1.0;
	double xdelta = x1src - x0src;
	double ydelta = y1src - y0src;
	double p = 0, q = 0, r;

	for (int edge = 0; edge<4; edge++) {   // Traverse through left, right, bottom, top edges.
		if (edge == 0) { p = -xdelta;    q = -(edgeLeft - x0src); }
		if (edge == 1) { p = xdelta;     q = (edgeRight - x0src); }
		if (edge == 2) { p = -ydelta;    q = -(edgeBottom - y0src); }
		if (edge == 3) { p = ydelta;     q = (edgeTop - y0src); }
		r = q / p;
		if (p == 0 && q<0) return false;   // Don't draw line at all. (parallel line outside)

		if (p<0) {
			if (r>t1) return false;         // Don't draw line at all.
			else if (r>t0) t0 = r;            // Line is clipped!
		}
		else if (p>0) {
			if (r<t0) return false;      // Don't draw line at all.
			else if (r<t1) t1 = r;         // Line is clipped!
		}
	}

	ClipFrom.x = long(x0src + t0*xdelta);
	ClipFrom.y = long(y0src + t0*ydelta);
	ClipTo.x = long(x0src + t1*xdelta);
	ClipTo.y = long(y0src + t1*ydelta);

	return true;        // (clipped) line is drawn
};

static bool mouseWithin(POINT mouseLocation,CRect rect) {
	if (mouseLocation.x >= rect.left + 1 && mouseLocation.x <= rect.right - 1 && mouseLocation.y >= rect.top + 1 && mouseLocation.y <= rect.bottom - 1)
		return true;
	return false;
}
//---Radians-----------------------------------------

inline static double DegToRad(double x)
{
	return x / 180.0 * M_PI;
};

inline static double RadToDeg(double x)
{
	return x / M_PI * 180.0;
};

inline static CPosition BetterHarversine(CPosition init, double angle, double meters)
{
	CPosition newPos;

	double d = (meters*0.00053996) / 60 * M_PI / 180;
	double trk = DegToRad(angle);
	double lat0 = DegToRad(init.m_Latitude);
	double lon0 = DegToRad(init.m_Longitude);

	double lat = asin(sin(lat0) * cos(d) + cos(lat0) * sin(d) * cos(trk));
	double lon = cos(lat) == 0 ? lon0 : fmod(lon0 + asin(sin(trk) * sin(d) / cos(lat)) + M_PI, 2 * M_PI) - M_PI;

	newPos.m_Latitude = RadToDeg(lat);
	newPos.m_Longitude = RadToDeg(lon);

	return newPos;
};

/*
inline static string padWithZeros(int padding, int s)
{
	stringstream ss;
	ss << setfill('0') << setw(padding) << s;
	return ss.str();
};
*/
//

const int DRAWING_TAG = 1211;
const int DRAWING_AC_SYMBOL = 1212;
const int DRAWING_BACKGROUND_CLICK = 1213;
const int DRAWING_AC_SYMBOL_APPWINDOW_BASE = 1251;
const int DRAWING_AC_SYMBOL_APPWINDOW1 = 1252;
const int DRAWING_AC_SYMBOL_APPWINDOW2 = 1253;

const int TAG_CITEM_NO = 1910;
const int TAG_CITEM_CALLSIGN = 1911;
const int TAG_CITEM_FPBOX = 1912;
const int TAG_CITEM_RWY = 1913;
const int TAG_CITEM_GATE = 1914;
const int TAG_CITEM_MANUALCORRELATE = 1915;
const int TAG_CITEM_SID = 1916;
const int TAG_CITEM_GROUNDSTATUS = 1917;
const int TAG_CITEM_SCRATCHPAD = 1918;
const int TAG_CITEM_COMMTYPE = 1919;

const int FUNC_MANUAL_CALLSIGN = 2000;

// RIMCAS Menus & shit
const int RIMCAS_CLOSE = EuroScopePlugIn::TAG_ITEM_FUNCTION_NO;
const int RIMCAS_ACTIVE_AIRPORT = 7999;
const int RIMCAS_ACTIVE_AIRPORT_FUNC = 8008;
const int RIMCAS_MENU = 8000;
const int RIMCAS_AUTO_DECONFLICTION_TOGGLE = 8003;
const int RIMCAS_PROMODE_TOGGLE = 8004;
const int RIMCAS_CUSTOM_CURSOR_TOGGLE = 8005;
const int RIMCAS_QDM_TOGGLE = 8006;
const int RIMCAS_QDM_SELECT_TOGGLE = 8007;
const int RIMCAS_OPEN_LIST = 9873;
const int RIMCAS_TIMER = 8015;
const int RIMCAS_UPDATE_PROFILE = 8016;
const int RIMCAS_UPDATE_BRIGHNESS = 8017;

const int RIMCAS_UPDATE_FONTSIZE_EDIT = 8018;
const int RIMCAS_UPDATE_FONTSIZE_EDITOR = 8019;
const int RIMCAS_UPDATE_PTL_LENGTH_EDIT = 8020;
const int RIMCAS_UPDATE_PTL_LENGTH_EDITOR = 8021;
const int RIMCAS_UPDATE_PTL_WIDTH_EDIT = 8022;
const int RIMCAS_UPDATE_PTL_WIDTH_EDITOR = 8023;
const int RIMCAS_UPDATE_PTL_SPEED_EDIT = 8024;
const int RIMCAS_UPDATE_PTL_SPEED_EDITOR = 8025;

const int RIMCAS_UPDATE_LVP = 8120;
const int RIMCAS_CA_DEPARTURE_FUNC = 8121;
const int RIMCAS_CA_ARRIVAL_FUNC = 8122;
const int RIMCAS_CLOSED_RUNWAYS_FUNC = 8123;
const int RIMCAS_UPDATE_AFTERGLOW = 8130;
const int RIMCAS_UPDATE_GND_TRAIL = 8131;
const int RIMCAS_UPDATE_APP_TRAIL = 8132;
const int RIMCAS_UPDATE_RELEASE = 8134;
const int RIMCAS_UPDATE_ACQUIRE = 8135;

const int RIMCAS_IAW = 7000;

// SRW Windows
const int SRW_APPWINDOW = 6000;
const int SRW_UPDATE_ZOOM = 6010;
const int SRW_UPDATE_ROTATE = 6020;
const int SRW_UPDATE_FILTER = 6030;
const int SRW_UPDATE_RANGE = 6040;
const int SRW_UPDATE_CENTERLINE = 6050;
const int SRW_UPDATE_TICKSPACING = 6060;
const int SRW_UPDATE_PREDICTEDLENGTH = 6070;
const int SRW_UPDATE_PREDICTEDWIDTH = 6080;

const int TAG_FUNC_SRW_ZOOM_EDITOR = 6110;
const int TAG_FUNC_SRW_ROTATE_EDITOR = 6120;
const int TAG_FUNC_SRW_FILTER_EDITOR = 6130;
const int TAG_FUNC_SRW_RANGE_EDITOR = 6140;
const int TAG_FUNC_SRW_CENTERLINE_EDITOR = 6150;
const int TAG_FUNC_SRW_TICKSPACING_EDITOR = 6160;
const int TAG_FUNC_SRW_PREDICTEDLENGTH_EDITOR = 6170;
const int TAG_FUNC_SRW_PREDICTEDWIDTH_EDITOR = 6180;


// Brightness update
const int RIMCAS_BRIGHTNESS_LABEL = 301;
const int RIMCAS_BRIGHTNESS_SYMBOL = 302;
const int RIMCAS_BRIGHTNESS_AFTERGLOW = 303;

const int RIMCAS_DISTANCE_TOOL = 201;
