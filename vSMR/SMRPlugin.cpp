#include "stdafx.h"
#include "SMRPlugin.hpp"

bool Logger::ENABLED;
CBString Logger::DLL_PATH;

bool HoppieConnected = false;
bool ConnectionMessage = false;

CBString logonCode = "";
CBString logonCallsign = "LSGG";

HttpHelper* httpHelper = NULL;

bool BLINK = false;
bool PlaySoundClr = false;

struct DatalinkPacket
{
	CBString callsign;
	CBString destination;
	CBString sid;
	CBString rwy;
	CBString freq;
	CBString ctot;
	CBString asat;
	CBString squawk;
	CBString message;
	CBString climb;
};

DatalinkPacket DatalinkToSend;

CBString baseUrlDatalink = "http://www.hoppie.nl/acars/system/connect.html";

struct AcarsMessage
{
	CBString from;
	CBString type;
	CBString message;
};

vector<CBString> AircraftDemandingClearance;
vector<CBString> AircraftMessageSent;
vector<CBString> AircraftMessage;
vector<CBString> AircraftWilco;
vector<CBString> AircraftStandby;
map<CBString, AcarsMessage> PendingMessages;

CBString tmessage;
CBString tdest;
CBString ttype;

int messageId = 0;

clock_t timer;

CBString myfrequency;

using namespace SMRPluginSharedData;
char recv_buf[1024];

vector<CSMRRadar*> RadarScreensOpened;

void datalinkLogin(void * arg)
{
	CBString url = baseUrlDatalink;
	url += "?logon=";
	url += logonCode;
	url += "&from=";
	url += logonCallsign;
	url += "&to=SERVER&type=PING";
	CBString response = httpHelper->downloadStringFromURL(url);

	if (StartsWith("ok", response)) {
		HoppieConnected = true;
		ConnectionMessage = true;
	}
};

void sendDatalinkMessage(void * arg)
{
	CBString url = baseUrlDatalink;
	url += "?logon=";
	url += logonCode;
	url += "&from=";
	url += logonCallsign;
	url += "&to=";
	url += tdest;
	url += "&type=";
	url += ttype;
	url += "&packet=";
	url += tmessage;

	url.findreplace(" ", "%20");
	CBString response = httpHelper->downloadStringFromURL(url);

	if (StartsWith("ok", response)) {
		if (PendingMessages.find(DatalinkToSend.callsign) != PendingMessages.end())
			PendingMessages.erase(DatalinkToSend.callsign);
		if (std::find(AircraftMessage.begin(), AircraftMessage.end(), DatalinkToSend.callsign) != AircraftMessage.end()) {
			AircraftMessage.erase(std::remove(AircraftMessage.begin(), AircraftMessage.end(), DatalinkToSend.callsign), AircraftMessage.end());
		}
		AircraftMessageSent.push_back(DatalinkToSend.callsign);
	}
};

