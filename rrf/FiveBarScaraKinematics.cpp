/*
 * FiveBarScaraKinematics.cpp
 *
 *  Created on: 11 Nov 2018
 *      Author: Joerg
 *
 *	documentation: https://duet3d.dozuki.com/Guide/Five+Bar+Parallel+SCARA/24?lang=en
 */

#include "FiveBarScaraKinematics.h"
#include "RepRap.h"
#include "Platform.h"
#include "Storage/MassStorage.h"
#include "GCodes/GCodeBuffer.h"
#include "Movement/DDA.h"

#include <limits>

//#define debugPrintf if(0) debugPrintf

FiveBarScaraKinematics::FiveBarScaraKinematics()
	: ZLeadscrewKinematics(KinematicsType::scara, DefaultSegmentsPerSecond, DefaultMinSegmentSize, true)
{
	Recalc();
}

// Return the name of the current kinematics
const char *FiveBarScaraKinematics::GetName(bool forStatusReport) const
{
	return "FiveBarScara";
}

//////////////////////// private functions /////////////////////////

// no results returnes. All results are stored in the cached variables.
void FiveBarScaraKinematics::getInverse(const float coords[]) const
{
	if(!cachedInvalid && coords[0] == cachedX0 && coords[1] == cachedY0) {		// already solved
		return;
	}

	float thetaL = -1.0;
	float thetaR = -1.0;
	float xL = -1.0;
	float xR = -1.0;
	float yL = -1.0;
	float yR = -1.0;
	float x1 = -1.0;
	float y1 = -1.0;

	float x_0 = coords[0];
	float y_0 = coords[1];

	if(isCantilevered(1)) {
		// calculate cantilevered side first:

		float lefttheta[6];
		getTheta(lefttheta, proximalL, distalL + cantL, xOrigL, yOrigL, x_0, y_0, Arm::left);
		xL = lefttheta[0];
		yL = lefttheta[1];
		thetaL = lefttheta[2];

		// calculate x1,y1, i.e. where the distal arms meet
		float fraction = distalL / (distalL + cantL);
		x1 = (x_0 - xL) * fraction + xL;
		y1 = (y_0 - yL) * fraction + yL;

		// calculate right, non cantilevered side:
		float righttheta[6];
		getTheta(righttheta, proximalR, distalR, xOrigR, yOrigR, x1, y1, Arm::right);
		xR = righttheta[0];
		yR = righttheta[1];
		thetaR = righttheta[2];
	}
	else if(isCantilevered(2)) {
		// calculate cantilevered side first:
		float righttheta[6];
		getTheta(righttheta, proximalR, distalR + cantR, xOrigR, yOrigR, x_0, y_0, Arm::right);
		xR = righttheta[0];
		yR = righttheta[1];
		thetaR = righttheta[2];

		// calculate x1,y1, i.e. where the distal arms meet
		float fraction = distalR / (distalR + cantR);
		x1 = (x_0 - xR) * fraction + xR;
		y1 = (y_0 - yR) * fraction + yR;

		// calculate left, non cantilevered side:
		float lefttheta[6];
		getTheta(lefttheta, proximalL, distalL, xOrigL, yOrigL, x1, y1, Arm::left);
		xL = lefttheta[0];
		yL = lefttheta[1];
		thetaL = lefttheta[2];

	}
	else {	// not cantilevered, hotend is at top joint
		float lefttheta[6];
		getTheta(lefttheta, proximalL, distalL, xOrigL, yOrigL, x_0, y_0, Arm::left);
		x1 = x_0;
		y1 = y_0;
		xL = lefttheta[0];
		yL = lefttheta[1];
		thetaL = lefttheta[2];

		float righttheta[6];
		getTheta(righttheta, proximalR, distalR, xOrigR, yOrigR, x_0, y_0, Arm::right);
		xR = righttheta[0];
		yR = righttheta[1];
		thetaR = righttheta[2];
	}

	if(1) { // PB constraintsOk(coords)) {
		cachedX0 = x_0;
		cachedY0 = y_0;
		cachedXL = xL;
		cachedYL = yL;
		cachedThetaL = thetaL;
		cachedXR = xR;
		cachedYR = yR;
		cachedThetaR = thetaR;
		cachedX1 = x1;
		cachedY1 = y1;
        if(std::isnan(cachedX0) || std::isnan(cachedY0) ||
           std::isnan(cachedX1) || std::isnan(cachedY1) ||
           std::isnan(cachedXL) || std::isnan(cachedYL) ||
           std::isnan(cachedXR) || std::isnan(cachedYR) ||
           std::isnan(cachedThetaR) || std::isnan(cachedXR) || std::isnan(cachedYR) ||
           std::isnan(cachedThetaL) || std::isnan(cachedXL) || std::isnan(cachedYL))
          cachedInvalid = true;
        else
          cachedInvalid = false;

		//cachedInvalid = false;
	}
	else {
		cachedInvalid = true;
	}

	return;
}

