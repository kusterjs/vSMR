#pragma once
#include <fstream>
#include <sstream>
#include <map>
#include "bstrlib\bstrwrap.h"

using namespace std;

class CCallsignLookup
{
private:
	std::map<CBString, CBString> callsigns;


public:

	CCallsignLookup(CBString fileName);
	CBString getCallsign(CBString airlineCode);

	~CCallsignLookup();
};