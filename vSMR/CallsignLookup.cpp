#include "stdafx.h"
#include "CallsignLookup.hpp"

//
// CCallsignLookup Class by Even Rognlien, used with permission
//

CCallsignLookup::CCallsignLookup(CBString fileName) {


	ifstream myfile;

	myfile.open(fileName);

	if (myfile) {
		CBString line;
		stringstream sstream;

		while (getline(myfile, line)) {
			sstream << line;
			if (line[0] == ';') // ignore comments
				continue;

			CBString key;
			CBString value;

			char delimiter = '\t';
			CBString token;

			size_t pos1 = line.find(delimiter);
			key = line.midstr(0, pos1);
			line.remove(0, pos1 + 1);

			size_t pos2 = line.find(delimiter);
			line.remove(0, pos2 + 1);

			size_t pos3 = line.find(delimiter);
			value = line.midstr(0, pos3);

			value.toupper();
			callsigns[key] = value;
		}
	}

	myfile.close();
}

CBString CCallsignLookup::GetFullCallsign(CBString airlineCode) {

	if (callsigns.find(airlineCode) == callsigns.end())
		return "";

	return callsigns.find(airlineCode)->second;
}

CCallsignLookup::~CCallsignLookup()
{
}