// for a given Parameter, get number of values which are separated by :
int FiveBarScaraKinematics::getNumParameters(char c, GCodeBuffer gb) const {
	String<MaxFilenameLength> str;
	bool seen = false;
	gb.TryGetPossiblyQuotedString(c, str.GetRef(), seen);

	int count = 0;
	size_t size = str.strlen();
	for (size_t i = 0; i < size; i++) {
		if (str[i] == ':') count++;
	}

	return count + 1;
}

// quadrants: 1 is right upper, 2 is left upper, 3 is left down, 4 is right down
int FiveBarScaraKinematics::getQuadrant(float x, float y) const
{
	if(x >= 0 && y >= 0) {
		return 1;
	}
	else if(x < 0 && y >= 0) {
		return 2;
	}
	else if(x < 0 && y < 0) {
		return 3;
	}
	else { // x >= 0 && y < 0
		return 4;
	}
}


// return true if the Scara is cantilevered
bool FiveBarScaraKinematics::isCantilevered(int mode) const
{
	if(cantL > 0.0 && mode == 1) {
		return true;
	}
	else if(cantR > 0.0 && mode == 2) {
		return true;
	}
	return false;
}

// get angle between -90 and 270 for given origin and destination coordinates
float FiveBarScaraKinematics::getAbsoluteAngle(float xOrig, float yOrig, float xDest, float yDest) const
{
	float length = sqrtf(fsquare(xOrig - xDest) + fsquare(yOrig - yDest));

	float y = fabs(yOrig - yDest);
	float angle = asin(y / length) * 180.0 / M_PI;

	int quad = getQuadrant(xDest-xOrig, yDest-yOrig);

	if(quad == 1) {  // right upper quadrant I
		// nothing to change
	}
	else if(quad == 2) {  // left upper quadrant II
		angle = 180.0 - angle;
	}
	else if(quad == 3) { // left lower quadrant III
		angle += 180.0;
	}
	else if(quad == 4) { // right lower quadrant IV
		angle = 360 - angle;
	}
	return angle;
}

// first circle, second circle. Return the two intersection points
void FiveBarScaraKinematics::getIntersec(float result[], float firstRadius, float secondRadius, float firstX, float firstY,
		float secondX, float secondY) const
{
	float firstRadius2 = fsquare(firstRadius);
	float secondRadius2 = fsquare(secondRadius);

	float distance2 = fsquare(firstX - secondX) + fsquare(firstY - secondY);
	float distance = sqrtf(distance2);

	float delta = 0.25 * sqrtf(
			(distance + firstRadius + secondRadius)
			* (distance + firstRadius - secondRadius)
			* (distance - firstRadius + secondRadius)
			* (-distance + firstRadius + secondRadius)
		);

	// calculate x
	float term1x = (firstX + secondX) / 2;
	float term2x = (secondX - firstX) * (firstRadius2 - secondRadius2) / (2 * distance2);
	float term3x = 2 * (firstY - secondY) / (distance2) * delta;
	float x1 = term1x + term2x + term3x;
	float x2 = term1x + term2x - term3x;

	// calculate y
	float term1y = (firstY + secondY) / 2;
	float term2y = (secondY - firstY)*(firstRadius2 - secondRadius2) / (2 * distance2);
	float term3y = 2 * (firstX - secondX) / distance2 * delta;
	float y1 = term1y + term2y - term3y;
	float y2 = term1y + term2y + term3y;

	result[0] = x1;
	result[1] = y1;
	result[2] = x2;
	result[3] = y2;
}