void pollMessages(void * arg)
{
	CBString url = baseUrlDatalink;
	url += "?logon=";
	url += logonCode;
	url += "&from=";
	url += logonCallsign;
	url += "&to=SERVER&type=POLL";
	CBString response = httpHelper->downloadStringFromURL(url);

	if (!StartsWith("ok", response) || response.length() <= 3)
		return;

	response.remove(0, 3);
	//response = response.midstr(3, response.length() - 3);
	response.trim();
	response.trim('{');
	response.trim('}');

	CBStringList parts;
	parts.split(response, '{');

	if (parts.size() < 2)
		return;

	CBStringList from_and_type;
	from_and_type.split(parts[0], ' ');
	
	if (from_and_type.size() < 2)
		return;

	struct AcarsMessage acars;
	acars.from = from_and_type[0];
	acars.type = from_and_type[1];
	acars.message = parts[1];

	if (acars.type == "cpdlc" || acars.type == "telex") {
		if (acars.message.find("REQ") != BSTR_ERR || acars.message.find("CLR") != BSTR_ERR || acars.message.find("PDC") != BSTR_ERR || acars.message.find("PREDEP") != BSTR_ERR || acars.message.find("REQUEST") != BSTR_ERR) {
			if (acars.message.find("LOGON") != BSTR_ERR) {
				tmessage = "UNABLE";
				ttype = "CPDLC";
				tdest = DatalinkToSend.callsign;
				_beginthread(sendDatalinkMessage, 0, NULL);
			}
			else {
				if (PlaySoundClr) {
					AFX_MANAGE_STATE(AfxGetStaticModuleState());
					PlaySound(MAKEINTRESOURCE(IDR_WAVE1), AfxGetInstanceHandle(), SND_RESOURCE | SND_ASYNC);
				}
				AircraftDemandingClearance.push_back(acars.from);
			}
		}	
		else if (acars.message.find("WILCO") != BSTR_ERR || acars.message.find("ROGER") != BSTR_ERR || acars.message.find("RGR") != BSTR_ERR) {
			if (std::find(AircraftMessageSent.begin(), AircraftMessageSent.end(), acars.from) != AircraftMessageSent.end()) {
				AircraftWilco.push_back(acars.from);
			}
		}
		else if (acars.message.length() != 0) {
			AircraftMessage.push_back(acars.from);
		}
		PendingMessages[acars.from] = acars;
	}
}


void sendDatalinkClearance(void * arg)
{
	CBString raw;
	CBString url = baseUrlDatalink;
	url += "?logon=";
	url += logonCode;
	url += "&from=";
	url += logonCallsign;
	url += "&to=";
	url += DatalinkToSend.callsign;
	url += "&type=CPDLC&packet=/data2/";
	messageId++;
	url += CBString(*bformat("%d", messageId));
	url += "//R/";
	url += "CLR TO @";
	url += DatalinkToSend.destination;
	url += "@ RWY @";
	url += DatalinkToSend.rwy;
	url += "@ DEP @";
	url += DatalinkToSend.sid;
	url += "@ INIT CLB @";
	url += DatalinkToSend.climb;
	url += "@ SQUAWK @";
	url += DatalinkToSend.squawk;
	url += "@ ";
	if (DatalinkToSend.ctot != "no" && DatalinkToSend.ctot.length() > 3) {
		url += "CTOT @";
		url += DatalinkToSend.ctot;
		url += "@ ";
	}
	if (DatalinkToSend.asat != "no" && DatalinkToSend.asat.length() > 3) {
		url += "TSAT @";
		url += DatalinkToSend.asat;
		url += "@ ";
	}
	if (DatalinkToSend.freq != "no" && DatalinkToSend.freq.length() > 5) {
		url += "WHEN RDY CALL FREQ @";
		url += DatalinkToSend.freq;
		url += "@";
	}
	else {
		url += "WHEN RDY CALL @";
		url += myfrequency;
		url += "@";
	}
	url += " IF UNABLE CALL VOICE ";
	if (DatalinkToSend.message != "no" && DatalinkToSend.message.length() > 1)
		url += DatalinkToSend.message;

	url.findreplace(" ", "%20");

	CBString response = httpHelper->downloadStringFromURL(url);

	if (StartsWith("ok", response)) {
		if (std::find(AircraftDemandingClearance.begin(), AircraftDemandingClearance.end(), DatalinkToSend.callsign) != AircraftDemandingClearance.end()) {
			AircraftDemandingClearance.erase(std::remove(AircraftDemandingClearance.begin(), AircraftDemandingClearance.end(), DatalinkToSend.callsign), AircraftDemandingClearance.end());
		}
		if (std::find(AircraftStandby.begin(), AircraftStandby.end(), DatalinkToSend.callsign) != AircraftStandby.end()) {
			AircraftStandby.erase(std::remove(AircraftStandby.begin(), AircraftStandby.end(), DatalinkToSend.callsign), AircraftStandby.end());
		}
		if (PendingMessages.find(DatalinkToSend.callsign) != PendingMessages.end())
			PendingMessages.erase(DatalinkToSend.callsign);
		AircraftMessageSent.push_back(DatalinkToSend.callsign);
	}
};



