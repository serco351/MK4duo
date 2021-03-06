/****************************************************************************************
* 80
* RUMBA
****************************************************************************************/

//###CHIP
#if DISABLED(__AVR_ATmega2560__)
  #error Oops!  Make sure you have 'Arduino Mega 2560' selected from the 'Tools -> Boards' menu.
#endif//@@@

#define KNOWN_BOARD 1

//###BOARD_NAME
#ifndef BOARD_NAME
	#define BOARD_NAME "Rumba"
#endif
//@@@


//###X_AXIS
#define ORIG_X_STEP_PIN 17
#define ORIG_X_DIR_PIN 16
#define ORIG_X_ENABLE_PIN 48
#define ORIG_X_CS_PIN -1

//###Y_AXIS
#define ORIG_Y_STEP_PIN 54
#define ORIG_Y_DIR_PIN 47
#define ORIG_Y_ENABLE_PIN 55
#define ORIG_Y_CS_PIN -1

//###Z_AXIS
#define ORIG_Z_STEP_PIN 57
#define ORIG_Z_DIR_PIN 56
#define ORIG_Z_ENABLE_PIN 62
#define ORIG_Z_CS_PIN -1

//###EXTRUDER_0
#define ORIG_E0_STEP_PIN 23
#define ORIG_E0_DIR_PIN 22
#define ORIG_E0_ENABLE_PIN 24
#define ORIG_E0_CS_PIN -1
#define ORIG_SOL0_PIN -1

//###EXTRUDER_1
#define ORIG_E1_STEP_PIN 26
#define ORIG_E1_DIR_PIN 25
#define ORIG_E1_ENABLE_PIN 27
#define ORIG_E1_CS_PIN -1
#define ORIG_SOL1_PIN -1

//###EXTRUDER_2
#define ORIG_E2_STEP_PIN 29
#define ORIG_E2_DIR_PIN 28
#define ORIG_E2_ENABLE_PIN 39
#define ORIG_E2_CS_PIN -1
#define ORIG_SOL2_PIN -1

//###EXTRUDER_3
#define ORIG_E3_STEP_PIN -1
#define ORIG_E3_DIR_PIN -1
#define ORIG_E3_ENABLE_PIN -1
#define ORIG_E3_CS_PIN -1
#define ORIG_SOL3_PIN -1

//###EXTRUDER_4
#define ORIG_E4_STEP_PIN -1
#define ORIG_E4_DIR_PIN -1
#define ORIG_E4_ENABLE_PIN -1
#define ORIG_E4_CS_PIN -1
#define ORIG_SOL4_PIN -1

//###EXTRUDER_5
#define ORIG_E5_STEP_PIN -1
#define ORIG_E5_DIR_PIN -1
#define ORIG_E5_ENABLE_PIN -1
#define ORIG_E5_CS_PIN -1
#define ORIG_SOL5_PIN -1

//###EXTRUDER_6
#define ORIG_E6_STEP_PIN -1
#define ORIG_E6_DIR_PIN -1
#define ORIG_E6_ENABLE_PIN -1
#define ORIG_E6_CS_PIN -1
#define ORIG_SOL6_PIN -1

//###EXTRUDER_7
#define ORIG_E7_STEP_PIN -1
#define ORIG_E7_DIR_PIN -1
#define ORIG_E7_ENABLE_PIN -1
#define ORIG_E7_CS_PIN -1
#define ORIG_SOL7_PIN -1

//###ENDSTOP
#define ORIG_X_MIN_PIN 37
#define ORIG_X_MAX_PIN 36
#define ORIG_Y_MIN_PIN 35
#define ORIG_Y_MAX_PIN 34
#define ORIG_Z_MIN_PIN 33
#define ORIG_Z_MAX_PIN 32
#define ORIG_Z2_MIN_PIN -1
#define ORIG_Z2_MAX_PIN -1
#define ORIG_Z3_MIN_PIN -1
#define ORIG_Z3_MAX_PIN -1
#define ORIG_Z4_MIN_PIN -1
#define ORIG_Z4_MAX_PIN -1
#define ORIG_E_MIN_PIN -1
#define ORIG_Z_PROBE_PIN -1

//###SINGLE_ENDSTOP
#define X_STOP_PIN -1
#define Y_STOP_PIN -1
#define Z_STOP_PIN -1

//###HEATER
#define ORIG_HEATER_0_PIN -1
#define ORIG_HEATER_1_PIN -1
#define ORIG_HEATER_2_PIN -1
#define ORIG_HEATER_3_PIN -1
#define ORIG_HEATER_BED_PIN -1
#define ORIG_HEATER_CHAMBER_PIN -1
#define ORIG_COOLER_PIN -1

