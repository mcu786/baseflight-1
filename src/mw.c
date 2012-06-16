#include "board.h"
#include "mw.h"

// March  2012     V2.0

int16_t debug1, debug2, debug3, debug4;
uint8_t buzzerState = 0;
uint8_t toggleBeep = 0;
uint32_t currentTime = 0;
uint32_t previousTime = 0;
uint16_t cycleTime = 0;         // this is the number in micro second to achieve a full loop, it can differ a little and is taken into account in the PID loop
uint8_t GPSModeHome = 0;        // if GPS RTH is activated
uint8_t GPSModeHold = 0;        // if GPS PH is activated
uint8_t headFreeMode = 0;       // if head free mode is a activated
uint8_t passThruMode = 0;       // if passthrough mode is activated
int16_t headFreeModeHold;

int16_t annex650_overrun_count = 0;
uint8_t armed = 0;
uint8_t vbat;                   // battery voltage in 0.1V steps

int16_t failsafeCnt = 0;
int16_t failsafeEvents = 0;
int16_t rcData[8] = { 1500, 1500, 1500, 1500, 1500, 1500, 1500, 1500 }; // interval [1000;2000]
int16_t rcCommand[4];           // interval [1000;2000] for THROTTLE and [-500;+500] for ROLL/PITCH/YAW 
int16_t lookupPitchRollRC[6];   // lookup table for expo & RC rate PITCH+ROLL
int16_t lookupThrottleRC[11];   // lookup table for expo & mid THROTTLE
rcReadRawDataPtr rcReadRawFunc = NULL;  // receive data from default (pwm/ppm) or additional (spek/sbus/?? receiver drivers)

uint8_t dynP8[3], dynI8[3], dynD8[3];
uint8_t rcOptions[CHECKBOXITEMS];
uint8_t okToArm = 0;
uint8_t accMode = 0;            // if level mode is a activated
uint8_t magMode = 0;            // if compass heading hold is a activated
uint8_t baroMode = 0;           // if altitude hold is activated

int16_t axisPID[3];

// **********************
// GPS
// **********************
int32_t GPS_coord[2];
int32_t GPS_home[2];
int32_t GPS_hold[2];
uint8_t GPS_fix, GPS_fix_home = 0;
uint8_t GPS_numSat;
uint16_t GPS_distanceToHome, GPS_distanceToHold;        // distance to home or hold point in meters
int16_t GPS_directionToHome, GPS_directionToHold;       // direction to home or hol point in degrees
uint16_t GPS_altitude, GPS_speed;       // altitude in 0.1m and speed in 0.1m/s
uint8_t GPS_update = 0;         // it's a binary toogle to distinct a GPS position update
int16_t GPS_angle[2] = { 0, 0 };        // it's the angles that must be applied for GPS correction
uint16_t GPS_ground_course = 0; // degrees*10
uint8_t GPS_Present = 0;        // Checksum from Gps serial
uint8_t GPS_Enable = 0;
int16_t nav[2];
int8_t nav_mode = NAV_MODE_NONE;        // Navigation mode

//Automatic ACC Offset Calibration
// **********************
uint16_t InflightcalibratingA = 0;
int16_t AccInflightCalibrationArmed;
uint16_t AccInflightCalibrationMeasurementDone = 0;
uint16_t AccInflightCalibrationSavetoEEProm = 0;
uint16_t AccInflightCalibrationActive = 0;

// Battery monitoring stuff
uint8_t batteryCellCount = 3;   // cell count
uint16_t batteryWarningVoltage; // annoying buzzer after this one, battery ready to be dead

void blinkLED(uint8_t num, uint8_t wait, uint8_t repeat)
{
    uint8_t i, r;

    for (r = 0; r < repeat; r++) {
        for (i = 0; i < num; i++) {
            LED0_TOGGLE;        // switch LEDPIN state
            BEEP_ON;
            delay(wait);
            BEEP_OFF;
        }
        delay(60);
    }
}