CSMRPlugin::CSMRPlugin(void) :CPlugIn(EuroScopePlugIn::COMPATIBILITY_CODE, MY_PLUGIN_NAME, MY_PLUGIN_VERSION, MY_PLUGIN_DEVELOPER, MY_PLUGIN_COPYRIGHT)
{

	Logger::DLL_PATH = "";
	Logger::ENABLED = false;

	//
	// Adding the SMR Display type
	//
	RegisterDisplayType(MY_PLUGIN_VIEW_AVISO, false, true, true, true);

	RegisterTagItemType("Datalink clearance", TAG_ITEM_DATALINK_STS);
	RegisterTagItemType("Stand number", TAG_CITEM_GATE);
	RegisterTagItemFunction("Datalink menu", TAG_FUNC_DATALINK_MENU);
	RegisterTagItemFunction("Edit stand", TAG_FUNC_STAND_EDIT);


	messageId = rand() % 10000 + 1789;

	timer = clock();

	if (httpHelper == NULL)
		httpHelper = new HttpHelper();

	const char * p_value;

	if ((p_value = GetDataFromSettings("cpdlc_logon")) != NULL)
		logonCallsign = p_value;
	if ((p_value = GetDataFromSettings("cpdlc_password")) != NULL)
		logonCode = p_value;
	if ((p_value = GetDataFromSettings("cpdlc_sound")) != NULL)
		PlaySoundClr = bool(!!atoi(p_value));

	char DllPathFile[_MAX_PATH];

	GetModuleFileNameA(HINSTANCE(&__ImageBase), DllPathFile, sizeof(DllPathFile));
	Logger::DLL_PATH = DllPathFile;
	Logger::DLL_PATH.rtrim("vSMR.dll");
}

CSMRPlugin::~CSMRPlugin()
{	
	// I do not understand where this is saved...
	SaveDataToSettings("cpdlc_logon", "The CPDLC logon callsign", logonCallsign);
	SaveDataToSettings("cpdlc_password", "The CPDLC logon password", logonCode);
	CBString temp = "0";
	if (PlaySoundClr)
		temp = "1";
	SaveDataToSettings("cpdlc_sound", "Play sound on clearance request", temp);

	try {
		io_service.stop();
		//vStripsThread.join();
	}
	catch (std::exception& e) {
		std::cerr << e.what() << std::endl;
	}
}

void CSMRPlugin::OnAirportRunwayActivityChanged()
{
	for (const auto &radar : RadarScreensOpened) {
		radar->ReloadActiveRunways();
	}
}

bool CSMRPlugin::OnCompileCommand(const char * sCommandLine)
{
	if (StartsWith(".smr connect", sCommandLine)) {
		if (ControllerMyself().IsController()) {
			if (!HoppieConnected) {
				_beginthread(datalinkLogin, 0, NULL);
			}
			else {
				HoppieConnected = false;
				DisplayUserMessage("CPDLC", "Server", "Logged off!", true, true, false, true, false);
			}
		}
		else {
			DisplayUserMessage("CPDLC", "Error", "You are not logged in as a controller!", true, true, false, true, false);
		}

		return true;
	}
	else if (StartsWith(".smr poll", sCommandLine)) {
		if (HoppieConnected) {
			_beginthread(pollMessages, 0, NULL);
		}
		return true;
	}
	else if (StartsWith(".smr log", sCommandLine)) {
		Logger::ENABLED = !Logger::ENABLED;
		return true;
	}
	else if (StartsWith(".smr", sCommandLine)) {
		CCPDLCSettingsDialog dia;
		dia.m_Logon = CString((const char*)logonCallsign);
		dia.m_Password = CString((const char*)logonCode);
		dia.m_Sound = int(PlaySoundClr);

		if (dia.DoModal() != IDOK)
			return true;

		logonCallsign = dia.m_Logon;
		logonCode = dia.m_Password;
		PlaySoundClr = bool(!!dia.m_Sound);
		SaveDataToSettings("cpdlc_logon", "The CPDLC logon callsign", logonCallsign);
		SaveDataToSettings("cpdlc_password", "The CPDLC logon password", logonCode);
		CBString temp = "0";
		if (PlaySoundClr)
			temp = "1";
		SaveDataToSettings("cpdlc_sound", "Play sound on clearance request", temp);

		return true;
	}
	return false;
}

