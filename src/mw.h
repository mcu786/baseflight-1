#pragma once

/* Flying Wing: you can change change servo orientation and servo min/max values here */
/* valid for all flight modes, even passThrough mode */
/* need to setup servo directions here; no need to swap servos amongst channels at rx */ 
#define PITCH_DIRECTION_L 1 // left servo - pitch orientation
#define PITCH_DIRECTION_R -1  // right servo - pitch orientation (opposite sign to PITCH_DIRECTION_L, if servos are mounted in mirrored orientation)
#define ROLL_DIRECTION_L 1 // left servo - roll orientation
#define ROLL_DIRECTION_R 1  // right servo - roll orientation  (same sign as ROLL_DIRECTION_L, if servos are mounted in mirrored orientation)
#define WING_LEFT_MIN  1020 // limit servo travel range must be inside [1020;2000]
#define WING_LEFT_MAX  2000 // limit servo travel range must be inside [1020;2000]
#define WING_RIGHT_MIN 1020 // limit servo travel range must be inside [1020;2000]
#define WING_RIGHT_MAX 2000 // limit servo travel range must be inside [1020;2000]

/* experimental
   camera trigger function : activated via Rc Options in the GUI, servo output=A2 on promini */
#define CAM_SERVO_HIGH 2000  // the position of HIGH state servo
#define CAM_SERVO_LOW 1020   // the position of LOW state servo
#define CAM_TIME_HIGH 1000   // the duration of HIGH state servo expressed in ms
#define CAM_TIME_LOW 1000    // the duration of LOW state servo expressed in ms

/* for VBAT monitoring frequency */
#define VBATFREQ 6        // to read battery voltage - nth number of loop iterations

#define  VERSION  200

#define LAT  0
#define LON  1

// Serial GPS only variables
// navigation mode
typedef enum NavigationMode
{
    NAV_MODE_NONE = 1,
    NAV_MODE_POSHOLD,
    NAV_MODE_WP
} NavigationMode;

// Syncronized with GUI. Only exception is mixer > 11, which is always returned as 11 during serialization.
typedef enum MultiType
{
    MULTITYPE_TRI = 1,
    MULTITYPE_QUADP = 2,
    MULTITYPE_QUADX = 3,
    MULTITYPE_BI = 4,
    MULTITYPE_GIMBAL = 5,
    MULTITYPE_Y6 = 6,
    MULTITYPE_HEX6 = 7,
    MULTITYPE_FLYING_WING = 8,
    MULTITYPE_Y4 = 9,
    MULTITYPE_HEX6X = 10,
    MULTITYPE_OCTOX8 = 11,
    MULTITYPE_OCTOFLATP = 12,       // the GUI is the same for all 8 motor configs
    MULTITYPE_OCTOFLATX = 13,       // the GUI is the same for all 8 motor configs
    MULTITYPE_AIRPLANE = 14,
    MULTITYPE_HELI_120_CCPM = 15,   // simple model
    MULTITYPE_HELI_90_DEG = 16,     // simple model
    MULTITYPE_VTAIL4 = 17,
    MULTITYPE_LAST = 18
} MultiType;

typedef enum GimbalFlags {
    GIMBAL_NORMAL = 1 << 0,
    GIMBAL_TILTONLY = 1 << 1,
    GIMBAL_DISABLEAUX34 = 1 << 2,
} GimbalFlags;

/*********** RC alias *****************/
#define ROLL       0
#define PITCH      1
#define YAW        2
#define THROTTLE   3
#define AUX1       4
#define AUX2       5
#define AUX3       6
#define AUX4       7

#define PIDALT     3
#define PIDPOS     4
#define PIDPOSR    5
#define PIDNAVR    6
#define PIDLEVEL   7
#define PIDMAG     8
#define PIDVEL     9 // not used currently

#define BOXACC       0
#define BOXBARO      1
#define BOXMAG       2
#define BOXCAMSTAB   3
#define BOXCAMTRIG   4
#define BOXARM       5
#define BOXGPSHOME   6
#define BOXGPSHOLD   7
#define BOXPASSTHRU  8
#define BOXHEADFREE  9
#define BOXBEEPERON  10

#define CHECKBOXITEMS 11
#define PIDITEMS 10

#define min(a, b) ((a) < (b) ? (a) : (b))
#define max(a, b) ((a) > (b) ? (a) : (b))
#define abs(x) ((x) > 0 ? (x) : -(x))
#define constrain(amt, low, high) ((amt) < (low) ? (low) : ((amt) > (high) ? (high) : (amt)))

