#pragma once
#include "stdafx.h"
#include <sstream>
#include <sstream>
#include <iomanip>
#include <fstream>

#include "bstrlib\bstrwrap.h"

using namespace std;

class Logger {
public:
	static bool ENABLED;
	static CBString DLL_PATH;

	static void info(CBString message) {
		if (Logger::ENABLED && DLL_PATH.length() > 0) {
			std::ofstream file;
			file.open(DLL_PATH + "\\vsmr.log", std::ofstream::out | std::ofstream::app);
			file << "INFO: " << message << endl;
			file.close();
		}
	}
};