void CSMRPlugin::OnGetTagItem(CFlightPlan FlightPlan, CRadarTarget RadarTarget, int ItemCode, int TagData, char sItemString[16], int * pColorCode, COLORREF * pRGB, double * pFontSize)
{
	Logger::info(__FUNCSIG__);
	if (ItemCode == TAG_CITEM_GATE) {
		if (FlightPlan.IsValid()) {
			strcpy_s(sItemString, 16, CSMRRadar::GetStandNumber(FlightPlan));		
		}
	}

	if (ItemCode == TAG_ITEM_DATALINK_STS) {
		if (FlightPlan.IsValid()) {
			if (std::find(AircraftDemandingClearance.begin(), AircraftDemandingClearance.end(), FlightPlan.GetCallsign()) != AircraftDemandingClearance.end()) {
				*pColorCode = TAG_COLOR_RGB_DEFINED;
				if (BLINK)
					*pRGB = RGB(130, 130, 130);
				else
					*pRGB = RGB(255, 255, 0);

				if (std::find(AircraftStandby.begin(), AircraftStandby.end(), FlightPlan.GetCallsign()) != AircraftStandby.end())
					strcpy_s(sItemString, 16, "S");
				else
					strcpy_s(sItemString, 16, "R");
			}
			else if (std::find(AircraftMessage.begin(), AircraftMessage.end(), FlightPlan.GetCallsign()) != AircraftMessage.end()) {
				*pColorCode = TAG_COLOR_RGB_DEFINED;
				if (BLINK)
					*pRGB = RGB(130, 130, 130);
				else
					*pRGB = RGB(255, 255, 0);
				strcpy_s(sItemString, 16, "T");
			}
			else if (std::find(AircraftWilco.begin(), AircraftWilco.end(), FlightPlan.GetCallsign()) != AircraftWilco.end()) {
				*pColorCode = TAG_COLOR_RGB_DEFINED;
				*pRGB = RGB(0, 176, 0);
				strcpy_s(sItemString, 16, "V");
			}
			else if (std::find(AircraftMessageSent.begin(), AircraftMessageSent.end(), FlightPlan.GetCallsign()) != AircraftMessageSent.end()) {
				*pColorCode = TAG_COLOR_RGB_DEFINED;
				*pRGB = RGB(255, 255, 0);
				strcpy_s(sItemString, 16, "V");
			}
			else {
				*pColorCode = TAG_COLOR_RGB_DEFINED;
				*pRGB = RGB(130, 130, 130);

				strcpy_s(sItemString, 16, "-");
			}
		}
	}
}

