#include "stdafx.h"
#include "Rimcas.hpp"

CRimcas::CRimcas()
{
}

CRimcas::~CRimcas()
{
	Reset();
}

void CRimcas::Reset()
{
	Logger::info(__FUNCSIG__);
	Runways.clear();
	AcColor.clear();
	AcOnRunway.clear();
	IAWQueue.clear();
	ApproachingAircrafts.clear();
}

void CRimcas::OnRefreshBegin(bool isLVP)
{
	Logger::info(__FUNCSIG__);
	AcColor.clear();
	AcOnRunway.clear();
	IAWQueue.clear();
	ApproachingAircrafts.clear();
	this->IsLVP = isLVP;
}

void CRimcas::OnRefresh(CRadarTarget Rt, CRadarScreen *instance, bool isCorrelated)
{
	Logger::info(__FUNCSIG__);
	GetAcInRunwayArea(Rt, instance);
	GetAcInRunwayAreaSoonDistance(Rt, instance);
}


void CRimcas::GetAcInRunwayArea(CRadarTarget Ac, CRadarScreen *instance)
{
	Logger::info(__FUNCSIG__);
	int AltitudeDif = Ac.GetPosition().GetFlightLevel() - Ac.GetPreviousPosition(Ac.GetPosition()).GetFlightLevel();
	if (!Ac.GetPosition().GetTransponderC())
		AltitudeDif = 0;

	if (Ac.GetGS() > 160 || AltitudeDif > 200)
		return;

	POINT AcPosPix = instance->ConvertCoordFromPositionToPixel(Ac.GetPosition().GetPosition());

	for (const auto &rwy : Runways) {
		if (!rwy.monitor_dep && !rwy.monitor_arr)
			continue;

		vector<POINT> RunwayOnScreen;
		for (const auto &Point : rwy.rimcas_path) {
			RunwayOnScreen.push_back(instance->ConvertCoordFromPositionToPixel(Point));
		}

		if (Is_Inside(AcPosPix, RunwayOnScreen)) {
			AcOnRunway.insert(std::pair<CBString, CBString>(rwy.name, Ac.GetCallsign()));
			IAWQueueColors.erase(Ac.GetCallsign());
			return;
		}
	}

	return;
}

