#include "Common.h"
#include "QuadControl.h"

#include "Utility/SimpleConfig.h"

#include "Utility/StringUtils.h"
#include "Trajectory.h"
#include "BaseController.h"
#include "Math/Mat3x3F.h"

#ifdef __PX4_NUTTX
#include <systemlib/param/param.h>
#endif

// Roswitha Remling Jan 27, 2019 submission

void QuadControl::Init()
{
    BaseController::Init();
    
    // variables needed for integral control
    integratedAltitudeError = 0;
    
#ifndef __PX4_NUTTX
    // Load params from simulator parameter system
    ParamsHandle config = SimpleConfig::GetInstance();
    
    // Load parameters (default to 0)
    kpPosXY = config->Get(_config+".kpPosXY", 0);
    kpPosZ = config->Get(_config + ".kpPosZ", 0);
    KiPosZ = config->Get(_config + ".KiPosZ", 0);
    
    kpVelXY = config->Get(_config + ".kpVelXY", 0);
    kpVelZ = config->Get(_config + ".kpVelZ", 0);
    
    kpBank = config->Get(_config + ".kpBank", 0);
    kpYaw = config->Get(_config + ".kpYaw", 0);
    
    kpPQR = config->Get(_config + ".kpPQR", V3F());
    
    maxDescentRate = config->Get(_config + ".maxDescentRate", 100);
    maxAscentRate = config->Get(_config + ".maxAscentRate", 100);
    maxSpeedXY = config->Get(_config + ".maxSpeedXY", 100);
    maxAccelXY = config->Get(_config + ".maxHorizAccel", 100);
    
    maxTiltAngle = config->Get(_config + ".maxTiltAngle", 100);
    
    minMotorThrust = config->Get(_config + ".minMotorThrust", 0);
    maxMotorThrust = config->Get(_config + ".maxMotorThrust", 100);
#else
    // load params from PX4 parameter system
    //TODO
    param_get(param_find("MC_PITCH_P"), &Kp_bank);
    param_get(param_find("MC_YAW_P"), &Kp_yaw);
#endif
}

VehicleCommand QuadControl::GenerateMotorCommands(float collThrustCmd, V3F momentCmd)
{
    // Convert a desired 3-axis moment and collective thrust command to
    //   individual motor thrust commands
    // INPUTS:
    //   collThrustCmd: desired collective thrust [N]
    //   momentCmd: desired rotation moment about each axis [N m]
    // OUTPUT:
    //   set class member variable cmd (class variable for graphing) where
    //   cmd.desiredThrustsN[0..3]: motor commands, in [N]
    
    // HINTS:
    // - you can access parts of momentCmd via e.g. momentCmd.x
    // You'll need the arm length parameter L, and the drag/thrust ratio kappa
    
    ////////////////////////////// BEGIN STUDENT CODE ///////////////////////////
    
    // from P3L4S7, exept there:
    //     - matrix is for motors cw, here order is in rows **
    //         thus had to re-resolve the matrix, switching omega 3 and 4
    //     - L = diagonal rotor distance and here we have it as "arm length" *
    
    float p = momentCmd.x * sqrt(2.f) / L ;  //  *
    float q = momentCmd.y * sqrt(2.f) / L ;  //  *
    float r = -momentCmd.z / kappa;
    
    cmd.desiredThrustsN[0] = (collThrustCmd + p + q + r) / 4.f; // front left
    cmd.desiredThrustsN[1] = (collThrustCmd - p + q - r) / 4.f; // front right **
    cmd.desiredThrustsN[2] = (collThrustCmd + p - q - r) / 4.f; // rear left **
    cmd.desiredThrustsN[3] = (collThrustCmd - p - q + r) / 4.f; // rear right **
    
    /////////////////////////////// END STUDENT CODE ////////////////////////////
    
    return cmd;
}

V3F QuadControl::BodyRateControl(V3F pqrCmd, V3F pqr)
{
    // Calculate a desired 3-axis moment given a desired and current body rate
    // INPUTS:
    //   pqrCmd: desired body rates [rad/s]
    //   pqr: current or estimated body rates [rad/s]
    // OUTPUT:
    //   return a V3F containing the desired moments for each of the 3 axes
    
    // HINTS:
    //  - you can use V3Fs just like scalars: V3F a(1,1,1), b(2,3,4), c; c=a-b;
    //  - you'll need parameters for moments of inertia Ixx, Iyy, Izz
    //  - you'll also need the gain parameter kpPQR (it's a V3F)
    
    V3F momentCmd;
    
    ////////////////////////////// BEGIN STUDENT CODE ///////////////////////////
    
    // P3L4S21 adding the tuning paramaters with Inertia_Params
    
    V3F Inertia_Params(Ixx,Iyy,Izz);
    momentCmd = Inertia_Params * kpPQR * (pqrCmd - pqr);
    
    /////////////////////////////// END STUDENT CODE ////////////////////////////
    
    return momentCmd;
}