// return coordinates and theta angles or both possible solutions
// first solution in [0], [1], [2] is the angle which fits to the current workmode
// result: x,y,theta of first point, then x,y,theta of second solution
void FiveBarScaraKinematics::getTheta(float result[], float prox, float distal, float proxX,
		float proxY, float destX, float destY, Arm arm) const
{
	float inter12[4];
	getIntersec(inter12, prox, distal, proxX, proxY, destX, destY);
	float x1 = inter12[0];
	float y1 = inter12[1];
	float x2 = inter12[2];
	float y2 = inter12[3];

	float thetaA = getAbsoluteAngle(proxX, proxY, x1, y1);
	float thetaB = getAbsoluteAngle(proxX, proxY, x2, y2);


	  float proxTurnA = getTurn(proxX, proxY, x1, y1, destX, destY);
	  float proxTurnB = getTurn(proxX, proxY, x2, y2, destX, destY);

	  int use = 0; // 1 for A, 2 for B

	  if(workmode == 1) {
	    if(proxTurnA > 0) use = 1;
	    else if(proxTurnB > 0) use = 2;
	  } else if (workmode == 2) {
	    if(arm == Arm::left) {
	      if(proxTurnA < 0) use = 1;
	      else if(proxTurnB < 0) use = 2;
	    } else {
	      if(proxTurnA > 0) use = 1;
	      else if(proxTurnB > 0) use = 2;
	    }
	  } else if (workmode == 3) {
	    if(arm == Arm::left) {
	      if(proxTurnA > 0) use = 1;
	      else if(proxTurnB > 0) use = 2;
	    } else {
	      if(proxTurnA < 0) use = 1;
	      else if(proxTurnB < 0) use = 2;
	    }
	  } else if (workmode == 4) {
	    if(proxTurnA < 0) use = 1;
	    else if(proxTurnB < 0) use = 2;
	  }

	  if(use == 1)
	  {
	    result[0] = x1;
	    result[1] = y1;
	    result[2] = thetaA;
	    result[3] = x2;
	    result[4] = y2;
	    result[5] = thetaB;
	  }
	  else if(use == 2)
	  {
	    result[0] = x2;
	    result[1] = y2;
	    result[2] = thetaB;
	    result[3] = x1;
	    result[4] = y1;
	    result[5] = thetaA;
	  }
	  else
	  {
	    // fail!! Is that even possible?
	    result[0] = std::numeric_limits<float>::quiet_NaN();
	    result[1] = std::numeric_limits<float>::quiet_NaN();
	    result[2] = std::numeric_limits<float>::quiet_NaN();
	    result[3] = std::numeric_limits<float>::quiet_NaN();
	    result[4] = std::numeric_limits<float>::quiet_NaN();
	    result[5] = std::numeric_limits<float>::quiet_NaN();
	  }

}

// from given point with angle and length, calculate destination
// resultcoords: x, y
void FiveBarScaraKinematics::getXYFromAngle(float resultcoords[], float angle, float length,
		float origX, float origY) const
{
	float xL = length * cos(angle * M_PI / 180.0);
	float yL = length * sin(angle * M_PI / 180.0);

	resultcoords[0] = xL + origX;
	resultcoords[1] = yL + origY;
}

// get forware kinematics: from theta actuators angles, calculate destination coordinates
// optional cantilevered will be added later
// resultcoords: xL, yL, xR, yR, x0, y0
void FiveBarScaraKinematics::getForward(float resultcoords[], float thetaL, float thetaR) const
{
	float coordsL[2];
	getXYFromAngle(coordsL, thetaL, proximalL, xOrigL, yOrigL);
	float xL = coordsL[0];
	float yL = coordsL[1];

	float coordsR[2];
	getXYFromAngle(coordsR, thetaR, proximalR, xOrigR, yOrigR);
	float xR = coordsR[0];
	float yR = coordsR[1];


	  float inter12[4];
	  getIntersec(inter12, distalL, distalR, xL, yL, xR, yR); // two intersection points x,y

	  resultcoords[0] = xL;
	  resultcoords[1] = yL;
	  resultcoords[2] = xR;
	  resultcoords[3] = yR;

	  // Figure out what solution to pick, depending on angle of hotend joints
	  float turnHot0 = getTurn(xL, yL, inter12[0], inter12[1], xR, yR);
	  float turnHot1 = getTurn(xL, yL, inter12[2], inter12[3], xR, yR);

	  float xDst = std::numeric_limits<float>::quiet_NaN();;
	  float yDst = std::numeric_limits<float>::quiet_NaN();;
	  if(workmode >= 1 && workmode <= 4) {
		if(turnHot0 < 0) { // right turn
		  xDst = inter12[0];
		  yDst = inter12[1];
		}
		else if(turnHot1 < 0) {
		  xDst = inter12[2];
		  yDst = inter12[3];
		}
		// Sanity check the elbow joins to make sure it's in the correct work mode
		float tL = getTurn(xOrigL, yOrigL, xL, yL, xDst, yDst);
		float tR = getTurn(xOrigR, yOrigR, xR, yR, xDst, yDst);

		if((workmode == 1 && (tL < 0 || tR < 0)) ||
		   (workmode == 2 && (tL > 0 || tR < 0)) ||
		   (workmode == 3 && (tL < 0 || tR > 0)) ||
		   (workmode == 4 && (tL > 0 || tR > 0))) {
		  xDst = std::numeric_limits<float>::quiet_NaN();
		  yDst = std::numeric_limits<float>::quiet_NaN();
		}

		//fprintf(stderr, "xy %f %f\n", xDst, yDst);
		resultcoords[4] = xDst;
		resultcoords[5] = yDst;
	  } else {
		// work mode 5-8
	  }


}