void CRimcas::GetAcInRunwayAreaSoonDistance(CRadarTarget aircraft, CRadarScreen *instance)
{
	Logger::info(__FUNCSIG__);
	int AltitudeDif = aircraft.GetPosition().GetFlightLevel() - aircraft.GetPreviousPosition(aircraft.GetPosition()).GetFlightLevel();
	if (!aircraft.GetPosition().GetTransponderC())
		AltitudeDif = 0;

	// Making sure the AC is airborne and not climbing, but below transition
	if (aircraft.GetGS() < 50 ||
		AltitudeDif > 50 ||
		aircraft.GetPosition().GetPressureAltitude() > instance->GetPlugIn()->GetTransitionAltitude())
		return;

	// If the AC is already on the runway, then there is no point in this step
	if (isAcOnRunway(aircraft.GetCallsign()))
		return;

	POINT AcPosPix = instance->ConvertCoordFromPositionToPixel(aircraft.GetPosition().GetPosition());
	for (const auto &rwy : Runways) {
		if (!rwy.monitor_arr)
			continue;

		CBString callsign = aircraft.GetCallsign();

		// We compute the pixel positions of the runway and the distance from plane to runway threshold
		vector<POINT> RunwayOnScreen;
		double directionToThreshold;
		double distanceToThreshold = 20.0; // We only care about planes closer to the runway than 15NM so we can default to this value even if the plane is further out since it won't be included in the final list.
		for (const auto &Point : rwy.path) {
			RunwayOnScreen.push_back(instance->ConvertCoordFromPositionToPixel(Point));
			double distanceToRunwayPoint = aircraft.GetPosition().GetPosition().DistanceTo(Point);
			if (distanceToRunwayPoint < distanceToThreshold) {
				distanceToThreshold = distanceToRunwayPoint;
				directionToThreshold = aircraft.GetPosition().GetPosition().DirectionTo(Point);
			}
		}

		//bool isGoingToLand = false;
		//int AngleMin = -2;
		//int AngleMax = 2;
		//if (distanceToThreshold <= 10.0)
		//{
		//	AngleMin = -3;
		//	AngleMax = 3;
		//}

		//for (double a = AngleMin; a <= AngleMax; a += 0.5)
		//{
		//	POINT PredictedPosition = instance->ConvertCoordFromPositionToPixel(
		//		BetterHarversine(aircraft.GetPosition().GetPosition(), fmod(aircraft.GetTrackHeading() + a, 360), NauticalMilesToMeters(distanceToThreshold + 0.1))); // Add small offset to be safely inside the runway area
		//	isGoingToLand = Is_Inside(PredictedPosition, RunwayOnScreen);

		//	if (isGoingToLand)
		//		break;
		//}

		// Check if the plane is heading for the runway (dot product between plane direction vector and plane-threshold vector)
		double planeDirection = aircraft.GetPosition().GetReportedHeading();
		// We check if the plane's heading matches the runway direction, we give (more or less) +/-2° at 20NM and +/-3° at 10NM
		if (abs(planeDirection - directionToThreshold) < (400.0 / (distanceToThreshold*distanceToThreshold + 50.0))) {
			// The aircraft is going to be on the runway, add it to the AIW if it's not already present
			double timeToThreshold = (distanceToThreshold / aircraft.GetPosition().GetReportedGS()) * 3600; // NM / knot = 1h 
			if (timeToThreshold < 180.0) {

				//// Check if airplane is already in IAW queue before inserting it
				//for (auto acft : IAWQueue[it->first]) {
				//	if (acft.callsign == aircraft.GetCallsign()) { // Plane was already inserted in the queue, update its data
				//		acft.time = timeToThreshold;
				//		acft.distance = distanceToThreshold;
				//		return;
				//	}
				//}

				//if (IAWQueue.size() == 1) {
				//	return;
				//}

				if (IAWQueueColors.count(callsign) == 0) {
					IAWQueueColors[callsign] = IAWColors.front();
					IAWColors.push_back(IAWColors.front());
					IAWColors.pop_front();
				}

				IAW_Aircraft aircraftData;
				aircraftData.callsign = callsign;
				aircraftData.time = timeToThreshold;
				aircraftData.distance = distanceToThreshold;
				aircraftData.colors = IAWQueueColors[callsign];

				IAWQueue[rwy.name].insert(aircraftData);

				// If the AC is xx seconds away from the runway, we consider him on it
				int StageTwoTrigger = 20;
				if (IsLVP)
					StageTwoTrigger = 30;

				if (timeToThreshold <= StageTwoTrigger)
					AcOnRunway.insert(std::pair<CBString, CBString>(rwy.name, callsign));

				// If the AC is 45 seconds away from the runway, we consider him approaching
				if (timeToThreshold > StageTwoTrigger && timeToThreshold <= 45)
					ApproachingAircrafts.insert(std::pair<CBString, CBString>(rwy.name, callsign));

			}
			return;
		}
	}
}

vector<CPosition> CRimcas::GetRunwayArea(CPosition Left, CPosition Right, float hwidth)
{
	Logger::info(__FUNCSIG__);
	vector<CPosition> out;

	double RunwayBearing = RadToDeg(TrueBearing(Left, Right));

	out.push_back(BetterHarversine(Left, fmod(RunwayBearing + 90, 360), hwidth)); // Bottom Left
	out.push_back(BetterHarversine(Right, fmod(RunwayBearing + 90, 360), hwidth)); // Bottom Right
	out.push_back(BetterHarversine(Right, fmod(RunwayBearing - 90, 360), hwidth)); // Top Right
	out.push_back(BetterHarversine(Left, fmod(RunwayBearing - 90, 360), hwidth)); // Top Left

	return out;
}