typedef struct config_t {
    uint8_t version;
    uint8_t mixerConfiguration;
    uint32_t enabledFeatures;

    uint8_t P8[PIDITEMS];
    uint8_t I8[PIDITEMS];
    uint8_t D8[PIDITEMS];

    uint8_t rcRate8;
    uint8_t rcExpo8;
    uint8_t thrMid8;
    uint8_t thrExpo8;

    uint8_t rollPitchRate;
    uint8_t yawRate;

    uint8_t dynThrPID;
    int16_t accZero[3];
    int16_t magZero[3];
    int16_t mag_declination;                // Get your magnetic decliniation from here : http://magnetic-declination.com/
    int16_t accTrim[2];

    // sensor-related stuff
    uint8_t acc_hardware;                   // Which acc hardware to use on boards with more than one device
    uint8_t acc_lpf_factor;                 // Set the Low Pass Filter factor for ACC. Increasing this value would reduce ACC noise (visible in GUI), but would increase ACC lag time. Zero = no filter
    uint16_t gyro_lpf;                      // mpuX050 LPF setting
    uint16_t gyro_cmpf_factor;              // Set the Gyro Weight for Gyro/Acc complementary filter. Increasing this value would reduce and delay Acc influence on the output of the filter.
    uint32_t gyro_smoothing_factor;         // How much to smoothen with per axis (32bit value with Roll, Pitch, Yaw in bits 24, 16, 8 respectively

    uint16_t activate[CHECKBOXITEMS];       // activate switches
    uint8_t vbatscale;                      // adjust this to match battery voltage to reported value
    uint8_t vbatmaxcellvoltage;             // maximum voltage per cell, used for auto-detecting battery voltage in 0.1V units, default is 43 (4.3V)
    uint8_t vbatmincellvoltage;             // minimum voltage per cell, this triggers battery out alarms, in 0.1V units, default is 33 (3.3V)

    // Radio/ESC-related configuration
    uint8_t rcmap[8];                       // mapping of radio channels to internal RPYTA+ order
    uint8_t deadband;                       // introduce a deadband around the stick center for pitch and roll axis. Must be greater than zero.
    uint8_t yawdeadband;                    // introduce a deadband around the stick center for yaw axis. Must be greater than zero.
    uint8_t spektrum_hires;                 // spektrum high-resolution y/n (1024/2048bit)
    uint16_t midrc;                         // Some radios have not a neutral point centered on 1500. can be changed here
    uint16_t mincheck;                      // minimum rc end
    uint16_t maxcheck;                      // maximum rc end
    uint8_t retarded_arm;                   // allow disarsm/arm on throttle down + roll left/right
    
    // Failsafe related configuration
    uint8_t failsafe_delay;                 // Guard time for failsafe activation after signal lost. 1 step = 0.1sec - 1sec in example (10)
    uint8_t failsafe_off_delay;             // Time for Landing before motors stop in 0.1sec. 1 step = 0.1sec - 20sec in example (200)
    uint16_t failsafe_throttle;             // Throttle level used for landing - specify value between 1000..2000 (pwm pulse width for slightly below hover). center throttle = 1500.

    // motor/esc/servo related stuff
    uint16_t minthrottle;                   // Set the minimum throttle command sent to the ESC (Electronic Speed Controller). This is the minimum value that allow motors to run at a idle speed.
    uint16_t maxthrottle;                   // This is the maximum value for the ESCs at full power this value can be increased up to 2000
    uint16_t mincommand;                    // This is the value for the ESCs when they are not armed. In some cases, this value must be lowered down to 900 for some specific ESCs
    uint16_t motor_pwm_rate;                // The update rate of motor outputs (50-498Hz)
    uint16_t servo_pwm_rate;                // The update rate of servo outputs (50-498Hz)

    // mixer-related configuration
    int8_t yaw_direction;
    uint16_t wing_left_mid;                 // left servo center pos. - use this for initial trim
    uint16_t wing_right_mid;                // right servo center pos. - use this for initial trim
    uint16_t tri_yaw_middle;                // tail servo center pos. - use this for initial trim
    uint16_t tri_yaw_min;                   // tail servo min
    uint16_t tri_yaw_max;                   // tail servo max

    // gimbal-related configuration
    int8_t gimbal_pitch_gain;               // gimbal pitch servo gain (tied to angle) can be negative to invert movement
    int8_t gimbal_roll_gain;                // gimbal roll servo gain (tied to angle) can be negative to invert movement
    uint8_t gimbal_flags;                   // in servotilt mode, various things that affect stuff
    uint16_t gimbal_pitch_min;              // gimbal pitch servo min travel
    uint16_t gimbal_pitch_max;              // gimbal pitch servo max travel
    uint16_t gimbal_pitch_mid;              // gimbal pitch servo neutral value
    uint16_t gimbal_roll_min;               // gimbal roll servo min travel
    uint16_t gimbal_roll_max;               // gimbal roll servo max travel
    uint16_t gimbal_roll_mid;               // gimbal roll servo neutral value

    // gps-related stuff
    uint32_t gps_baudrate;
    uint16_t gps_wp_radius;                 // if we are within this distance to a waypoint then we consider it reached (distance is in cm)
    uint8_t gps_lpf;                        // Low pass filter cut frequency for derivative calculation (default 20Hz)
    uint8_t nav_controls_heading;           // copter faces toward the navigation point, maghold must be enabled for it
    uint16_t nav_speed_min;                 // cm/sec
    uint16_t nav_speed_max;                 // cm/sec

   // serial(uart1) baudrate
    uint32_t serial_baudrate;
} config_t;