// 1 - angle - 2 are ordered clockwise. The angle is at the inner/right side, between 2 and 1 clockwise
float FiveBarScaraKinematics::getAngle(float x1, float y1, float xAngle, float yAngle, float x2, float y2) const
{
	float angle1 = getAbsoluteAngle(xAngle, yAngle, x1, y1);
	float angle2 = getAbsoluteAngle(xAngle, yAngle, x2, y2);

	float angle = 0.0;
	if(angle2 < angle1) {
		angle = 360 + angle2 - angle1;
	}
	else {
		angle = angle2 - angle1;
	}

	return angle;
}

// return positive if turn is left (counter-clockwise), negative if turn is right (clockwise)
float FiveBarScaraKinematics::getTurn(float x1, float y1, float xAngle, float yAngle, float x2, float y2) const
{
  return (xAngle-x1)*(y2-y1)-(yAngle-y1)*(x2-x1);
}


bool FiveBarScaraKinematics::isPointInsideDefinedPrintableArea(float x0, float y0) const
{
	float x1 = printArea[0];
	float y1 = printArea[1];
	float x2 = printArea[2];
	float y2 = printArea[3];

	float xmin = min(x1, x2);
	float xmax = max(x1, x2);
	float ymin = min(y1, y2);
	float ymax = max(y1, y2);

	if(x0 >= xmin && x0 <= xmax && y0 >= ymin && y0 <= ymax) {
		return true;
	}
	else {
		return false;
	}
}

bool FiveBarScaraKinematics::constraintsOk(const float coords[]) const
{
	if(!cachedInvalid && coords[0] == cachedX0 && coords[1] == cachedY0) {	// already solved and ok
		return true;
	}

	getInverse(coords);	// xL, yL, thetaL, xR, yR, thetaR, x1, y1 (joint of distals)

	if(cachedInvalid) {
		return false;
    }

	// check theta angles
	float thetaL = cachedThetaL;
	if(actuatorAngleLMin < 0 &&  thetaL > actuatorAngleLMax) thetaL -= 360;
	if(actuatorAngleLMin > thetaL || actuatorAngleLMax < thetaL) {
		cachedInvalid = true;
		return false;
	}
	float thetaR = cachedThetaR;
	if(actuatorAngleRMin < 0 &&  thetaR > actuatorAngleRMax) thetaR -= 360;
	if(actuatorAngleRMin > thetaR || actuatorAngleRMax < thetaR) {
		cachedInvalid = true;
		return false;
	}

	// check constr
	float constr = getAngle(cachedXL, cachedYL, cachedX1, cachedY1, cachedXR, cachedYR);
	if(constrMin > constr || constrMax < constr || std::isnan(constr)) {
		cachedInvalid = true;
		return false;
	}

	// check proxDistal angle L and R
	float angleProxDistL = getAngle(xOrigL, yOrigL, cachedXL, cachedYL, cachedX1, cachedY1);
	if(proxDistLAngleMin > angleProxDistL || proxDistLAngleMax < angleProxDistL  || std::isnan(angleProxDistL)) {
		cachedInvalid = true;
		return false;
	}
	float angleProxDistR = getAngle(xOrigR, yOrigR, cachedXR, cachedYR, cachedX1, cachedY1);
	if(proxDistRAngleMin > angleProxDistR || proxDistRAngleMax < angleProxDistR || std::isnan(angleProxDistR)) {
		cachedInvalid = true;
		return false;
	}

	cachedInvalid = false;
	return true;
}