void CSMRPlugin::OnFunctionCall(int FunctionId, const char * sItemString, POINT Pt, RECT Area)
{
	Logger::info(__FUNCSIG__);
	Logger::info(*bformat("%d - %s", FunctionId, sItemString));

	/* 	-----------------------------------------------------------------------------------------------
	This function is seemingly called twice when coming from a popup edit box
	Who the hell knows why... but it is quite problematic

	Also, both the CPlugIn AND the CRadarScreen versions of the function always get called together,
	so technically (apparently) you could have the implementation of both into just one...
	not sure what the point would be or if it's any good either, just weird all around
	----------------------------------------------------------------------------------------------- */

	switch (FunctionId) {

	case TAG_FUNC_STAND_EDIT: {
			CFlightPlan fp = FlightPlanSelectASEL();
			OpenPopupEdit(Area, TAG_FUNC_STAND_EDITOR, CSMRRadar::GetStandNumber(fp));
			CSMRRadar::onFunctionCallDoubleCallHack = true;
			break;
		}

	case TAG_FUNC_STAND_EDITOR: { // when finished editing
			if (CSMRRadar::onFunctionCallDoubleCallHack) {
				CFlightPlan fp = FlightPlanSelectASEL();
				CSMRRadar::SetStandNumber(fp, sItemString);
				CSMRRadar::onFunctionCallDoubleCallHack = false;
			}
			break;
		}
	
	case TAG_FUNC_DATALINK_MENU: {
		CFlightPlan FlightPlan = FlightPlanSelectASEL();

		bool menu_is_datalink = true;

		if (FlightPlan.IsValid()) {
			if (std::find(AircraftDemandingClearance.begin(), AircraftDemandingClearance.end(), FlightPlan.GetCallsign()) != AircraftDemandingClearance.end()) {
				menu_is_datalink = false;
			}
		}

		OpenPopupList(Area, "Datalink menu", 1);
		AddPopupListElement("Confirm", "", TAG_FUNC_DATALINK_CONFIRM, false, 2, menu_is_datalink);
		AddPopupListElement("Message", "", TAG_FUNC_DATALINK_MESSAGE, false, 2, false, true);
		AddPopupListElement("Standby", "", TAG_FUNC_DATALINK_STBY, false, 2, menu_is_datalink);
		AddPopupListElement("Voice", "", TAG_FUNC_DATALINK_VOICE, false, 2, menu_is_datalink);
		AddPopupListElement("Reset", "", TAG_FUNC_DATALINK_RESET, false, 2, false, true);
		AddPopupListElement("Close", "", EuroScopePlugIn::TAG_ITEM_FUNCTION_NO, false, 2, false, true);

		break;
	}

	case TAG_FUNC_DATALINK_RESET: {
		CFlightPlan FlightPlan = FlightPlanSelectASEL();

		if (FlightPlan.IsValid()) {
			if (std::find(AircraftDemandingClearance.begin(), AircraftDemandingClearance.end(), FlightPlan.GetCallsign()) != AircraftDemandingClearance.end()) {
				AircraftDemandingClearance.erase(std::remove(AircraftDemandingClearance.begin(), AircraftDemandingClearance.end(), FlightPlan.GetCallsign()), AircraftDemandingClearance.end());
			}
			if (std::find(AircraftStandby.begin(), AircraftStandby.end(), FlightPlan.GetCallsign()) != AircraftStandby.end()) {
				AircraftStandby.erase(std::remove(AircraftStandby.begin(), AircraftStandby.end(), FlightPlan.GetCallsign()), AircraftStandby.end());
			}
			if (std::find(AircraftMessageSent.begin(), AircraftMessageSent.end(), FlightPlan.GetCallsign()) != AircraftMessageSent.end()) {
				AircraftMessageSent.erase(std::remove(AircraftMessageSent.begin(), AircraftMessageSent.end(), FlightPlan.GetCallsign()), AircraftMessageSent.end());
			}
			if (std::find(AircraftWilco.begin(), AircraftWilco.end(), FlightPlan.GetCallsign()) != AircraftWilco.end()) {
				AircraftWilco.erase(std::remove(AircraftWilco.begin(), AircraftWilco.end(), FlightPlan.GetCallsign()), AircraftWilco.end());
			}
			if (std::find(AircraftMessage.begin(), AircraftMessage.end(), FlightPlan.GetCallsign()) != AircraftMessage.end()) {
				AircraftMessage.erase(std::remove(AircraftMessage.begin(), AircraftMessage.end(), FlightPlan.GetCallsign()), AircraftMessage.end());
			}
			if (PendingMessages.find(FlightPlan.GetCallsign()) != PendingMessages.end()) {
				PendingMessages.erase(FlightPlan.GetCallsign());
			}
		}
		break;
	}

	case TAG_FUNC_DATALINK_STBY: {
		CFlightPlan FlightPlan = FlightPlanSelectASEL();

		if (FlightPlan.IsValid()) {
			AircraftStandby.push_back(FlightPlan.GetCallsign());
			tmessage = "STANDBY";
			ttype = "CPDLC";
			tdest = FlightPlan.GetCallsign();
			_beginthread(sendDatalinkMessage, 0, NULL);
		}
		break;
	}

	case TAG_FUNC_DATALINK_MESSAGE: {
		CFlightPlan FlightPlan = FlightPlanSelectASEL();

		if (FlightPlan.IsValid()) {
			AFX_MANAGE_STATE(AfxGetStaticModuleState());

			CDataLinkDialog dia;
			dia.m_Callsign = FlightPlan.GetCallsign();
			dia.m_Aircraft = FlightPlan.GetFlightPlanData().GetAircraftFPType();
			dia.m_Dest = FlightPlan.GetFlightPlanData().GetDestination();
			dia.m_From = FlightPlan.GetFlightPlanData().GetOrigin();

			AcarsMessage msg = PendingMessages[FlightPlan.GetCallsign()];
			dia.m_Req = CString((const char*)msg.message);
						
			if (dia.DoModal() != IDOK)
				return;

			tmessage = dia.m_Message;
			ttype = "TELEX";
			tdest = FlightPlan.GetCallsign();
			_beginthread(sendDatalinkMessage, 0, NULL);
		}
		break;
	}

	case TAG_FUNC_DATALINK_VOICE: {
		CFlightPlan FlightPlan = FlightPlanSelectASEL();

		if (FlightPlan.IsValid()) {
			tmessage = "UNABLE CALL ON FREQ";
			ttype = "CPDLC";
			tdest = FlightPlan.GetCallsign();

			if (std::find(AircraftDemandingClearance.begin(), AircraftDemandingClearance.end(), DatalinkToSend.callsign) != AircraftDemandingClearance.end()) {
				AircraftDemandingClearance.erase(std::remove(AircraftDemandingClearance.begin(), AircraftDemandingClearance.end(), FlightPlan.GetCallsign()), AircraftDemandingClearance.end());
			}
			if (std::find(AircraftStandby.begin(), AircraftStandby.end(), DatalinkToSend.callsign) != AircraftStandby.end()) {
				AircraftStandby.erase(std::remove(AircraftStandby.begin(), AircraftStandby.end(), FlightPlan.GetCallsign()), AircraftDemandingClearance.end());
			}
			PendingMessages.erase(DatalinkToSend.callsign);

			_beginthread(sendDatalinkMessage, 0, NULL);
		}
		break;
	}

	case TAG_FUNC_DATALINK_CONFIRM: {
		CFlightPlan FlightPlan = FlightPlanSelectASEL();

		if (FlightPlan.IsValid()) {

			AFX_MANAGE_STATE(AfxGetStaticModuleState());

			CDataLinkDialog dia;
			dia.m_Callsign = FlightPlan.GetCallsign();
			dia.m_Aircraft = FlightPlan.GetFlightPlanData().GetAircraftFPType();
			dia.m_Dest = FlightPlan.GetFlightPlanData().GetDestination();
			dia.m_From = FlightPlan.GetFlightPlanData().GetOrigin();
			dia.m_Departure = FlightPlan.GetFlightPlanData().GetSidName();
			dia.m_Rwy = FlightPlan.GetFlightPlanData().GetDepartureRwy();
			dia.m_SSR = FlightPlan.GetControllerAssignedData().GetSquawk();

			CBString nextFreq = *bformat("%.3f", ControllerMyself().GetPrimaryFrequency());
			if (ControllerSelect(FlightPlan.GetCoordinatedNextController()).GetPrimaryFrequency() != 0)
				nextFreq = *bformat("%.3f", ControllerSelect(FlightPlan.GetCoordinatedNextController()).GetPrimaryFrequency());
			nextFreq.trunc(7);
			dia.m_Freq = CString((const char*)nextFreq);
			
			AcarsMessage msg = PendingMessages[FlightPlan.GetCallsign()];
			dia.m_Req = CString((const char*)msg.message);
				
			int ClearedAltitude = FlightPlan.GetControllerAssignedData().GetClearedAltitude();
			int Ta = GetTransitionAltitude();
			CBString initClimb;

			if (ClearedAltitude != 0) {
				if (ClearedAltitude > Ta && ClearedAltitude > 2) {					
					initClimb.format("FL%03d", ClearedAltitude/100);
				}
				else if (ClearedAltitude <= Ta && ClearedAltitude > 2) {
					initClimb.format("%dft", ClearedAltitude);
				}
			}
			dia.m_Climb = CString((const char*)initClimb);


			if (dia.DoModal() != IDOK)
				return;

			DatalinkToSend.callsign = dia.m_Callsign;
			DatalinkToSend.destination = dia.m_Dest;
			DatalinkToSend.rwy = dia.m_Rwy;
			DatalinkToSend.sid = dia.m_Departure;
			DatalinkToSend.asat = dia.m_TSAT;
			DatalinkToSend.ctot = dia.m_CTOT;
			DatalinkToSend.freq = dia.m_Freq;
			DatalinkToSend.message = dia.m_Message;
			DatalinkToSend.squawk = dia.m_SSR;
			DatalinkToSend.climb = dia.m_Climb;

			myfrequency.format("%.3f", ControllerMyself().GetPrimaryFrequency());

			_beginthread(sendDatalinkClearance, 0, NULL);

		}
		break;
	}
	}
}