// returns a desired roll and pitch rate
V3F QuadControl::RollPitchControl(V3F accelCmd, Quaternion<float> attitude, float collThrustCmd)
{
    // Calculate a desired pitch and roll angle rates based on a desired global
    //   lateral acceleration, the current attitude of the quad, and desired
    //   collective thrust command
    // INPUTS:
    //   accelCmd: desired acceleration in global XY coordinates [m/s2]
    //   attitude: current or estimated attitude of the vehicle
    //   collThrustCmd: desired collective thrust of the quad [N]
    // OUTPUT:
    //   return a V3F containing the desired pitch and roll rates. The Z
    //     element of the V3F should be left at its default value (0)
    
    // HINTS:
    //  - we already provide rotation matrix R: to get element R[1,2] (python) use R(1,2) (C++)
    //  - you'll need the roll/pitch gain kpBank
    //  - collThrustCmd is a force in Newtons! You'll likely want to convert it to acceleration first
    
    V3F pqrCmd(0,0,0); // ensuring z is zero, should be default
    Mat3x3F R = attitude.RotationMatrix_IwrtB();
    
    ////////////////////////////// BEGIN STUDENT CODE ///////////////////////////
    // from P3L4S18
    float collAccCmd = - collThrustCmd / mass; // get -z acceleration
    
    // calculate commanded dot with kpBank aand x/y error (P3L4S18)
    float b_x_p = kpBank * ( accelCmd.x / collAccCmd - R(0,2) );
    float b_y_p = kpBank * ( accelCmd.y / collAccCmd - R(1,2) );
    
    // convert with matrix R provided
    pqrCmd.x = ( R(1,0) * b_x_p - R(0,0) * b_y_p) / R(2,2);
    pqrCmd.y = ( R(1,1) * b_x_p - R(0,1) * b_y_p) / R(2,2);
    
    /////////////////////////////// END STUDENT CODE ////////////////////////////
    
    return pqrCmd;
}

float QuadControl::AltitudeControl(float posZCmd, float velZCmd, float posZ, float velZ, Quaternion<float> attitude, float accelZCmd, float dt)
{
    // Calculate desired quad thrust based on altitude setpoint, actual altitude,
    //   vertical velocity setpoint, actual vertical velocity, and a vertical
    //   acceleration feed-forward command
    // INPUTS:
    //   posZCmd, velZCmd: desired vertical position and velocity in NED [m]
    //   posZ, velZ: current vertical position and velocity in NED [m]
    //   accelZCmd: feed-forward vertical acceleration in NED [m/s2]
    //   dt: the time step of the measurements [seconds]
    // OUTPUT:
    //   return a collective thrust command in [N]
    
    // HINTS:
    //  - we already provide rotation matrix R: to get element R[1,2] (python) use R(1,2) (C++)
    //  - you'll need the gain parameters kpPosZ and kpVelZ
    //  - maxAscentRate and maxDescentRate are maximum vertical speeds. Note they're both >=0!
    //  - make sure to return a force, not an acceleration
    //  - remember that for an upright quad in NED, thrust should be HIGHER if the desired Z acceleration is LOWER
    
    Mat3x3F R = attitude.RotationMatrix_IwrtB();
    float thrust = 0;
    
    ////////////////////////////// BEGIN STUDENT CODE ///////////////////////////
    // a bit unorthodox, but keeping the lines a bit shorter and
    // straight from my Python in P3L4S21 and P3L2S19-20
    
    float u1_bar = kpPosZ * (posZCmd - posZ) + accelZCmd;   // position and acc terms
    u1_bar = u1_bar + kpVelZ * (velZCmd - velZ);            // add velocity term
    
    // add Integrated postition error over dt for meas time
    // for Scenario 4
    // may have been influenced by github.com/darienmt/FCND-Term1-P3-3D-Quadrotor-Controller.
    u1_bar = u1_bar + KiPosZ * integratedAltitudeError * dt * (posZCmd - posZ);
    
    // need force
    thrust = - mass * (u1_bar - CONST_GRAVITY) / R(2,2);
    
    /////////////////////////////// END STUDENT CODE ////////////////////////////
    
    return thrust;
}