///////////////////////////// public functions /////////////////////////


// Set the parameters from a M665, M666 or M669 command
// Return true if we changed any parameters. Set 'error' true if there was an error, otherwise leave it alone.
bool FiveBarScaraKinematics::Configure(unsigned int mCode, GCodeBuffer& gb, const StringRef& reply, bool& error) /*override*/
{
	if (mCode == 669)
	{
		// must be defined: X, Y, P, D
		if(!gb.Seen('X') || !(gb.Seen('Y')) || !(gb.Seen('P')) || !(gb.Seen('D'))) {
			error = true;
			return true;
		}

		bool seen = false;
		bool seenNonGeometry = false;


		// parameter X: x origins of actuators
		float paraX[2];
		gb.TryGetFloatArray('X', 2, paraX, reply, seen);
		xOrigL = paraX[0];
		xOrigR = paraX[1];


		// parameter Y: y origins of actuators
		float paraY[2];
		gb.TryGetFloatArray('Y', 2, paraY, reply, seen);
		yOrigL = paraY[0];
		yOrigR = paraY[1];


		// workmode, default 1 is left buckled, right bulged
		if(gb.Seen('L')) {
			long wm = 0L;
			gb.TryGetIValue('L', wm, seen);
			workmode = (int) wm;
			if(!(workmode == 1 || workmode == 2 || workmode == 4)) {
				error = true;
				return true;
			}
		}
		else {
			workmode = 1;	// default
		}


		// proximal arm lenghts
		float proximalLengths[2];
		gb.TryGetFloatArray('P', 2, proximalLengths, reply, seen);
		proximalL = proximalLengths[0];
		proximalR = proximalLengths[1];


		// distal arm lengths and optional lengths of cantilevered arm
		bool dseen = false;
		float distalLengths[4];
		if(!gb.TryGetFloatArray('D', 4, distalLengths, reply, dseen) && dseen) {
			distalL = distalLengths[0];
			distalR = distalLengths[1];
			cantL = distalLengths[2];
			cantR = distalLengths[3];
			seen = true;
		} else {
			dseen = false;
			if(!gb.TryGetFloatArray('D', 2, distalLengths, reply, dseen) && dseen) {
				distalL = distalLengths[0];
				distalR = distalLengths[1];
				cantL = 0.0;
				cantR = 0.0;
				seen = true;
			}
		}

		// angle of the actuator in the home position
		if(gb.Seen('B')) {
			float homingAngles[2];
			gb.TryGetFloatArray('B', 2, homingAngles, reply, seen);
			homingAngleL = homingAngles[0];
			homingAngleR = homingAngles[1];
		}
		else {
			if(workmode == 1) {
				homingAngleL = 20.0;	// default
				homingAngleR = 10.0;	// default
			}
			else if(workmode == 2) {
				homingAngleL = 110.0;	// default
				homingAngleR = 20.0;	// default
			}
			else if(workmode == 4) {
				homingAngleL = 110.0;	// default
				homingAngleR = 100.0;	// default
			}
			else {
				homingAngleL = 90.0;	// default
				homingAngleR = 90.0;	// default
			}
		}


		// angle constraints: between distals and between proximal and distal
		if(gb.Seen('A')) {
			float angles[6];
			gb.TryGetFloatArray('A', 6, angles, reply, seen);
			constrMin = angles[0];
			constrMax = angles[1];
			proxDistLAngleMin = angles[2];
			proxDistLAngleMax = angles[3];
			proxDistRAngleMin = angles[4];
			proxDistRAngleMax = angles[5];
		}
		else  {
			constrMin = 15.0;
			constrMax = 165.0;
			proxDistLAngleMin = 0.0;
			proxDistLAngleMax = 360.0;
			proxDistRAngleMin = 0.0;
			proxDistRAngleMax = 360.0;
		}


		// actuator angle constraints
		if(gb.Seen('C')) {
			float angles[4];
			gb.TryGetFloatArray('C', 4, angles, reply, seen);
			actuatorAngleLMin = angles[0];
			actuatorAngleLMax = angles[1];
			actuatorAngleRMin = angles[2];
			actuatorAngleRMax = angles[3];
		}
		else {
			actuatorAngleLMin = 10.0;
			actuatorAngleLMax = 170.0;
			actuatorAngleRMin = 10.0;
			actuatorAngleRMax = 170.0;
		}

		// optional rectangle definition of a print area. Must match the workmode reachable area
		if(gb.Seen('Z')) {
			float coordinates[4];
			gb.TryGetFloatArray('Z', 4, coordinates, reply, seen);
			for(int i=0; i < 4; i++) {
				printArea[i] = coordinates[i];
			}
			printAreaDefined = true;
		}
		else {
			printAreaDefined = false;
		}


		gb.TryGetFValue('S', segmentsPerSecond, seenNonGeometry);		// value defined in Kinematics.h
		gb.TryGetFValue('T', minSegmentLength, seenNonGeometry);		// value defined in Kinematics.h


		if (seen || seenNonGeometry)
		{
			Recalc();
		}
		else if (!gb.Seen('K'))
		{
			reply.printf("Kinematics is FiveBarScara, documented in https://duet3d.dozuki.com/Guide/Five+Bar+Parallel+SCARA/24?lang=en");
		}

		return seen;
	}
	else
	{

		return ZLeadscrewKinematics::Configure(mCode, gb, reply, error);
	}
}