#define BREAKPOINT 1500

// this code is executed at each loop and won't interfere with control loop if it lasts less than 650 microseconds
void annexCode(void)
{
    static uint32_t calibratedAccTime;
    uint16_t tmp, tmp2;
    static uint8_t vbatTimer = 0;
    static uint8_t buzzerFreq;  //delay between buzzer ring
    uint8_t axis, prop1, prop2;
    static uint8_t ind = 0;
    uint16_t vbatRaw = 0;
    static uint16_t vbatRawArray[8];
    uint8_t i;

    // PITCH & ROLL only dynamic PID adjustemnt,  depending on throttle value
    if (rcData[THROTTLE] < BREAKPOINT) {
        prop2 = 100;
    } else {
        if (rcData[THROTTLE] < 2000) {
            prop2 = 100 - (uint16_t) cfg.dynThrPID * (rcData[THROTTLE] - BREAKPOINT) / (2000 - BREAKPOINT);
        } else {
            prop2 = 100 - cfg.dynThrPID;
        }
    }

    for (axis = 0; axis < 3; axis++) {
        tmp = min(abs(rcData[axis] - cfg.midrc), 500);
        if (axis != 2) {        // ROLL & PITCH
            if (cfg.deadband) {
                if (tmp > cfg.deadband) {
                    tmp -= cfg.deadband;
                } else {
                    tmp = 0;
                }
            }

            tmp2 = tmp / 100;
            rcCommand[axis] = lookupPitchRollRC[tmp2] + (tmp - tmp2 * 100) * (lookupPitchRollRC[tmp2 + 1] - lookupPitchRollRC[tmp2]) / 100;
            prop1 = 100 - (uint16_t) cfg.rollPitchRate * tmp / 500;
            prop1 = (uint16_t) prop1 *prop2 / 100;
        } else {                // YAW
            if (cfg.yawdeadband) {
                if (tmp > cfg.yawdeadband) {
                    tmp -= cfg.yawdeadband;
                } else {
                    tmp = 0;
                }
            }
            rcCommand[axis] = tmp;
            prop1 = 100 - (uint16_t) cfg.yawRate * tmp / 500;
        }
        dynP8[axis] = (uint16_t) cfg.P8[axis] * prop1 / 100;
        dynD8[axis] = (uint16_t) cfg.D8[axis] * prop1 / 100;
        if (rcData[axis] < cfg.midrc)
            rcCommand[axis] = -rcCommand[axis];
    }

    tmp = constrain(rcData[THROTTLE], cfg.mincheck, 2000);
    tmp = (uint32_t) (tmp - cfg.mincheck) * 1000 / (2000 - cfg.mincheck);       // [MINCHECK;2000] -> [0;1000]
    tmp2 = tmp / 100;
    rcCommand[THROTTLE] = lookupThrottleRC[tmp2] + (tmp - tmp2 * 100) * (lookupThrottleRC[tmp2 + 1] - lookupThrottleRC[tmp2]) / 100;    // [0;1000] -> expo -> [MINTHROTTLE;MAXTHROTTLE]

    if (headFreeMode) {
        float radDiff = (heading - headFreeModeHold) * M_PI / 180.0f;
        float cosDiff = cosf(radDiff);
        float sinDiff = sinf(radDiff);
        int16_t rcCommand_PITCH = rcCommand[PITCH] * cosDiff + rcCommand[ROLL] * sinDiff;
        rcCommand[ROLL] = rcCommand[ROLL] * cosDiff - rcCommand[PITCH] * sinDiff;
        rcCommand[PITCH] = rcCommand_PITCH;
    }

    if (feature(FEATURE_VBAT)) {
        if (!(++vbatTimer % VBATFREQ)) {
            vbatRawArray[(ind++) % 8] = adcGetBattery();
            for (i = 0; i < 8; i++)
                vbatRaw += vbatRawArray[i];
            vbat = batteryAdcToVoltage(vbatRaw / 8);
        }
        if (rcOptions[BOXBEEPERON]) {   // unconditional beeper on via AUXn switch 
            buzzerFreq = 7;
        } else if ((vbat > batteryWarningVoltage) || (vbat < cfg.vbatmincellvoltage)) { //VBAT ok, buzzer off
            buzzerFreq = 0;
            buzzerState = 0;
            BEEP_OFF;
        } else
            buzzerFreq = 4;     // low battery
    }

    buzzer(buzzerFreq);         // external buzzer routine that handles buzzer events globally now

    if ((calibratingA > 0 && sensors(SENSOR_ACC)) || (calibratingG > 0)) {      // Calibration phasis
        LED0_TOGGLE;
    } else {
        if (calibratedACC == 1) {
            LED0_OFF;
        }
        if (armed) {
            LED0_ON;
        }
    }

#ifdef LEDRING
    if (feature(FEATURE_LED_RING)) {
        static uint32_t LEDTime;
        if (currentTime > LEDTime) {
            LEDTime = currentTime + 50000;
            ledringState();
        }
    }
#endif

    if (currentTime > calibratedAccTime) {
        if (smallAngle25 == 0) {
            calibratedACC = 0;  //the multi uses ACC and is not calibrated or is too much inclinated
            LED0_TOGGLE;
            calibratedAccTime = currentTime + 500000;
        } else
            calibratedACC = 1;
    }

    serialCom();

    if (sensors(SENSOR_GPS)) {
        static uint32_t GPSLEDTime;
        if (currentTime > GPSLEDTime && (GPS_numSat >= 5)) {
            GPSLEDTime = currentTime + 150000;
            LED1_TOGGLE;
        }
    }
}