// returns a desired acceleration in global frame
V3F QuadControl::LateralPositionControl(V3F posCmd, V3F velCmd, V3F pos, V3F vel, V3F accelCmdFF)
{
    // Calculate a desired horizontal acceleration based on
    //  desired lateral position/velocity/acceleration and current pose
    // INPUTS:
    //   posCmd: desired position, in NED [m]
    //   velCmd: desired velocity, in NED [m/s]
    //   pos: current position, NED [m]
    //   vel: current velocity, NED [m/s]
    //   accelCmdFF: feed-forward acceleration, NED [m/s2]
    // OUTPUT:
    //   return a V3F with desired horizontal accelerations.
    //     the Z component should be 0
    // HINTS:
    //  - use the gain parameters kpPosXY and kpVelXY
    //  - make sure you limit the maximum horizontal velocity and acceleration
    //    to maxSpeedXY and maxAccelXY
    
    // make sure we don't have any incoming z-component
    accelCmdFF.z = 0;
    velCmd.z = 0;
    posCmd.z = pos.z;
    
    // we initialize the returned desired acceleration to the feed-forward value.
    // Make sure to _add_, not simply replace, the result of your controller
    // to this variable
    V3F accelCmd = accelCmdFF;
    
    ////////////////////////////// BEGIN STUDENT CODE ///////////////////////////
    
    // first check commanded velocity to be within allowable range
    // assuming actual velocity is checked elsewhere
    // code influenced by github.com/darienmt/FCND-Term1-P3-3D-Quadrotor-Controller
    
    V3F limVelCmd;
    if (velCmd.mag() > maxSpeedXY) {
        limVelCmd = limVelCmd.norm() * maxSpeedXY;
        limVelCmd.z = 0;  // just for good measure in case their are rounding errors
    }
    else {
        limVelCmd = velCmd;
    }
    // vector addition should be shortest (seems to handle scalars just fine
    accelCmd =  accelCmd + kpPosXY * (posCmd - pos) + kpVelXY * (limVelCmd - vel);
    
    // verify new commanded velocity to be within allowable range
    // code influenced by same source as 2 paragraphs above
    if (accelCmd.mag() > maxAccelXY) {
        accelCmd = accelCmd.norm() * maxAccelXY;
        accelCmd.z = 0; // just for good measure in case their are rounding errors
    }
    
    /////////////////////////////// END STUDENT CODE ////////////////////////////
    
    return accelCmd;
}

// returns desired yaw rate
float QuadControl::YawControl(float yawCmd, float yaw)
{
    // Calculate a desired yaw rate to control yaw to yawCmd
    // INPUTS:
    //   yawCmd: commanded yaw [rad]
    //   yaw: current yaw [rad]
    // OUTPUT:
    //   return a desired yaw rate [rad/s]
    // HINTS:
    //  - use fmodf(foo,b) to unwrap a radian angle measure float foo to range [0,b].
    //  - use the yaw control gain parameter kpYaw
    
    float yawRateCmd=0;
    ////////////////////////////// BEGIN STUDENT CODE ///////////////////////////
    // based on P3L4S21, exercise 5.2
    // assuming yawCmd and yaw were fmodf-ed before being passed along
    // fmodf usage influenced by github.com/darienmt/FCND-Term1-P3-3D-Quadrotor-Controller
    yawRateCmd = kpYaw * fmodf((yawCmd - yaw), 2. * F_PI);
    
    /////////////////////////////// END STUDENT CODE ////////////////////////////
    
    return yawRateCmd;
    
}

VehicleCommand QuadControl::RunControl(float dt, float simTime)
{
    curTrajPoint = GetNextTrajectoryPoint(simTime);
    
    float collThrustCmd = AltitudeControl(curTrajPoint.position.z, curTrajPoint.velocity.z, estPos.z, estVel.z, estAtt, curTrajPoint.accel.z, dt);
    
    // reserve some thrust margin for angle control
    float thrustMargin = .1f*(maxMotorThrust - minMotorThrust);
    collThrustCmd = CONSTRAIN(collThrustCmd, (minMotorThrust+ thrustMargin)*4.f, (maxMotorThrust-thrustMargin)*4.f);
    
    V3F desAcc = LateralPositionControl(curTrajPoint.position, curTrajPoint.velocity, estPos, estVel, curTrajPoint.accel);
    
    V3F desOmega = RollPitchControl(desAcc, estAtt, collThrustCmd);
    desOmega.z = YawControl(curTrajPoint.attitude.Yaw(), estAtt.Yaw());
    
    V3F desMoment = BodyRateControl(desOmega, estOmega);
    
    return GenerateMotorCommands(collThrustCmd, desMoment);
}