//bool FiveBarScaraKinematics::LimitPosition(float position[], size_t numAxes, AxesBitmap axesHomed, bool isCoordinated) const override;
// Limit the Cartesian position that the user wants to move to, returning true if any coordinates were changed
bool       FiveBarScaraKinematics::LimitPosition(float finalCoords[], float * null initialCoords, size_t numVisibleAxes, AxesBitmap axesHomed, bool isCoordinated, bool applyM208Limits) const
//OLD bool FiveBarScaraKinematics::LimitPosition(float coords[], size_t numVisibleAxes, AxesBitmap axesHomed, bool isCoordinated, bool applyM208Limits) const
{
	// TODO
	// First limit all axes according to M208
	const bool m208Limited = Kinematics::LimitPosition(finalCoords, initialCoords, numVisibleAxes, axesHomed, isCoordinated, applyM208Limits);

	if(!constraintsOk(finalCoords)) {
		cachedInvalid = true;  // PB: A bit of a hack to place this here...
		return true;
	}

	return m208Limited;
}


// Convert Cartesian coordinates to motor coordinates, returning true if successful
// In the following, theta is the proximal arm angle relative to the X axis, psi is the distal arm angle relative to the proximal arm
bool FiveBarScaraKinematics::CartesianToMotorSteps(const float machinePos[], const float stepsPerMm[], size_t numVisibleAxes, size_t numTotalAxes, int32_t motorPos[], bool isCoordinated) const
{
	float coords[2] = {machinePos[0], machinePos[1]};

	getInverse(coords);

	if(!constraintsOk(coords))
		return false;

	motorPos[X_AXIS] = lrintf(cachedThetaL * stepsPerMm[X_AXIS]);
	motorPos[Y_AXIS] = lrintf(cachedThetaR * stepsPerMm[Y_AXIS]);
	motorPos[Z_AXIS] = lrintf(machinePos[Z_AXIS] * stepsPerMm[Z_AXIS]);

	// Transform any additional axes linearly
	for (size_t axis = XYZ_AXES; axis < numVisibleAxes; ++axis)
	{
		motorPos[axis] = lrintf(machinePos[axis] * stepsPerMm[axis]);
	}

	return true;
}

// Convert motor coordinates to machine coordinates. Used after homing and after individual motor moves.
// For Scara, the X and Y components of stepsPerMm are actually steps per degree angle.
void FiveBarScaraKinematics::MotorStepsToCartesian(const int32_t motorPos[], const float stepsPerMm[], size_t numVisibleAxes, size_t numTotalAxes, float machinePos[]) const
{

	float thetaL = ((float)motorPos[X_AXIS]/stepsPerMm[X_AXIS]);
	float thetaR = ((float)motorPos[Y_AXIS]/stepsPerMm[Y_AXIS]);

	float x_0 = -1.0;
	float y_0 = -1.0;

	float resultcoords[6];
	getForward(resultcoords, thetaL, thetaR);
	float x1 = resultcoords[4];
	float y1 = resultcoords[5];

	if(isCantilevered(1)) {	// left distal is prolongued
		float xL = resultcoords[0];
		float yL = resultcoords[1];

		// calculate cantilever from psiL
		float psiL = getAbsoluteAngle(xL, yL, x1, y1);
		float dest[2];
		getXYFromAngle(dest, psiL, cantL, x1, y1);
		x_0 = dest[0];
		y_0 = dest[1];
	}
	else if(isCantilevered(2)) { // right distal is prolongued
		float xR = resultcoords[2];
		float yR = resultcoords[3];

		// now calculate cantilever from psiR
		float psiR = getAbsoluteAngle(xR, yR, x1, y1);
		float dest[2];
		getXYFromAngle(dest, psiR, cantR, x1, y1);
		x_0 = dest[0];
		y_0 = dest[1];
	}
	else {
		x_0 = x1;
		y_0 = y1;
	}


    machinePos[X_AXIS] = x_0;
    machinePos[Y_AXIS] = y_0;

    machinePos[Z_AXIS] = (float) motorPos[Z_AXIS] / stepsPerMm[Z_AXIS];

	// Convert any additional axes linearly
	for (size_t drive = XYZ_AXES; drive < numVisibleAxes; ++drive)
	{
		machinePos[drive] = motorPos[drive]/stepsPerMm[drive];
	}

}

