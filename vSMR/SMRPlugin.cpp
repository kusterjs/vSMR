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

map<CBString, CBString> vStrips_Stands;

bool startThreadvStrips = true;

using namespace SMRPluginSharedData;
asio::ip::udp::socket* _socket;
asio::ip::udp::endpoint receiver_endpoint;
//std::thread vStripsThread;
char recv_buf[1024];

vector<CSMRRadar*> RadarScreensOpened;

void datalinkLogin(void * arg)
{
	CBString raw;
	CBString url = baseUrlDatalink;
	url += "?logon=";
	url += logonCode;
	url += "&from=";
	url += logonCallsign;
	url += "&to=SERVER&type=PING";
	raw = httpHelper->downloadStringFromURL(url);

	if (StartsWith("ok", raw)) {
		HoppieConnected = true;
		ConnectionMessage = true;
	}
};

void sendDatalinkMessage(void * arg)
{

	CBString raw;
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

	size_t start_pos = 0;
	while ((start_pos = url.find(' ', start_pos)) != BSTR_ERR) {
		url.replace(start_pos, string(" ").length(), "%20");
		start_pos += string("%20").length();
	}

	raw = httpHelper->downloadStringFromURL(url);

	if (StartsWith("ok", raw)) {
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
	CBString raw = "";
	CBString url = baseUrlDatalink;
	url += "?logon=";
	url += logonCode;
	url += "&from=";
	url += logonCallsign;
	url += "&to=SERVER&type=POLL";
	raw = httpHelper->downloadStringFromURL(url);

	if (!StartsWith("ok", raw) || raw.length() <= 3)
		return;

	raw = raw + " ";
	raw = raw.midstr(3, raw.length() - 3);

	CBString delimiter = "}} ";
	size_t pos = 0;
	CBString token;
	while ((pos = raw.find(delimiter)) != BSTR_ERR) {
		token = raw.midstr(1, pos);

		CBString parsed;
		stringstream input_stringstream(token);
		struct AcarsMessage message;
		int i = 1;
		while (getline(input_stringstream, parsed, ' ')) {
			if (i == 1)
				message.from = parsed;
			if (i == 2)
				message.type = parsed;
			if (i > 2) {
				message.message += " ";
				message.message += parsed;
			}

			i++;
		}
		if (message.type.find("telex") != BSTR_ERR || message.type.find("cpdlc") != BSTR_ERR) {
			if (message.message.find("REQ") != BSTR_ERR || message.message.find("CLR") != BSTR_ERR || message.message.find("PDC") != BSTR_ERR || message.message.find("PREDEP") != BSTR_ERR || message.message.find("REQUEST") != BSTR_ERR) {
				if (message.message.find("LOGON") != BSTR_ERR) {
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
					AircraftDemandingClearance.push_back(message.from);
				}
			}
			else if (message.message.find("WILCO") != BSTR_ERR || message.message.find("ROGER") != BSTR_ERR || message.message.find("RGR") != BSTR_ERR) {
				if (std::find(AircraftMessageSent.begin(), AircraftMessageSent.end(), message.from) != AircraftMessageSent.end()) {
					AircraftWilco.push_back(message.from);
				}
			}
			else if (message.message.length() != 0) {
				AircraftMessage.push_back(message.from);
			}
			PendingMessages[message.from] = message;
		}

		raw.remove(0, pos + delimiter.length());
	}


};

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
	assert(false);
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

	size_t start_pos = 0;
	assert(false);
	while ((start_pos = url.find(" ", start_pos)) != std::string::npos) {
		url.replace(start_pos, string(" ").length(), "%20");
		start_pos += string("%20").length();
	}

	raw = httpHelper->downloadStringFromURL(url);

	if (StartsWith("ok", raw)) {
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

void vStripsReceiveThread(const asio::error_code &error, size_t bytes_transferred)
{
	Logger::info(__FUNCSIG__);
	assert(false);
	CBString out(recv_buf, bytes_transferred);

	// Processing the data
	CBStringList data;
	data.split(out, ':');

	if (data.front() == "STAND") {
		CSMRRadar::vStripsStands[data.at(1)] = data.back();
	}

	if (data.front() == "DELETE") {
		if (CSMRRadar::vStripsStands.find(data.back()) != CSMRRadar::vStripsStands.end()) {
			CSMRRadar::vStripsStands.erase(CSMRRadar::vStripsStands.find(data.back()));
		}
	}

	if (!error) {
		_socket->async_receive_from(asio::buffer(recv_buf), receiver_endpoint,
			std::bind(&vStripsReceiveThread, std::placeholders::_1, std::placeholders::_2));
	}
}

void vStripsThreadFunction(void * arg)
{
	try {
		_socket = new asio::ip::udp::socket(io_service, asio::ip::udp::endpoint(asio::ip::udp::v4(), VSTRIPS_PORT));
		_socket->async_receive_from(asio::buffer(recv_buf), receiver_endpoint,
			std::bind(&vStripsReceiveThread, std::placeholders::_1, std::placeholders::_2));

		io_service.run();
	}
	catch (std::exception& e) {
		std::cerr << e.what() << std::endl;
	}
}


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

	try {
		//vStripsThread = std::thread();
		_beginthread(vStripsThreadFunction, 0, NULL);
	}
	catch (std::exception& e) {
		std::cerr << e.what() << std::endl;
	}
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

			CBString toReturn = "";

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
			CBString freq = *bformat("%f", ControllerMyself().GetPrimaryFrequency());
			assert(false);

			if (ControllerSelect(FlightPlan.GetCoordinatedNextController()).GetPrimaryFrequency() != 0)
				CBString freq = *bformat("%f", ControllerSelect(FlightPlan.GetCoordinatedNextController()).GetPrimaryFrequency());
			freq.trunc(7);
			dia.m_Freq = CString((const char*)freq);
			AcarsMessage msg = PendingMessages[FlightPlan.GetCallsign()];
			dia.m_Req = CString((const char*)msg.message);

			CBString toReturn = "";

			int ClearedAltitude = FlightPlan.GetControllerAssignedData().GetClearedAltitude();
			int Ta = GetTransitionAltitude();

			if (ClearedAltitude != 0) {
				if (ClearedAltitude > Ta && ClearedAltitude > 2) {
					assert(false);
					CBString str = *bformat("%d", ClearedAltitude);
					for (int i = 0; i < 5 - str.length(); i++)
						str = "0" + str;
					str.trunc(3);
					toReturn = "FL";
					toReturn += str;
				}
				else if (ClearedAltitude <= Ta && ClearedAltitude > 2) {
					toReturn.format("%d", ClearedAltitude);
					toReturn += "ft";
				}
			}
			dia.m_Climb = CString((const char*)toReturn);

			if (dia.DoModal() != IDOK)
				return;

			DatalinkToSend.callsign = FlightPlan.GetCallsign();
			DatalinkToSend.destination = FlightPlan.GetFlightPlanData().GetDestination();
			DatalinkToSend.rwy = FlightPlan.GetFlightPlanData().GetDepartureRwy();
			DatalinkToSend.sid = FlightPlan.GetFlightPlanData().GetSidName();
			DatalinkToSend.asat = dia.m_TSAT;
			DatalinkToSend.ctot = dia.m_CTOT;
			DatalinkToSend.freq = dia.m_Freq;
			DatalinkToSend.message = dia.m_Message;
			DatalinkToSend.squawk = FlightPlan.GetControllerAssignedData().GetSquawk();
			DatalinkToSend.climb = toReturn;

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