void CRimcas::OnRefreshEnd(CRadarScreen *instance, int threshold)
{
	Logger::info(__FUNCSIG__);
	for (const auto &rwy : Runways) {
		if (!rwy.monitor_arr && !rwy.monitor_dep)
			continue;

		bool isOnClosedRunway = rwy.closed;	// @TODO I'm very confused about this
		bool isAnotherAcApproaching = ApproachingAircrafts.count(rwy.name) > 0;

		if (AcOnRunway.count(rwy.name) > 1 || isOnClosedRunway || isAnotherAcApproaching) {

			auto AcOnRunwayRange = AcOnRunway.equal_range(rwy.name);

			for (map<CBString, CBString>::iterator it2 = AcOnRunwayRange.first; it2 != AcOnRunwayRange.second; ++it2) {
				if (isOnClosedRunway) {
					AcColor[it2->second] = StageTwo;
				}
				else {
					if (instance->GetPlugIn()->RadarTargetSelect(it2->second).GetGS() > threshold) {
						// If the aircraft is on the runway and stage two, we check if 
						// the aircraft is going towards any aircraft thats on the runway
						// if not, we don't display the warning
						bool triggerStageTwo = false;
						CRadarTarget rd1 = instance->GetPlugIn()->RadarTargetSelect(it2->second);
						CRadarTargetPositionData currentRd1 = rd1.GetPosition();
						for (map<CBString, CBString>::iterator it3 = AcOnRunwayRange.first; it3 != AcOnRunwayRange.second; ++it3) {
							CRadarTarget rd2 = instance->GetPlugIn()->RadarTargetSelect(it3->second);

							double currentDist = currentRd1.GetPosition().DistanceTo(rd2.GetPosition().GetPosition());
							double oldDist = rd1.GetPreviousPosition(currentRd1).GetPosition()
								.DistanceTo(rd2.GetPreviousPosition(rd2.GetPosition()).GetPosition());

							if (currentDist < oldDist) {
								triggerStageTwo = true;
								break;
							}
						}

						if (triggerStageTwo)
							AcColor[it2->second] = StageTwo;
					}
					else {
						AcColor[it2->second] = StageOne;
					}
				}
			}

			for (const auto &ac : ApproachingAircrafts) {
				if (ac.first == rwy.name && AcOnRunway.count(rwy.name) > 1)
					AcColor[ac.second] = StageOne;

				if (ac.first == rwy.name && isOnClosedRunway)
					AcColor[ac.second] = StageTwo;
			}
		}

	}

}

bool CRimcas::isAcOnRunway(CBString callsign)
{
	Logger::info(__FUNCSIG__);
	for (std::map<CBString, CBString>::iterator it = AcOnRunway.begin(); it != AcOnRunway.end(); ++it) {
		if (it->second == callsign)
			return true;
	}

	return false;
}

CRimcas::RimcasAlertTypes CRimcas::getAlert(CBString callsign)
{
	Logger::info(__FUNCSIG__);
	if (AcColor.find(callsign) == AcColor.end())
		return NoAlert;

	return AcColor[callsign];
}

Color CRimcas::GetAircraftColor(CBString AcCallsign, Color StandardColor, Color OnRunwayColor, Color RimcasStageOne, Color RimcasStageTwo)
{
	Logger::info(__FUNCSIG__);
	if (AcColor.find(AcCallsign) == AcColor.end()) {
		if (isAcOnRunway(AcCallsign)) {
			return OnRunwayColor;
		}
		else {
			return StandardColor;
		}
	}
	else {
		if (AcColor[AcCallsign] == StageOne) {
			return RimcasStageOne;
		}
		else {
			return RimcasStageTwo;
		}
	}
}

Color CRimcas::GetAircraftColor(CBString AcCallsign, Color StandardColor, Color OnRunwayColor)
{
	Logger::info(__FUNCSIG__);
	if (isAcOnRunway(AcCallsign)) {
		return OnRunwayColor;
	}
	else {
		return StandardColor;
	}
}