// Return true if the specified XY position is reachable by the print head reference point.
bool FiveBarScaraKinematics::IsReachable(float x, float y, bool isCoordinated) const
{
	// Check the M208 limits first
	float coords[2] = {x, y};
	if (Kinematics::LimitPosition(coords, NULL, 2, LowestNBits<AxesBitmap>(2), isCoordinated, true))
	{
		return false;
	}

	if(printAreaDefined) {
		if(isPointInsideDefinedPrintableArea(x, y)) {
			return true;
		}
	}
	else {
		float coords[2] = {x, y};
		if(constraintsOk(coords)) {
			return true;
		}
	}

	return false;
}

// Return the initial Cartesian coordinates we assume after switching to this kinematics
void FiveBarScaraKinematics::GetAssumedInitialPosition(size_t numAxes, float positions[]) const
{
	// x and y are unknown. The stepper angles shall be near the homing endstops, so some
	// iterations turning left and right will find the endstop positions. The angles and
	// coordinates are clearly defined then.

	positions[X_AXIS] = 0.0;
	positions[Y_AXIS] = 0.0;
	for (size_t i = Z_AXIS; i < numAxes; ++i)
	{
		positions[i] = 0.0;
	}
}

// Return the axes that we can assume are homed after executing a G92 command to set the specified axis coordinates
AxesBitmap FiveBarScaraKinematics::AxesAssumedHomed(AxesBitmap g92Axes) const
{
	// If both X and Y have been specified then we know the positions of both arm motors, otherwise we don't
	const AxesBitmap xyAxes = MakeBitmap<AxesBitmap>(X_AXIS) | MakeBitmap<AxesBitmap>(Y_AXIS);
	if ((g92Axes & xyAxes) != xyAxes)
	{
		g92Axes &= ~xyAxes;
	}
	return g92Axes;
}

// Return the set of axes that must be homed prior to regular movement of the specified axes
AxesBitmap FiveBarScaraKinematics::MustBeHomedAxes(AxesBitmap axesMoving, bool disallowMovesBeforeHoming) const
{
	constexpr AxesBitmap xyzAxes = MakeBitmap<AxesBitmap>(X_AXIS) |  MakeBitmap<AxesBitmap>(Y_AXIS) |  MakeBitmap<AxesBitmap>(Z_AXIS);
	if ((axesMoving & xyzAxes) != 0)
	{
		axesMoving |= xyzAxes;
	}
	return axesMoving;
}

size_t FiveBarScaraKinematics::NumHomingButtons(size_t numVisibleAxes) const
{
	//const MassStorage *storage = reprap.GetPlatform().GetMassStorage();
	// TODO, SYS_DIR is not defined anymore
	/*if (!storage->FileExists(SYS_DIR, Home5BarScaraFileName))
	{
		return 0;
	}*/
	return numVisibleAxes;
}

// This function is called when a request is made to home the axes in 'toBeHomed' and the axes in 'alreadyHomed' have already been homed.
// If we can proceed with homing some axes, return the name of the homing file to be called.
// If we can't proceed because other axes need to be homed first, return nullptr and pass those axes back in 'mustBeHomedFirst'.
AxesBitmap FiveBarScaraKinematics::GetHomingFileName(AxesBitmap toBeHomed, AxesBitmap alreadyHomed, size_t numVisibleAxes, const StringRef& filename) const
{
// Ask the base class which homing file we should call first
	AxesBitmap ret = Kinematics::GetHomingFileName(toBeHomed, alreadyHomed, numVisibleAxes, filename);
	filename.copy(Home5BarScaraFileName);

	return ret;
}