void CSMRPlugin::OnControllerDisconnect(CController Controller)
{
	Logger::info(__FUNCSIG__);
	if (Controller.GetFullName() == ControllerMyself().GetFullName() && HoppieConnected == true) {
		HoppieConnected = false;
		DisplayUserMessage("CPDLC", "Server", "Logged off!", true, true, false, true, false);
	}
}

void CSMRPlugin::OnFlightPlanDisconnect(CFlightPlan FlightPlan)
{
	Logger::info(__FUNCSIG__);
	CRadarTarget rt = RadarTargetSelect(FlightPlan.GetCallsign());

	//if (std::find(ReleasedTracks.begin(), ReleasedTracks.end(), rt.GetSystemID()) != ReleasedTracks.end())
		//ReleasedTracks.erase(std::find(ReleasedTracks.begin(), ReleasedTracks.end(), rt.GetSystemID()));

	if (CSMRRadar::manuallyCorrelated.count(rt.GetSystemID()) > 0) {
		CSMRRadar::manuallyCorrelated.erase(rt.GetSystemID());
	}
}

void CSMRPlugin::OnTimer(int Counter)
{
	Logger::info(__FUNCSIG__);
	BLINK = !BLINK;

	if (HoppieConnected && ConnectionMessage) {
		DisplayUserMessage("CPDLC", "Server", "Logged in!", true, true, false, true, false);
		ConnectionMessage = false;
	}

	if (((clock() - timer) / CLOCKS_PER_SEC) > 10 && HoppieConnected) {
		_beginthread(pollMessages, 0, NULL);
		timer = clock();
	}

	for (auto &ac : AircraftWilco) {
		CRadarTarget RadarTarget = RadarTargetSelect(ac);

		if (RadarTarget.IsValid()) {
			if (RadarTarget.GetGS() > 160) {
				AircraftWilco.erase(std::remove(AircraftWilco.begin(), AircraftWilco.end(), ac), AircraftWilco.end());
			}
		}
	}
};

CRadarScreen * CSMRPlugin::OnRadarScreenCreated(const char * sDisplayName, bool NeedRadarContent, bool GeoReferenced, bool CanBeSaved, bool CanBeCreated)
{
	Logger::info(__FUNCSIG__);
	if (!strcmp(sDisplayName, MY_PLUGIN_VIEW_AVISO)) {
		CSMRRadar* rd = new CSMRRadar();
		RadarScreensOpened.push_back(rd);
		return rd;
	}

	return NULL;
}

//---EuroScopePlugInExit-----------------------------------------------

void __declspec (dllexport) EuroScopePlugInExit(void)
{	
	for each (auto var in RadarScreensOpened)
	{
		var->EuroScopePlugInExitCustom();
	}
}