extern int16_t gyroZero[3];
extern int16_t gyroData[3];
extern int16_t angle[2];
extern int16_t axisPID[3];
extern int16_t rcCommand[4];
extern uint8_t rcOptions[CHECKBOXITEMS];
extern int16_t failsafeCnt;

extern int16_t debug1, debug2, debug3, debug4;
extern uint8_t armed;
extern int16_t gyroADC[3], accADC[3], accSmooth[3], magADC[3];
extern uint16_t acc_1G;
extern uint32_t currentTime;
extern uint32_t previousTime;
extern uint16_t cycleTime;
extern uint8_t calibratedACC;
extern uint16_t calibratingA;
extern uint8_t calibratingM;
extern uint16_t calibratingG;
extern int16_t heading;
extern int16_t annex650_overrun_count;
extern int32_t pressure;
extern int32_t BaroAlt;
extern int16_t sonarAlt;
extern int32_t EstAlt;
extern int32_t  AltHold;
extern int16_t  errorAltitudeI;
extern int16_t  BaroPID;
extern uint8_t headFreeMode;
extern int16_t headFreeModeHold;
extern uint8_t passThruMode;
extern int8_t smallAngle25;
extern int16_t zVelocity;
extern int16_t heading, magHold;
extern int16_t motor[8];
extern int16_t servo[8];
extern int16_t rcData[8];
extern uint8_t accMode;
extern uint8_t magMode;
extern uint8_t baroMode;
extern uint8_t  GPSModeHome;
extern uint8_t  GPSModeHold;
extern int32_t  GPS_coord[2];
extern int32_t  GPS_home[2];
extern int32_t  GPS_hold[2];
extern uint8_t  GPS_fix , GPS_fix_home;
extern uint8_t  GPS_numSat;
extern uint16_t GPS_distanceToHome,GPS_distanceToHold;       // distance to home or hold point in meters
extern int16_t  GPS_directionToHome,GPS_directionToHold;     // direction to home or hol point in degrees
extern uint16_t GPS_altitude,GPS_speed;                      // altitude in 0.1m and speed in 0.1m/s
extern uint8_t  GPS_update;                                  // it's a binary toogle to distinct a GPS position update
extern int16_t  GPS_angle[2];                                // it's the angles that must be applied for GPS correction
extern uint16_t GPS_ground_course;                           // degrees*10
extern uint8_t  GPS_Present;                                 // Checksum from Gps serial
extern uint8_t  GPS_Enable;
extern int16_t	nav[2];
extern int8_t   nav_mode;                                    // Navigation mode
extern uint8_t vbat;
extern int16_t lookupPitchRollRC[6];   // lookup table for expo & RC rate PITCH+ROLL
extern int16_t lookupThrottleRC[11];   // lookup table for expo & mid THROTTLE
extern uint8_t toggleBeep;

extern config_t cfg;
extern sensor_t acc;
extern sensor_t gyro;

// main
void loop(void);

// IMU
void imuInit(void);
void annexCode(void);
void computeIMU(void);
void blinkLED(uint8_t num, uint8_t wait, uint8_t repeat);
void getEstimatedAltitude(void);

// Sensors
void sensorsAutodetect(void);
void batteryInit(void);
uint16_t batteryAdcToVoltage(uint16_t src);
void ACC_getADC(void);
void Baro_update(void);
void Gyro_getADC(void);
void Mag_init(void);
void Mag_getADC(void);

// Output
void mixerInit(void);
void writeServos(void);
void writeMotors(void);
void writeAllMotors(int16_t mc);
void mixTable(void);

// Serial
void serialInit(uint32_t baudrate);
void serialCom(void);

// Config
void parseRcChannels(const char *input);
void readEEPROM(void);
void writeParams(uint8_t b);
void checkFirstTime(bool reset);
bool sensors(uint32_t mask);
void sensorsSet(uint32_t mask);
void sensorsClear(uint32_t mask);
uint32_t sensorsMask(void);
bool feature(uint32_t mask);
void featureSet(uint32_t mask);
void featureClear(uint32_t mask);
void featureClearAll(void);
uint32_t featureMask(void);

// spektrum
void spektrumInit(void);
bool spektrumFrameComplete(void);

// buzzer
void buzzer(uint8_t warn_vbat);

// cli
void cliProcess(void);

// gps
void gpsInit(uint32_t baudrate);
void GPS_reset_home_position(void);
void GPS_reset_nav(void);
void GPS_set_next_wp(int32_t* lat, int32_t* lon);