// This function is called from the step ISR when an endstop switch is triggered during homing.
// Return true if the entire homing move should be terminated, false if only the motor associated with the endstop switch should be stopped.
bool FiveBarScaraKinematics::QueryTerminateHomingMove(size_t axis) const
{
	return false; //(axis == X_AXIS  || axis == Y_AXIS);
}

// This function is called from the step ISR when an endstop switch is triggered during homing after stopping just one motor or all motors.
// Take the action needed to define the current position, normally by calling dda.SetDriveCoordinate() and return false.
void FiveBarScaraKinematics::OnHomingSwitchTriggered(size_t axis, bool highEnd, const float stepsPerMm[], DDA& dda) const
{
	switch (axis)
	{
	case X_AXIS:	// proximal joint homing switch
		dda.SetDriveCoordinate(lrintf(homingAngleL * stepsPerMm[axis]), axis);
		break;

	case Y_AXIS:	// distal joint homing switch
		dda.SetDriveCoordinate(lrintf(homingAngleR * stepsPerMm[axis]), axis);
		break;

	case Z_AXIS:	// Z axis homing switch
		{
			const float hitPoint = ((highEnd) ? reprap.GetPlatform().AxisMaximum(axis) : reprap.GetPlatform().AxisMinimum(axis));
			dda.SetDriveCoordinate(lrintf(hitPoint * stepsPerMm[axis]), axis);
		}
		break;

	default:		// Additional axis
		{
			const float hitPoint = (highEnd) ? reprap.GetPlatform().AxisMaximum(axis) : reprap.GetPlatform().AxisMinimum(axis);
			dda.SetDriveCoordinate(lrintf(hitPoint * stepsPerMm[axis]), axis);
		}
		break;
	}
}

// Limit the speed and acceleration of a move to values that the mechanics can handle.
// The speeds in Cartesian space have already been limited.
void FiveBarScaraKinematics::LimitSpeedAndAcceleration(DDA& dda, const float *normalisedDirectionVector, size_t numVisibleAxes, bool continuousRotationShortcut) const
//void FiveBarScaraKinematics::LimitSpeedAndAcceleration(DDA& dda, const float *normalisedDirectionVector, size_t numVisibleAxes) const
{
	// For now we limit the speed in the XY plane to the lower of the X and Y maximum speeds, and similarly for the acceleration.
	// Limiting the angular rates of the arms would be better.
	const float xyFactor = sqrtf(fsquare(normalisedDirectionVector[X_AXIS]) + fsquare(normalisedDirectionVector[Y_AXIS]));
	if (xyFactor > 0.01)
	{
		const Platform& platform = reprap.GetPlatform();
		const float maxSpeed = min<float>(platform.MaxFeedrate(X_AXIS), platform.MaxFeedrate(Y_AXIS));
		const float maxAcceleration = min<float>(platform.Acceleration(X_AXIS), platform.Acceleration(Y_AXIS));
		dda.LimitSpeedAndAcceleration(maxSpeed/xyFactor, maxAcceleration/xyFactor);
	}
}

// Return true if the specified axis is a continuous rotation axis
bool FiveBarScaraKinematics::IsContinuousRotationAxis(size_t axis) const
{
	return axis < 2; //  && supportsContinuousRotation[axis];
	//return false;
}

AxesBitmap FiveBarScaraKinematics::GetLinearAxes() const
{
	return MakeBitmap<AxesBitmap>(Z_AXIS);
}

AxesBitmap FiveBarScaraKinematics::GetConnectedAxes(size_t axis) const
{
	if(axis == X_AXIS || axis == Y_AXIS) {
		AxesBitmap bm = MakeBitmap<AxesBitmap>(X_AXIS);
		SetBit(bm, Y_AXIS);
		return bm;
	}
	else
		return MakeBitmap<AxesBitmap>(axis);


}

// Recalculate the derived parameters
void FiveBarScaraKinematics::Recalc()
{
	cachedX0 = std::numeric_limits<float>::quiet_NaN(); // make sure that the cached values won't match any coordinates}
	cachedY0 = std::numeric_limits<float>::quiet_NaN(); // make sure that the cached values won't match any coordinates}
	cachedInvalid = true;
}
// End