//###TEMPERATURE
#define ORIG_TEMP_0_PIN -1
#define ORIG_TEMP_1_PIN -1
#define ORIG_TEMP_2_PIN -1
#define ORIG_TEMP_3_PIN -1
#define ORIG_TEMP_BED_PIN -1
#define ORIG_TEMP_CHAMBER_PIN -1
#define ORIG_TEMP_COOLER_PIN -1

//###FAN
#define ORIG_FAN_PIN 7
#define ORIG_FAN1_PIN 8
#define ORIG_FAN2_PIN -1
#define ORIG_FAN3_PIN -1

//###MISC
#define ORIG_PS_ON_PIN 45
#define ORIG_BEEPER_PIN 44
#define LED_PIN 13
#define SDPOWER -1
#define SD_DETECT_PIN 49
#define SDSS 53
#define KILL_PIN 46
#define DEBUG_PIN -1
#define SUICIDE_PIN -1

//###LASER
#define ORIG_LASER_PWR_PIN -1
#define ORIG_LASER_PWM_PIN -1

//###SERVOS
#if NUM_SERVOS > 0
	#define SERVO0_PIN 5
	#if NUM_SERVOS > 1
		#define SERVO1_PIN -1
		#if NUM_SERVOS > 2
			#define SERVO2_PIN -1
			#if NUM_SERVOS > 3
				#define SERVO3_PIN -1
			#endif
		#endif
	#endif
#endif
//@@@

//###UNKNOWN_PINS
#define LCD_PINS_RS        19
#define LCD_PINS_ENABLE    42
#define LCD_PINS_D4        18
#define LCD_PINS_D5        38
#define LCD_PINS_D6        41
#define LCD_PINS_D7        40
#define BTN_EN1            11
#define BTN_EN2            12
#define BTN_ENC            43
//@@@

//###IF_BLOCKS
#if (TEMP_SENSOR_0==0)
 #define ORIG_TEMP_0_PIN             -1
 #define ORIG_HEATER_0_PIN           -1
#else
 #define ORIG_HEATER_0_PIN           2    // EXTRUDER 1
 #if (TEMP_SENSOR_0==-1)
  #define ORIG_TEMP_0_PIN            6    // ANALOG NUMBERING - connector *K1* on RUMBA thermocouple ADD ON is used
 #else
  #define ORIG_TEMP_0_PIN            15   // ANALOG NUMBERING - default connector for thermistor *T0* on rumba board is used
 #endif
#endif

#if (TEMP_SENSOR_1==0)
 #define ORIG_TEMP_1_PIN             -1
 #define ORIG_HEATER_1_PIN           -1
#else
 #define ORIG_HEATER_1_PIN           3    // EXTRUDER 2
 #if (TEMP_SENSOR_1==-1)
  #define ORIG_TEMP_1_PIN            5    // ANALOG NUMBERING - connector *K2* on RUMBA thermocouple ADD ON is used
 #else
  #define ORIG_TEMP_1_PIN            14   // ANALOG NUMBERING - default connector for thermistor *T1* on rumba board is used
 #endif
#endif

#if (TEMP_SENSOR_2==0)
 #define ORIG_TEMP_2_PIN         -1
 #define ORIG_HEATER_2_PIN       -1
#else
 #define ORIG_HEATER_2_PIN        6    // EXTRUDER 3
 #if (TEMP_SENSOR_2==-1)
  #define ORIG_TEMP_2_PIN         7    // ANALOG NUMBERING - connector *K3* on RUMBA thermocouple ADD ON is used <-- this can not be used when TEMP_SENSOR_BED is defined as thermocouple
 #else
  #define ORIG_TEMP_2_PIN         13   // ANALOG NUMBERING - default connector for thermistor *T2* on rumba board is used
 #endif
#endif

#if (TEMP_SENSOR_BED==0)
 #define ORIG_TEMP_BED_PIN       -1
 #define ORIG_HEATER_BED_PIN     -1
#else
 #define ORIG_HEATER_BED_PIN      9    // BED
 #if (TEMP_SENSOR_BED==-1)
  #define ORIG_TEMP_BED_PIN       7    // ANALOG NUMBERING - connector *K3* on RUMBA thermocouple ADD ON is used <-- this can not be used when TEMP_SENSOR_2 is defined as thermocouple
 #else
  #define ORIG_TEMP_BED_PIN       11   // ANALOG NUMBERING - default connector for thermistor *THB* on rumba board is used
 #endif
#endif
//@@@