uint16_t pwmReadRawRC(uint8_t chan)
{
    uint16_t data;

    data = pwmRead(cfg.rcmap[chan]);
    if (data < 750 || data > 2250)
        data = cfg.midrc;

    return data;
}

void computeRC(void)
{
    static int16_t rcData4Values[8][4], rcDataMean[8];
    static uint8_t rc4ValuesIndex = 0;
    uint8_t chan, a;

    rc4ValuesIndex++;
    for (chan = 0; chan < 8; chan++) {
        rcData4Values[chan][rc4ValuesIndex % 4] = rcReadRawFunc(chan);
        rcDataMean[chan] = 0;
        for (a = 0; a < 4; a++)
            rcDataMean[chan] += rcData4Values[chan][a];

        rcDataMean[chan] = (rcDataMean[chan] + 2) / 4;
        if (rcDataMean[chan] < rcData[chan] - 3)
            rcData[chan] = rcDataMean[chan] + 2;
        if (rcDataMean[chan] > rcData[chan] + 3)
            rcData[chan] = rcDataMean[chan] - 2;
    }
}

// #define TIMINGDEBUG

#ifdef TIMINGDEBUG
uint32_t trollTime = 0;
uint16_t cn = 0xffff, cx = 0x0;
#endif

void loop(void)
{
    static uint8_t rcDelayCommand;      // this indicates the number of time (multiple of RC measurement at 50Hz) the sticks must be maintained to run or switch off motors
    uint8_t axis, i;
    int16_t error, errorAngle;
    int16_t delta, deltaSum;
    int16_t PTerm, ITerm, DTerm;
    static int16_t lastGyro[3] = { 0, 0, 0 };
    static int16_t delta1[3], delta2[3];
    static int16_t errorGyroI[3] = { 0, 0, 0 };
    static int16_t errorAngleI[2] = { 0, 0 };
    static uint32_t rcTime = 0;
    static int16_t initialThrottleHold;

#ifdef TIMINGDEBUG
    trollTime = micros();
#endif

	//if the robot under the wifi control instead of spektrum
	if (feature(FEATURE_WIFICONTROLLER) && !feature(FEATURE_SPEKTRUM) && wifiFrameComplete())
	{
		assignWifiFrame();
		computeRC();
	}

    // this will return false if spektrum is disabled. shrug.
    if (spektrumFrameComplete())
        computeRC();

    if (currentTime > rcTime) { // 50Hz
        rcTime = currentTime + 20000;
        // TODO clean this up. computeRC should handle this check
        if (!feature(FEATURE_SPEKTRUM) && !feature(FEATURE_WIFICONTROLLER))	 //Add by DY
            computeRC();

        // Failsafe routine
        if (feature(FEATURE_FAILSAFE)) {
            if (failsafeCnt > (5 * cfg.failsafe_delay) && armed == 1) { // Stabilize, and set Throttle to specified level
                for (i = 0; i < 3; i++)
                    rcData[i] = cfg.midrc;      // after specified guard time after RC signal is lost (in 0.1sec)
                rcData[THROTTLE] = cfg.failsafe_throttle;
                if (failsafeCnt > 5 * (cfg.failsafe_delay + cfg.failsafe_off_delay)) {  // Turn OFF motors after specified Time (in 0.1sec)
                    armed = 0;  // This will prevent the copter to automatically rearm if failsafe shuts it down and prevents
                    okToArm = 0;        // to restart accidentely by just reconnect to the tx - you will have to switch off first to rearm
                }
                failsafeEvents++;
            }
            failsafeCnt++;
        }

        if (rcData[THROTTLE] < cfg.mincheck) {
            errorGyroI[ROLL] = 0;
            errorGyroI[PITCH] = 0;
            errorGyroI[YAW] = 0;
            errorAngleI[ROLL] = 0;
            errorAngleI[PITCH] = 0;
            rcDelayCommand++;
            if (rcData[YAW] < cfg.mincheck && rcData[PITCH] < cfg.mincheck && armed == 0) {
                if (rcDelayCommand == 20) {
                    calibratingG = 1000;
                    if (feature(FEATURE_GPS))
                        GPS_reset_home_position();
                }
            } else if (feature(FEATURE_INFLIGHT_ACC_CAL) && (armed == 0 && rcData[YAW] < cfg.mincheck && rcData[PITCH] > cfg.maxcheck && rcData[ROLL] > cfg.maxcheck)) {
                if (rcDelayCommand == 20) {
                    if (AccInflightCalibrationMeasurementDone) {        // trigger saving into eeprom after landing
                        AccInflightCalibrationMeasurementDone = 0;
                        AccInflightCalibrationSavetoEEProm = 1;
                    } else {
                        AccInflightCalibrationArmed = !AccInflightCalibrationArmed;
                        if (AccInflightCalibrationArmed) {
                            toggleBeep = 2;
                        } else {
                            toggleBeep = 3;
                        }
                    }
                }
            } else if (cfg.activate[BOXARM] > 0) {
                if (rcOptions[BOXARM] && okToArm) {
                    armed = 1;
                    headFreeModeHold = heading;
                } else if (armed)
                    armed = 0;
                rcDelayCommand = 0;
            } else if ((rcData[YAW] < cfg.mincheck || (cfg.retarded_arm == 1 && rcData[ROLL] < cfg.mincheck)) && armed == 1) {
                if (rcDelayCommand == 20)
                    armed = 0;  // rcDelayCommand = 20 => 20x20ms = 0.4s = time to wait for a specific RC command to be acknowledged
            } else if ((rcData[YAW] > cfg.maxcheck || (rcData[ROLL] > cfg.maxcheck && cfg.retarded_arm == 1)) && rcData[PITCH] < cfg.maxcheck && armed == 0 && calibratingG == 0 && calibratedACC == 1) {
                if (rcDelayCommand == 20) {
                    armed = 1;
                    headFreeModeHold = heading;
                }
            } else
                rcDelayCommand = 0;
        } else if (rcData[THROTTLE] > cfg.maxcheck && armed == 0) {
            if (rcData[YAW] < cfg.mincheck && rcData[PITCH] < cfg.mincheck) {   // throttle=max, yaw=left, pitch=min
                if (rcDelayCommand == 20)
                    calibratingA = 400;
                rcDelayCommand++;
            } else if (rcData[YAW] > cfg.maxcheck && rcData[PITCH] < cfg.mincheck) {    // throttle=max, yaw=right, pitch=min
                if (rcDelayCommand == 20)
                    calibratingM = 1;   // MAG calibration request
                rcDelayCommand++;
            } else if (rcData[PITCH] > cfg.maxcheck) {
                cfg.accTrim[PITCH] += 2;
                writeParams(1);
#ifdef LEDRING
                if (feature(FEATURE_LED_RING))
                    ledringBlink();
#endif
            } else if (rcData[PITCH] < cfg.mincheck) {
                cfg.accTrim[PITCH] -= 2;
                writeParams(1);
#ifdef LEDRING
                if (feature(FEATURE_LED_RING))
                    ledringBlink();
#endif
            } else if (rcData[ROLL] > cfg.maxcheck) {
                cfg.accTrim[ROLL] += 2;
                writeParams(1);
#ifdef LEDRING
                if (feature(FEATURE_LED_RING))
                    ledringBlink();
#endif
            } else if (rcData[ROLL] < cfg.mincheck) {
                cfg.accTrim[ROLL] -= 2;
                writeParams(1);
#ifdef LEDRING
                if (feature(FEATURE_LED_RING))
                    ledringBlink();
#endif
            } else {
                rcDelayCommand = 0;
            }
        }

        if (feature(FEATURE_INFLIGHT_ACC_CAL)) {
            if (AccInflightCalibrationArmed && armed == 1 && rcData[THROTTLE] > cfg.mincheck && !rcOptions[BOXARM]) {   // Copter is airborne and you are turning it off via boxarm : start measurement
                InflightcalibratingA = 50;
                AccInflightCalibrationArmed = 0;
            }
            if (rcOptions[BOXPASSTHRU]) {       // Use the Passthru Option to activate : Passthru = TRUE Meausrement started, Land and passtrhu = 0 measurement stored
                if (!AccInflightCalibrationArmed) {
                    AccInflightCalibrationArmed = 1;
                    InflightcalibratingA = 50;
                }
            } else if (AccInflightCalibrationMeasurementDone && armed == 0) {
                AccInflightCalibrationArmed = 0;
                AccInflightCalibrationMeasurementDone = 0;
                AccInflightCalibrationSavetoEEProm = 1;
            }
        }

        for (i = 0; i < CHECKBOXITEMS; i++) {
            rcOptions[i] = (((rcData[AUX1] < 1300) | (1300 < rcData[AUX1] && rcData[AUX1] < 1700) << 1 | (rcData[AUX1] > 1700) << 2
                             | (rcData[AUX2] < 1300) << 3 | (1300 < rcData[AUX2] && rcData[AUX2] < 1700) << 4 | (rcData[AUX2] > 1700) << 5 | (rcData[AUX3] < 1300) << 6 | (1300 < rcData[AUX3] && rcData[AUX3] < 1700) << 7 | (rcData[AUX3] > 1700) << 8 | (rcData[AUX4] < 1300) << 9 | (1300 < rcData[AUX4] && rcData[AUX4] < 1700) << 10 | (rcData[AUX4] > 1700) << 11) & cfg.activate[i]) > 0;
        }

        // note: if FAILSAFE is disable, failsafeCnt > 5*FAILSAVE_DELAY is always false
        if ((rcOptions[BOXACC] || (failsafeCnt > 5 * cfg.failsafe_delay)) && (sensors(SENSOR_ACC))) {
            // bumpless transfer to Level mode
            if (!accMode) {
                errorAngleI[ROLL] = 0;
                errorAngleI[PITCH] = 0;
                accMode = 1;
            }
        } else
            accMode = 0;        // failsave support

        if ((rcOptions[BOXARM]) == 0)
            okToArm = 1;
        if (accMode == 1) {
            LED1_ON;
        } else {
            LED1_OFF;
        }

#ifdef BARO
        if (sensors(SENSOR_BARO)) {
            if (rcOptions[BOXBARO]) {
                if (baroMode == 0) {
                    baroMode = 1;
                    AltHold = EstAlt;
                    initialThrottleHold = rcCommand[THROTTLE];
                    errorAltitudeI = 0;
                    BaroPID = 0;
                }
            } else
                baroMode = 0;
        }
#endif

#ifdef  MAG
        if (sensors(SENSOR_MAG)) {
            if (rcOptions[BOXMAG]) {
                if (magMode == 0) {
                    magMode = 1;
                    magHold = heading;
                }
            } else
                magMode = 0;
            if (rcOptions[BOXHEADFREE]) {
                if (headFreeMode == 0) {
                    headFreeMode = 1;
                }
            } else
                headFreeMode = 0;
        }
#endif

        if (sensors(SENSOR_GPS)) {
            if (rcOptions[BOXGPSHOME]) {
                if (GPSModeHome == 0) {
                    GPSModeHome = 1;
                    GPS_set_next_wp(&GPS_home[LAT], &GPS_home[LON]);
                    nav_mode = NAV_MODE_WP;
                }
            } else
                GPSModeHome = 0;
            if (rcOptions[BOXGPSHOLD]) {
                if (GPSModeHold == 0) {
                    GPSModeHold = 1;
                    GPS_hold[LAT] = GPS_coord[LAT];
                    GPS_hold[LON] = GPS_coord[LON];
                    GPS_set_next_wp(&GPS_hold[LAT], &GPS_hold[LON]);
                    nav_mode = NAV_MODE_POSHOLD;
                }
            } else {
                GPSModeHold = 0;
            }
        }

        if (rcOptions[BOXPASSTHRU]) {
            passThruMode = 1;
        } else
            passThruMode = 0;
    } else {                    // not in rc loop
        static int8_t taskOrder = 0;    // never call all function in the same loop, to avoid high delay spikes
        switch (taskOrder++ % 4) {
        case 0:
#ifdef MAG
            if (sensors(SENSOR_MAG))
                Mag_getADC();
#endif
            break;
        case 1:
#ifdef BARO
            if (sensors(SENSOR_BARO))
                Baro_update();
#endif
            break;
        case 2:
#ifdef BARO
            if (sensors(SENSOR_BARO))
                getEstimatedAltitude();
#endif
            break;
        case 3:
#ifdef SONAR
            Sonar_update();
            debug3 = sonarAlt;
#endif
            break;
        default:
            taskOrder = 0;
            break;
        }
    }

    computeIMU();
    // Measure loop rate just afer reading the sensors
    currentTime = micros();
    cycleTime = currentTime - previousTime;
    previousTime = currentTime;

#ifdef MPU6050_DMP
    mpu6050DmpLoop();
#endif

#ifdef MAG
    if (sensors(SENSOR_MAG)) {
        if (abs(rcCommand[YAW]) < 70 && magMode) {
            int16_t dif = heading - magHold;
            if (dif <= -180)
                dif += 360;
            if (dif >= +180)
                dif -= 360;
            if (smallAngle25)
                rcCommand[YAW] -= dif * cfg.P8[PIDMAG] / 30;    // 18 deg
        } else
            magHold = heading;
    }
#endif

#ifdef BARO
    if (sensors(SENSOR_BARO)) {
        if (baroMode) {
            if (abs(rcCommand[THROTTLE] - initialThrottleHold) > 20) {
                baroMode = 0;   // so that a new althold reference is defined
            }
            rcCommand[THROTTLE] = initialThrottleHold + BaroPID;
        }
    }
#endif

    if (sensors(SENSOR_GPS)) {
        // Check that we really need to navigate ?
        if ((GPSModeHome == 0 && GPSModeHold == 0) || (GPS_fix_home == 0)) {
            // If not. Reset nav loops and all nav related parameters
            GPS_reset_nav();
        } else {
            float sin_yaw_y = sinf(heading * 0.0174532925f);
            float cos_yaw_x = cosf(heading * 0.0174532925f);
            GPS_angle[ROLL] = (nav[LON] * cos_yaw_x - nav[LAT] * sin_yaw_y) / 10;
            GPS_angle[PITCH] = (nav[LON] * sin_yaw_y + nav[LAT] * cos_yaw_x) / 10;
        }
    }
    // **** PITCH & ROLL & YAW PID ****    
    for (axis = 0; axis < 3; axis++) {
        if (accMode == 1 && axis < 2) { // LEVEL MODE
            // 50 degrees max inclination
            errorAngle = constrain(2 * rcCommand[axis] - GPS_angle[axis], -500, +500) - angle[axis] + cfg.accTrim[axis];        //16 bits is ok here
#ifdef LEVEL_PDF
            PTerm = -(int32_t) angle[axis] * cfg.P8[PIDLEVEL] / 100;
#else
            PTerm = (int32_t) errorAngle *cfg.P8[PIDLEVEL] / 100;       //32 bits is needed for calculation: errorAngle*P8[PIDLEVEL] could exceed 32768   16 bits is ok for result
#endif
            PTerm = constrain(PTerm, -cfg.D8[PIDLEVEL] * 5, +cfg.D8[PIDLEVEL] * 5);

            errorAngleI[axis] = constrain(errorAngleI[axis] + errorAngle, -10000, +10000);      // WindUp     // 16 bits is ok here
            ITerm = ((int32_t) errorAngleI[axis] * cfg.I8[PIDLEVEL]) >> 12;     // 32 bits is needed for calculation:10000*I8 could exceed 32768   16 bits is ok for result
        } else {                // ACRO MODE or YAW axis
            error = (int32_t) rcCommand[axis] * 10 * 8 / cfg.P8[axis];  //32 bits is needed for calculation: 500*5*10*8 = 200000   16 bits is ok for result if P8>2 (P>0.2)
            error -= gyroData[axis];

            PTerm = rcCommand[axis];

            errorGyroI[axis] = constrain(errorGyroI[axis] + error, -16000, +16000);     // WindUp // 16 bits is ok here
            if (abs(gyroData[axis]) > 640)
                errorGyroI[axis] = 0;
            ITerm = (errorGyroI[axis] / 125 * cfg.I8[axis]) >> 6;       // 16 bits is ok here 16000/125 = 128 ; 128*250 = 32000
        }
        PTerm -= (int32_t) gyroData[axis] * dynP8[axis] / 10 / 8;       // 32 bits is needed for calculation

        delta = gyroData[axis] - lastGyro[axis];        //16 bits is ok here, the dif between 2 consecutive gyro reads is limited to 800
        lastGyro[axis] = gyroData[axis];
        deltaSum = delta1[axis] + delta2[axis] + delta;
        delta2[axis] = delta1[axis];
        delta1[axis] = delta;

        DTerm = ((int32_t) deltaSum * dynD8[axis]) >> 5;        //32 bits is needed for calculation

        axisPID[axis] = PTerm + ITerm - DTerm;
    }

    mixTable();
    writeServos();
    writeMotors();

#ifdef TIMINGDEBUG
    while (micros() < trollTime + 1750);
    // LED0_TOGGLE;
    {
        if (cycleTime < cn)
            cn = cycleTime;
        if (cycleTime > cx)
            cx = cycleTime;
            
        debug1 = cn;
        debug2 = cx;
    }
#endif
}
