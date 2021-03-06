/**
 * MK4duo 3D Printer Firmware
 *
 * Based on Marlin, Sprinter and grbl
 * Copyright (C) 2011 Camiel Gubbels / Erik van der Zalm
 * Copyright (C) 2013 - 2017 Alberto Cotronei @MagoKimbra
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 */

/**
 * This is the main Hardware Abstraction Layer (HAL).
 * To make the firmware work with different processors and toolchains,
 * all hardware related code should be packed into the hal files.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 *
 * Description: HAL for Arduino Due and compatible (SAM3X8E)
 *
 * Contributors:
 * Copyright (c) 2014 Bob Cousins bobcousins42@googlemail.com
 *                    Nico Tonnhofer wurstnase.reprap@gmail.com
 *
 * Copyright (c) 2015 - 2016 Alberto Cotronei @MagoKimbra
 *
 * ARDUINO_ARCH_SAM
 */

// --------------------------------------------------------------------------
// Includes
// --------------------------------------------------------------------------

#include "../../../base.h"

#if ENABLED(ARDUINO_ARCH_SAM)

#include <malloc.h>
#include <Wire.h>

// --------------------------------------------------------------------------
// Public Variables
// --------------------------------------------------------------------------
extern "C" char *sbrk(int i);
uint8_t MCUSR;

#if ANALOG_INPUTS > 0
  int32_t   AnalogInputRead[ANALOG_INPUTS],
            AnalogSamples[ANALOG_INPUTS][MEDIAN_COUNT],
            AnalogSamplesSum[ANALOG_INPUTS],
            adcSamplesMin[ANALOG_INPUTS],
            adcSamplesMax[ANALOG_INPUTS];
  int       adcCounter = 0,
            adcSamplePos = 0;
  uint32_t  adcEnable = 0;
  bool      Analog_is_ready = false;

  volatile int16_t HAL::AnalogInputValues[ANALOG_INPUTS] = { 0 };
#endif

const uint8_t AnalogInputChannels[] PROGMEM = ANALOG_INPUT_CHANNELS;
static unsigned int cycle_100ms = 0;

// disable interrupts
void cli(void) {
  noInterrupts();
}

// enable interrupts
void sei(void) {
  interrupts();
}

static inline void ConfigurePin(const PinDescription& pinDesc) {
  PIO_Configure(pinDesc.pPort, pinDesc.ulPinType, pinDesc.ulPin, pinDesc.ulPinConfiguration);
}

#ifndef DUE_SOFTWARE_SPI
  int spiDueDividors[] = {10,21,42,84,168,255,255};
#endif

HAL::HAL() {
  // ctor
}

HAL::~HAL() {
  // dtor
}

bool HAL::execute_100ms = false;

// do any hardware-specific initialization here
void HAL::hwSetup(void) {

  #if DISABLED(USE_WATCHDOG)
    // Disable watchdog
    WDT_Disable(WDT);
  #endif

  TimeTick_Configure(F_CPU);

  // setup microsecond delay timer
  pmc_enable_periph_clk(DELAY_TIMER_IRQ);
  TC_Configure(DELAY_TIMER, DELAY_TIMER_CHANNEL, TC_CMR_WAVSEL_UP |
               TC_CMR_WAVE | DELAY_TIMER_CLOCK);
  TC_Start(DELAY_TIMER, DELAY_TIMER_CHANNEL);

  #if MB(ALLIGATOR) || MB(ALLIGATOR_V3)

    ExternalDac::begin();
    SET_INPUT(MOTOR_FAULT_PIN);
    #if MB(ALLIGATOR_V3)
      SET_INPUT(MOTOR_FAULT_PIGGY_PIN);
      SET_INPUT(FTDI_COM_RESET_PIN);
      SET_INPUT(ESP_WIFI_MODULE_RESET_PIN);
      SET_OUTPUT(EXP1_VOLTAGE_SELECT);
      OUT_WRITE(EXP1_OUT_ENABLE_PIN, HIGH);
    #elif MB(ALLIGATOR)
      // Init Expansion Port Voltage logic Selector
      OUT_WRITE(EXP_VOLTAGE_LEVEL_PIN, UI_VOLTAGE_LEVEL);
    #endif

    #if HAS_BUZZER
      buzz(10,10);
    #endif

  #elif MB(ULTRATRONICS)

    /* avoid floating pins */
    OUT_WRITE(ORIG_FAN_PIN, LOW);
    OUT_WRITE(ORIG_FAN1_PIN, LOW);

    OUT_WRITE(ORIG_HEATER_0_PIN, LOW);
    OUT_WRITE(ORIG_HEATER_1_PIN, LOW);
    OUT_WRITE(ORIG_HEATER_2_PIN, LOW);
    OUT_WRITE(ORIG_HEATER_3_PIN, LOW);

    /* setup CS pins */
    OUT_WRITE(MAX31855_SS0, HIGH);
    OUT_WRITE(MAX31855_SS1, HIGH);
    OUT_WRITE(MAX31855_SS2, HIGH);
    OUT_WRITE(MAX31855_SS3, HIGH);

    OUT_WRITE(ENC424_SS, HIGH);
    OUT_WRITE(SS_PIN, HIGH);

    SET_INPUT(MISO);
    SET_OUTPUT(MOSI);

  #endif
}

// Print apparent cause of start/restart
void HAL::showStartReason() {

  int mcu = (RSTC->RSTC_SR & RSTC_SR_RSTTYP_Msk) >> RSTC_SR_RSTTYP_Pos;
  switch (mcu) {
    case 0:
      SERIAL_EM(MSG_POWERUP); break;
    case 1:
      // this is return from backup mode on SAM
      SERIAL_EM(MSG_BROWNOUT_RESET); break;
    case 2:
      SERIAL_EM(MSG_WATCHDOG_RESET); break;
    case 3:
      SERIAL_EM(MSG_SOFTWARE_RESET); break;
    case 4:
      SERIAL_EM(MSG_EXTERNAL_RESET); break;
  }
}

// Return available memory
int HAL::getFreeRam() {
  struct mallinfo memstruct = mallinfo();
  register char * stack_ptr asm ("sp");

  // avail mem in heap + (bottom of stack addr - end of heap addr)
  return (memstruct.fordblks + (int)stack_ptr -  (int)sbrk(0));
}

#if ANALOG_INPUTS > 0

  // Convert an Arduino Due pin number to the corresponding ADC channel number
  adc_channel_num_t HAL::pinToAdcChannel(int pin) {
    if (pin == ADC_TEMPERATURE_SENSOR) return (adc_channel_num_t)ADC_TEMPERATURE_SENSOR; // MCU TEMPERATURE SENSOR
    if (pin < A0) pin += A0;
    return (adc_channel_num_t) (int)g_APinDescription[pin].ulADCChannelNumber;
  }

  // Initialize ADC channels
  void HAL::analogStart(void) {

    #if MB(ALLIGATOR) || MB(ALLIGATOR_V3)
      PIO_Configure(
        g_APinDescription[58].pPort,
        g_APinDescription[58].ulPinType,
        g_APinDescription[58].ulPin,
        g_APinDescription[58].ulPinConfiguration);
      PIO_Configure(
        g_APinDescription[59].pPort,
        g_APinDescription[59].ulPinType,
        g_APinDescription[59].ulPin,
        g_APinDescription[59].ulPinConfiguration);
    #endif // MB(ALLIGATOR) || MB(ALLIGATOR_V3)

    // ensure we can write to ADC registers
    ADC->ADC_WPMR = 0x41444300u;    // ADC_WPMR_WPKEY(0);
    pmc_enable_periph_clk(ID_ADC);  // enable adc clock

    for (int i = 0; i < ANALOG_INPUTS; i++) {
      AnalogInputValues[i] = 0;
      adcSamplesMin[i] = 100000;
      adcSamplesMax[i] = 0;
      adcEnable |= (0x1u << pinToAdcChannel(AnalogInputChannels[i]));
      AnalogSamplesSum[i] = 2048 * MEDIAN_COUNT;
      for (int j = 0; j < MEDIAN_COUNT; j++)
        AnalogSamples[i][j] = 2048;
    }

    // enable channels
    ADC->ADC_CHER = adcEnable;
    ADC->ADC_CHDR = !adcEnable;

    #if !MB(RADDS) // RADDS not have MCU Temperature
      // Enable MCU temperature
      ADC->ADC_ACR |= ADC_ACR_TSON;
    #endif

    // Initialize ADC mode register (some of the following params are not used here)
    // HW trigger disabled, use external Trigger, 12 bit resolution
    // core and ref voltage stays on, normal sleep mode, normal not free-run mode
    // startup time 16 clocks, settling time 17 clocks, no changes on channel switch
    // convert channels in numeric order
    // set prescaler rate  MCK/((PRESCALE+1) * 2)
    // set tracking time  (TRACKTIM+1) * clock periods
    // set transfer period  (TRANSFER * 2 + 3)
    ADC->ADC_MR = ADC_MR_TRGEN_DIS | ADC_MR_TRGSEL_ADC_TRIG0 | ADC_MR_LOWRES_BITS_12 |
                  ADC_MR_SLEEP_NORMAL | ADC_MR_FWUP_OFF | ADC_MR_FREERUN_OFF |
                  ADC_MR_STARTUP_SUT64 | ADC_MR_SETTLING_AST17 | ADC_MR_ANACH_NONE |
                  ADC_MR_USEQ_NUM_ORDER |
                  ADC_MR_PRESCAL(AD_PRESCALE_FACTOR) |
                  ADC_MR_TRACKTIM(AD_TRACKING_CYCLES) |
                  ADC_MR_TRANSFER(AD_TRANSFER_CYCLES);

    ADC->ADC_IER = 0;             // no ADC interrupts
    ADC->ADC_CGR = 0;             // Gain = 1
    ADC->ADC_COR = 0;             // Single-ended, no offset

    // start first conversion
    ADC->ADC_CR = ADC_CR_START;
  }

#endif

// Reset peripherals and cpu
void HAL::resetHardware() {
  RSTC->RSTC_CR = RSTC_CR_KEY(0xA5) | RSTC_CR_PERRST | RSTC_CR_PROCRST;
}

#ifdef DUE_SOFTWARE_SPI
  // bitbanging transfer
  // run at ~100KHz (necessary for init)
  uint8_t HAL::spiTransfer(uint8_t b) { // using Mode 0
    for (int bits = 0; bits < 8; bits++) {
      if (b & 0x80) {
        WRITE(MOSI_PIN, HIGH);
      }
      else {
        WRITE(MOSI_PIN, LOW);
      }
      b <<= 1;

      WRITE(SCK_PIN, HIGH);
      delayMicroseconds(5);

      if (READ(MISO_PIN)) {
        b |= 1;
      }
      WRITE(SCK_PIN, LOW);
      delayMicroseconds(5);
    }
    return b;
  }

  void HAL::spiBegin() {
    SET_OUTPUT(SDSS);
    WRITE(SDSS, HIGH);
    SET_OUTPUT(SCK_PIN);
    SET_INPUT(MISO_PIN);
    SET_OUTPUT(MOSI_PIN);
  }

  void HAL::spiInit(uint8_t spiClock) {
    WRITE(SDSS, HIGH);
    WRITE(MOSI_PIN, HIGH);
    WRITE(SCK_PIN, LOW);
  }

  uint8_t HAL::spiReceive() {
    WRITE(SDSS, LOW);
    uint8_t b = spiTransfer(0xff);
    WRITE(SDSS, HIGH);
    return b;
  }

  void HAL::spiReadBlock(uint8_t* buf, uint16_t nbyte) {
    if (nbyte == 0) return;
    WRITE(SDSS, LOW);
    for (int i = 0; i < nbyte; i++) {
      buf[i] = spiTransfer(0xff);
    }
    WRITE(SDSS, HIGH);
  }

  void HAL::spiSend(uint8_t b) {
    WRITE(SDSS, LOW);
    uint8_t response = spiTransfer(b);
    WRITE(SDSS, HIGH);
  }

  void HAL::spiSend(const uint8_t* buf , size_t n) {
    uint8_t response;
    if (n == 0) return;
    WRITE(SDSS, LOW);
    for (uint16_t i = 0; i < n; i++) {
      response = spiTransfer(buf[i]);
    }
    WRITE(SDSS, HIGH);
  }

  void HAL::spiSendBlock(uint8_t token, const uint8_t* buf) {
    uint8_t response;

    WRITE(SDSS, LOW);
    response = spiTransfer(token);

    for (uint16_t i = 0; i < 512; i++) {
      response = spiTransfer(buf[i]);
    }
    WRITE(SDSS, HIGH);
  }

#else

  // --------------------------------------------------------------------------
  // hardware SPI
  // --------------------------------------------------------------------------
  #if MB(ALLIGATOR) || MB(ALLIGATOR_V3)
    bool spiInitMaded = false;
  #endif

  void HAL::spiBegin() {
    #if MB(ALLIGATOR) || MB(ALLIGATOR_V3)
      if (spiInitMaded == false) {
    #endif

    // Configre SPI pins
    PIO_Configure(
      g_APinDescription[SCK_PIN].pPort,
      g_APinDescription[SCK_PIN].ulPinType,
      g_APinDescription[SCK_PIN].ulPin,
      g_APinDescription[SCK_PIN].ulPinConfiguration
    );
    PIO_Configure(
      g_APinDescription[MOSI_PIN].pPort,
      g_APinDescription[MOSI_PIN].ulPinType,
      g_APinDescription[MOSI_PIN].ulPin,
      g_APinDescription[MOSI_PIN].ulPinConfiguration
    );
    PIO_Configure(
      g_APinDescription[MISO_PIN].pPort,
      g_APinDescription[MISO_PIN].ulPinType,
      g_APinDescription[MISO_PIN].ulPin,
      g_APinDescription[MISO_PIN].ulPinConfiguration
    );

    // set master mode, peripheral select, fault detection
    SPI_Configure(SPI0, ID_SPI0, SPI_MR_MSTR | SPI_MR_MODFDIS | SPI_MR_PS);
    SPI_Enable(SPI0);

    #if MB(ALLIGATOR) || MB(ALLIGATOR_V3)
      SET_OUTPUT(DAC0_SYNC);
      #if EXTRUDERS > 1
        SET_OUTPUT(DAC1_SYNC);
        WRITE(DAC1_SYNC, HIGH);
      #endif
      SET_OUTPUT(SPI_EEPROM1_CS);
      SET_OUTPUT(SPI_EEPROM2_CS);
      SET_OUTPUT(SPI_FLASH_CS);
      WRITE(DAC0_SYNC, HIGH);
      WRITE(SPI_EEPROM1_CS, HIGH );
      WRITE(SPI_EEPROM2_CS, HIGH );
      WRITE(SPI_FLASH_CS, HIGH );
      WRITE(SDSS, HIGH );
    #endif // MB(ALLIGATOR) || MB(ALLIGATOR_V3)

    PIO_Configure(
      g_APinDescription[SPI_PIN].pPort,
      g_APinDescription[SPI_PIN].ulPinType,
      g_APinDescription[SPI_PIN].ulPin,
      g_APinDescription[SPI_PIN].ulPinConfiguration
    );
    spiInit(1);
    #if MB(ALLIGATOR) || MB(ALLIGATOR_V3)
      spiInitMaded = true;
      }
    #endif

  }

  void HAL::spiInit(uint8_t spiClock) {
    #if MB(ALLIGATOR) || MB(ALLIGATOR_V3)
      if (spiInitMaded == false) {
    #endif

    if (spiClock > 4) spiClock = 1;

    #if MB(ALLIGATOR) || MB(ALLIGATOR_V3)
      // Set SPI mode 1, clock, select not active after transfer, with delay between transfers  
      SPI_ConfigureNPCS(SPI0, SPI_CHAN_DAC,
                        SPI_CSR_CSAAT | SPI_CSR_SCBR(spiDueDividors[spiClock]) |
                        SPI_CSR_DLYBCT(1));

      // Set SPI mode 0, clock, select not active after transfer, with delay between transfers 
      SPI_ConfigureNPCS(SPI0, SPI_CHAN_EEPROM1, SPI_CSR_NCPHA |
                        SPI_CSR_CSAAT | SPI_CSR_SCBR(spiDueDividors[spiClock]) |
                        SPI_CSR_DLYBCT(1));

    #endif // MB(ALLIGATOR) || MB(ALLIGATOR_V3) || MB(ULTRATRONICS)

    // Set SPI mode 0, clock, select not active after transfer, with delay between transfers
    SPI_ConfigureNPCS(SPI0, SPI_CHAN, SPI_CSR_NCPHA |
                      SPI_CSR_CSAAT | SPI_CSR_SCBR(spiDueDividors[spiClock]) |
                      SPI_CSR_DLYBCT(1));

    SPI_Enable(SPI0);

    #if MB(ALLIGATOR) || MB(ALLIGATOR_V3)
      spiInitMaded = true;
      }
    #endif
  }

  // Write single byte to SPI
  void HAL::spiSend(byte b) {
    // write byte with address and end transmission flag
    SPI0->SPI_TDR = (uint32_t)b | SPI_PCS(SPI_CHAN) | SPI_TDR_LASTXFER;
    // wait for transmit register empty
    while ((SPI0->SPI_SR & SPI_SR_TDRE) == 0);
    // wait for receive register
    while ((SPI0->SPI_SR & SPI_SR_RDRF) == 0);
    // clear status
    SPI0->SPI_RDR;
    //delayMicroseconds(1);
  }

  void HAL::spiSend(const uint8_t* buf, size_t n) {
    if (n == 0) return;
    for (size_t i = 0; i < n - 1; i++) {
      SPI0->SPI_TDR = (uint32_t)buf[i] | SPI_PCS(SPI_CHAN);
      while ((SPI0->SPI_SR & SPI_SR_TDRE) == 0);
      while ((SPI0->SPI_SR & SPI_SR_RDRF) == 0);
      SPI0->SPI_RDR;
      //        delayMicroseconds(1);
    }
    spiSend(buf[n - 1]);
  }

  // Read single byte from SPI
  uint8_t HAL::spiReceive() {
    // write dummy byte with address and end transmission flag
    SPI0->SPI_TDR = 0x000000FF | SPI_PCS(SPI_CHAN) | SPI_TDR_LASTXFER;
    // wait for transmit register empty
    while ((SPI0->SPI_SR & SPI_SR_TDRE) == 0);

    // wait for receive register
    while ((SPI0->SPI_SR & SPI_SR_RDRF) == 0);
    // get byte from receive register
    //delayMicroseconds(1);
    return SPI0->SPI_RDR;
  }

  #if MB(ALLIGATOR) || MB(ALLIGATOR_V3)

    void HAL::spiSend(uint32_t chan, byte b) {
      // wait for transmit register empty
      while ((SPI0->SPI_SR & SPI_SR_TDRE) == 0);
      // write byte with address and end transmission flag
      SPI0->SPI_TDR = (uint32_t)b | SPI_PCS(chan) | SPI_TDR_LASTXFER;
      // wait for receive register
      while ((SPI0->SPI_SR & SPI_SR_RDRF) == 0);
      // clear status
      while ((SPI0->SPI_SR & SPI_SR_RDRF) == 1) SPI0->SPI_RDR;
    }

    void HAL::spiSend(uint32_t chan, const uint8_t* buf, size_t n) {
      if (n == 0) return;
      for (uint32_t i = 0; i < n - 1; i++) {
        while ((SPI0->SPI_SR & SPI_SR_TDRE) == 0);
        SPI0->SPI_TDR = (uint32_t)buf[i] | SPI_PCS(chan);
        while ((SPI0->SPI_SR & SPI_SR_RDRF) == 0);
        while ((SPI0->SPI_SR & SPI_SR_RDRF) == 1) SPI0->SPI_RDR;
      }
      spiSend(chan, buf[n - 1]);
    }

    uint8_t HAL::spiReceive(uint32_t chan) {
      // wait for transmit register empty
      while ((SPI0->SPI_SR & SPI_SR_TDRE) == 0);
      while ((SPI0->SPI_SR & SPI_SR_RDRF) == 1) SPI0->SPI_RDR;

      // write dummy byte with address and end transmission flag
      SPI0->SPI_TDR = 0x000000FF | SPI_PCS(chan) | SPI_TDR_LASTXFER;

      // wait for receive register
      while ((SPI0->SPI_SR & SPI_SR_RDRF) == 0);
      // get byte from receive register
      return SPI0->SPI_RDR;
    }

  #endif // MB(ALLIGATOR) || MB(ALLIGATOR_V3)

  // Read from SPI into buffer
  void HAL::spiReadBlock(uint8_t* buf, uint16_t nbyte) {
    if (nbyte-- == 0) return;

    for (int i = 0; i < nbyte; i++) {
      //while ((SPI0->SPI_SR & SPI_SR_TDRE) == 0);
      SPI0->SPI_TDR = 0x000000FF | SPI_PCS(SPI_CHAN);
      while ((SPI0->SPI_SR & SPI_SR_RDRF) == 0);
      buf[i] = SPI0->SPI_RDR;
      // delayMicroseconds(1);
    }
    buf[nbyte] = spiReceive();
  }

  // Write from buffer to SPI
  void HAL::spiSendBlock(uint8_t token, const uint8_t* buf) {
    SPI0->SPI_TDR = (uint32_t)token | SPI_PCS(SPI_CHAN);
    while ((SPI0->SPI_SR & SPI_SR_TDRE) == 0);
    //while ((SPI0->SPI_SR & SPI_SR_RDRF) == 0);
    //SPI0->SPI_RDR;
    for (int i = 0; i < 511; i++) {
      SPI0->SPI_TDR = (uint32_t)buf[i] | SPI_PCS(SPI_CHAN);
      while ((SPI0->SPI_SR & SPI_SR_TDRE) == 0);
      while ((SPI0->SPI_SR & SPI_SR_RDRF) == 0);
      SPI0->SPI_RDR;
      // delayMicroseconds(1);
    }
    spiSend(buf[511]);
  }

#endif // DISABLED(SOFTWARE_SPI)

// --------------------------------------------------------------------------
// Analogic write to a PWM Pin
// --------------------------------------------------------------------------
static bool     PWMEnabled      = false;
static uint16_t PWMChanFreq[8]  = {0},
                PWMChanPeriod[8];

static const uint32_t PwmFastClock =  25000 * 255;        // fast PWM clock for Intel spec PWM fans that need 25kHz PWM
static const uint32_t PwmSlowClock = (25000 * 255) / 256; // slow PWM clock to allow us to get slow speeds

static inline uint32_t ConvertRange(float f, uint32_t top) { return LROUND(f * (float)top); }

// AnalogWritePwm to a PWM pin
// Return true if successful, false if we need to call software pwm
static bool AnalogWritePwm(const PinDescription& pinDesc, float ulValue, uint16_t freq) {

  const uint32_t chan = pinDesc.ulPWMChannel;

  if (freq == 0) {
    PWMChanFreq[chan] = freq;
    return false;
  }
  else if (PWMChanFreq[chan] != freq) {
    if (!PWMEnabled) {
      // PWM Startup code
      pmc_enable_periph_clk(PWM_INTERFACE_ID);
      PWMC_ConfigureClocks(PwmSlowClock, PwmFastClock, VARIANT_MCK);
      PWM->PWM_SCM = 0;     // ensure no sync channels
      PWMEnabled = true;
    }

    const bool useFastClock = (freq >= PwmFastClock / 65535);
    const uint32_t period = ((useFastClock) ? PwmFastClock : PwmSlowClock) / freq;
    const uint32_t duty = ConvertRange(ulValue, period);

    PWMChanFreq[chan] = freq;
    PWMChanPeriod[chan] = (uint16_t)period;

    // Set up the PWM channel
    // We need to work around a bug in the SAM PWM channels. Enabling a channel is supposed to clear the counter, but it doesn't.
    // A further complication is that on the SAM3X, the update-period register doesn't appear to work.
    // So we need to make sure the counter is less than the new period before we change the period.
    for (unsigned int j = 0; j < 5; ++j) {  // twice through should be enough, but just in case...
    
      PWMC_DisableChannel(PWM, chan);
      uint32_t oldCurrentVal = PWM->PWM_CH_NUM[chan].PWM_CCNT & 0xFFFF;
      if (oldCurrentVal < period || oldCurrentVal > 65536 - 10) // if counter is already small enough or about to wrap round, OK
        break;
      oldCurrentVal += 2;											// note: +1 doesn't work here, has to be at least +2
      PWM->PWM_CH_NUM[chan].PWM_CPRD = oldCurrentVal;				// change the period to be just greater than the counter
      PWM->PWM_CH_NUM[chan].PWM_CMR = PWM_CMR_CPRE_CLKB;			// use the fast clock to avoid waiting too long
      PWMC_EnableChannel(PWM, chan);
      for (unsigned int i = 0; i < 1000; ++i) {
        const uint32_t newCurrentVal = PWM->PWM_CH_NUM[chan].PWM_CCNT & 0xFFFF;
        if (newCurrentVal < period || newCurrentVal > oldCurrentVal)
          break;    // get out when we have wrapped round, or failed to
      }
    }

    PWMC_ConfigureChannel(PWM, chan, ((useFastClock) ? PWM_CMR_CPRE_CLKB : PWM_CMR_CPRE_CLKA), 0, 0);
    PWMC_SetDutyCycle(PWM, chan, duty);
    PWMC_SetPeriod(PWM, chan, period);
    PWMC_EnableChannel(PWM, chan);

    // Now setup the PWM output pin for PWM this channel - do this after configuring the PWM to avoid glitches
    ConfigurePin(pinDesc);
  }
  else {
    const uint32_t ul_period = (uint32_t)PWMChanPeriod[chan];
    PWMC_SetDutyCycle(PWM, chan, ConvertRange(ulValue, ul_period));
  }
  return true;
}

// --------------------------------------------------------------------------
// Analogic Write to a TC Pin
// --------------------------------------------------------------------------
const unsigned int numTcChannels = 9;

// Map from timer channel to TC channel number
static const uint8_t channelToChNo[] = { 0, 1, 2, 0, 1, 2, 0, 1, 2 };

// Map from timer channel to TC number
static Tc * const channelToTC[] = { TC0, TC0, TC0,
                                    TC1, TC1, TC1,
                                    TC2, TC2, TC2 };

// Map from timer channel to TIO number
static const uint8_t channelToId[] = {  ID_TC0, ID_TC1, ID_TC2,
                                        ID_TC3, ID_TC4, ID_TC5,
                                        ID_TC6, ID_TC7, ID_TC8 };

// Current frequency of each TC channel
static uint16_t TCChanFreq[numTcChannels] = {0};

static inline void TC_SetCMR_ChannelA(Tc *tc, uint32_t chan, uint32_t v) {
  tc->TC_CHANNEL[chan].TC_CMR = (tc->TC_CHANNEL[chan].TC_CMR & 0xFFF0FFFF) | v;
}
static inline void TC_SetCMR_ChannelB(Tc *tc, uint32_t chan, uint32_t v) {
  tc->TC_CHANNEL[chan].TC_CMR = (tc->TC_CHANNEL[chan].TC_CMR & 0xF0FFFFFF) | v;
}

static inline void TC_WriteCCR(Tc *tc, uint32_t chan, uint32_t v) {
  tc->TC_CHANNEL[chan].TC_CCR = v;
}

static inline uint32_t TC_read_ra(Tc *tc, uint32_t chan) {
  return tc->TC_CHANNEL[chan].TC_RA;
}
static inline uint32_t TC_read_rb(Tc *tc, uint32_t chan) {
  return tc->TC_CHANNEL[chan].TC_RB;
}
static inline uint32_t TC_read_rc(Tc *tc, uint32_t chan) {
  return tc->TC_CHANNEL[chan].TC_RC;
}

// AnalogWriteTc to a TC pin
// Return true if successful, false if we need to call software pwm
static bool AnalogWriteTc(const PinDescription& pinDesc, float ulValue, uint16_t freq) {

  const uint32_t chan = (uint32_t)pinDesc.ulTCChannel >> 1;
  if (freq == 0) {
    TCChanFreq[chan] = freq;
    return false;
  }
  else {
    Tc * const chTC = channelToTC[chan];
    const uint32_t chNo = channelToChNo[chan];
    const bool doInit = (TCChanFreq[chan] != freq);

    if (doInit) {
      TCChanFreq[chan] = freq;

      // Enable the peripheral clock to this timer
      pmc_enable_periph_clk(channelToId[chan]);

      // Set up the timer mode and top count
      TC_Configure(chTC, chNo,
              TC_CMR_TCCLKS_TIMER_CLOCK2 |    // clock is MCLK/8 to save a little power and avoid overflow later on
              TC_CMR_WAVE |                   // Waveform mode
              TC_CMR_WAVSEL_UP_RC |           // Counter running up and reset when equals to RC
              TC_CMR_EEVT_XC0 |               // Set external events from XC0 (this setup TIOB as output)
              TC_CMR_ACPA_CLEAR | TC_CMR_ACPC_CLEAR |
              TC_CMR_BCPB_CLEAR | TC_CMR_BCPC_CLEAR |
              TC_CMR_ASWTRG_SET | TC_CMR_BSWTRG_SET);	// Software trigger will let us set the output high
      const uint32_t top = (VARIANT_MCK / 8) / (uint32_t)freq;  // with 120MHz clock this varies between 228 (@ 65.535kHz) and 15 million (@ 1Hz)
      // The datasheet doesn't say how the period relates to the RC value, but from measurement it seems that we do not need to subtract one from top
      TC_SetRC(chTC, chNo, top);

      // When using TC channels to do PWM control of heaters with active low outputs on the Duet WiFi, if we don't take precautions
      // then we get a glitch straight after initialising the channel, because the compare output starts in the low state.
      // To avoid that, set the output high here if a high PWM was requested.
      if (ulValue >= 0.5)
        TC_WriteCCR(chTC, chan, TC_CCR_SWTRG);
    }

    const uint32_t threshold = ConvertRange(ulValue, TC_read_rc(chTC, chNo));
    if (threshold == 0) {
      if (((uint32_t)pinDesc.ulTCChannel & 1) == 0) {
        TC_SetRA(chTC, chNo, 1);
        TC_SetCMR_ChannelA(chTC, chNo, TC_CMR_ACPA_CLEAR | TC_CMR_ACPC_CLEAR);
      }
      else {
        TC_SetRB(chTC, chNo, 1);
        TC_SetCMR_ChannelB(chTC, chNo, TC_CMR_BCPB_CLEAR | TC_CMR_BCPC_CLEAR);
      }
    }
    else {
      if (((uint32_t)pinDesc.ulTCChannel & 1) == 0) {
        TC_SetRA(chTC, chNo, threshold);
        TC_SetCMR_ChannelA(chTC, chNo, TC_CMR_ACPA_CLEAR | TC_CMR_ACPC_SET);
      }
      else {
        TC_SetRB(chTC, chNo, threshold);
        TC_SetCMR_ChannelB(chTC, chNo, TC_CMR_BCPB_CLEAR | TC_CMR_BCPC_SET);
      }
    }

    if (doInit) {
      ConfigurePin(pinDesc);
      TC_Start(chTC, chNo);
    }
  }
  return true;
}

bool HAL::AnalogWrite(Pin pin, const uint8_t value, const uint16_t freq) {

  if (isnan(value)) return true;

  const float ulValue = (float)value / 255.0;
  const PinDescription& pinDesc = g_APinDescription[pin];
  const uint32_t attr = pinDesc.ulPinAttribute;

  if ((attr & PIN_ATTR_PWM) != 0)
    return AnalogWritePwm(pinDesc, ulValue, freq);
  else if ((attr & PIN_ATTR_TIMER) != 0)
    return AnalogWriteTc(pinDesc, ulValue, freq);

  return false;
}

// --------------------------------------------------------------------------
// eeprom
// --------------------------------------------------------------------------

#ifdef SPI_EEPROM

  static constexpr uint8_t CMD_WREN   = 6;  // WREN
  static constexpr uint8_t CMD_READ   = 3;  // READ
  static constexpr uint8_t CMD_WRITE  = 2;  // WRITE

  uint8_t eeprom_read_byte(uint8_t* pos) {
    uint8_t eeprom_temp[3] = {0};
    uint8_t v;

    // set read location
    // begin transmission from device
    eeprom_temp[0] = CMD_READ;
    eeprom_temp[1] = ((unsigned)pos >> 8) & 0xFF; // addr High
    eeprom_temp[2] = (unsigned)pos & 0xFF;        // addr Low
    digitalWrite(SPI_EEPROM1_CS, HIGH);
    digitalWrite(SPI_EEPROM1_CS, LOW);
    HAL::spiSend(SPI_CHAN_EEPROM1, eeprom_temp, 3);

    v = HAL::spiReceive(SPI_CHAN_EEPROM1);
    digitalWrite(SPI_EEPROM1_CS, HIGH);
    return v;
  }

  void eeprom_read_block(void* pos, const void* eeprom_address, size_t n) {
    uint8_t eeprom_temp[3] = {0};
    uint8_t *p_pos = (uint8_t *)pos;

    // set read location
    // begin transmission from device
    eeprom_temp[0] = CMD_READ;
    eeprom_temp[1] = ((unsigned)eeprom_address >> 8) & 0xFF; // addr High
    eeprom_temp[2] = (unsigned)eeprom_address & 0xFF;        // addr Low
    digitalWrite(SPI_EEPROM1_CS, HIGH);
    digitalWrite(SPI_EEPROM1_CS, LOW);
    HAL::spiSend(SPI_CHAN_EEPROM1, eeprom_temp, 3);

    while (n--)
      *p_pos++ = HAL::spiReceive(SPI_CHAN_EEPROM1);
    digitalWrite(SPI_EEPROM1_CS, HIGH);
  }

  void eeprom_write_byte(uint8_t* pos, uint8_t value) {
    uint8_t eeprom_temp[3] = {0};

    // write enable
    eeprom_temp[0] = CMD_WREN;
    digitalWrite(SPI_EEPROM1_CS, LOW);
    HAL::spiSend(SPI_CHAN_EEPROM1, eeprom_temp, 1);
    digitalWrite(SPI_EEPROM1_CS, HIGH);
    HAL::delayMilliseconds(1);

    // write addr
    eeprom_temp[0] = CMD_WRITE;
    eeprom_temp[1] = ((unsigned)pos >> 8) & 0xFF; // addr High
    eeprom_temp[2] = (unsigned)pos & 0xFF;        // addr Low
    digitalWrite(SPI_EEPROM1_CS, LOW);
    HAL::spiSend(SPI_CHAN_EEPROM1, eeprom_temp, 3);

    HAL::spiSend(SPI_CHAN_EEPROM1, value);
    digitalWrite(SPI_EEPROM1_CS, HIGH);
    HAL::delayMilliseconds(7);   // wait for page write to complete
  }

  void eeprom_update_block(const void* pos, void* eeprom_address, size_t n) {
    uint8_t eeprom_temp[3] = {0};

    // write enable
    eeprom_temp[0] = CMD_WREN;
    digitalWrite(SPI_EEPROM1_CS, LOW);
    HAL::spiSend(SPI_CHAN_EEPROM1, eeprom_temp, 1);
    digitalWrite(SPI_EEPROM1_CS, HIGH);
    HAL::delayMilliseconds(1);

    // write addr
    eeprom_temp[0] = CMD_WRITE;
    eeprom_temp[1] = ((unsigned)eeprom_address >> 8) & 0xFF; // addr High
    eeprom_temp[2] = (unsigned)eeprom_address & 0xFF;        // addr Low
    digitalWrite(SPI_EEPROM1_CS, LOW);
    HAL::spiSend(SPI_CHAN_EEPROM1, eeprom_temp, 3);

    HAL::spiSend(SPI_CHAN_EEPROM1, (const uint8_t*)pos, n);
    digitalWrite(SPI_EEPROM1_CS, HIGH);
    HAL::delayMilliseconds(7); // wait for page write to complete
  }

#else // !SPI_EEPROM

  #include <Wire.h>

  static bool eeprom_initialised = false;
  static uint8_t eeprom_device_address = 0x50;

  static void eeprom_init(void) {
    if (!eeprom_initialised) {
      Wire.begin();
      eeprom_initialised = true;
    }
  }

  unsigned char eeprom_read_byte(unsigned char *pos) {
    byte data = 0xFF;
    unsigned eeprom_address = (unsigned) pos;

    eeprom_init();

    Wire.beginTransmission(eeprom_device_address);
    Wire.write((int)(eeprom_address >> 8));   // MSB
    Wire.write((int)(eeprom_address & 0xFF)); // LSB
    Wire.endTransmission();
    Wire.requestFrom(eeprom_device_address, (byte)1);
    if (Wire.available())
      data = Wire.read();
    return data;
  }

  // maybe let's not read more than 30 or 32 bytes at a time!
  void eeprom_read_block(void* pos, const void* eeprom_address, size_t n) {
    eeprom_init();

    Wire.beginTransmission(eeprom_device_address);
    Wire.write((int)((unsigned)eeprom_address >> 8));   // MSB
    Wire.write((int)((unsigned)eeprom_address & 0xFF)); // LSB
    Wire.endTransmission();
    Wire.requestFrom(eeprom_device_address, (byte)n);
    for (byte c = 0; c < n; c++ )
      if (Wire.available()) *((uint8_t*)pos + c) = Wire.read();
  }

  void eeprom_write_byte(uint8_t *pos, uint8_t value) {
    unsigned eeprom_address = (unsigned) pos;

    eeprom_init();

    Wire.beginTransmission(eeprom_device_address);
    Wire.write((int)(eeprom_address >> 8));   // MSB
    Wire.write((int)(eeprom_address & 0xFF)); // LSB
    Wire.write(value);
    Wire.endTransmission();

    // wait for write cycle to complete
    // this could be done more efficiently with "acknowledge polling"
    HAL::delayMilliseconds(5);
  }

  // WARNING: address is a page address, 6-bit end will wrap around
  // also, data can be maximum of about 30 bytes, because the Wire library has a buffer of 32 bytes
  void eeprom_update_block(const void* pos, void* eeprom_address, size_t n) {
    uint8_t eeprom_temp[32] = {0};
    uint8_t flag = 0;

    eeprom_init();

    Wire.beginTransmission(eeprom_device_address);
    Wire.write((int)((unsigned)eeprom_address >> 8));   // MSB
    Wire.write((int)((unsigned)eeprom_address & 0xFF)); // LSB
    Wire.endTransmission();
    Wire.requestFrom(eeprom_device_address, (byte)n);
    for (byte c = 0; c < n; c++) {
      if (Wire.available()) eeprom_temp[c] = Wire.read();
      if (flag = (eeprom_temp[c] ^ *((uint8_t*)pos + c))) break;
    }

    if (flag) {
      Wire.beginTransmission(eeprom_device_address);
      Wire.write((int)((unsigned)eeprom_address >> 8));   // MSB
      Wire.write((int)((unsigned)eeprom_address & 0xFF)); // LSB
      Wire.write((uint8_t*)(pos), n);
      Wire.endTransmission();

      // wait for write cycle to complete
      // this could be done more efficiently with "acknowledge polling"
      HAL::delayMilliseconds(5);
    }
  }

#endif // I2C_EEPROM

/**
 * Timer 0 is is called 3906 timer per second.
 * It is used to update pwm values for heater and some other frequent jobs.
 *
 *  - Manage PWM to all the heaters and fan
 *  - Prepare or Measure one of the raw ADC sensor values
 *  - Step the babysteps value for each axis towards 0
 *  - For PINS_DEBUGGING, monitor and report endstop pins
 *  - For ENDSTOP_INTERRUPTS_FEATURE check endstops if flagged
 */
HAL_TEMP_TIMER_ISR {

  HAL_timer_isr_prologue(TEMP_TIMER);

  // Allow UART ISRs
  _DISABLE_ISRs();

  static uint8_t  pwm_count_heater        = 0,
                  pwm_count_fan           = 0;

  #if HAS_TEMP_HOTEND
    static uint8_t  pwm_heater_pos[HOTENDS] = { 0 };
    static bool     pwm_heater_hd[HOTENDS]  = ARRAY_BY_HOTENDS(true);
  #endif
  #if HAS_HEATER_BED
    static uint8_t  pwm_bed_pos = 0;
    static bool     pwm_bed_hd  = true;
  #endif
  #if HAS_HEATER_CHAMBER
    static uint8_t  pwm_chamber_pos = 0;
    static bool     pwm_chamber_hd  = true;
  #endif
  #if HAS_COOLER
    static uint8_t  pwm_cooler_pos = 0;
    static bool     pwm_cooler_hd  = true;
  #endif
  #if FAN_COUNT > 0
    static uint8_t  pwm_fan_pos[FAN_COUNT]  = { 0 };
    static bool     pwm_fan_hd[FAN_COUNT]   = ARRAY_BY_FANS(true);
  #endif
  #if HAS_CONTROLLERFAN
    static uint8_t  pwm_controller_pos = 0;
    static bool     pwm_controller_hd  = true;
  #endif

  #if ENABLED(FILAMENT_SENSOR)
    static unsigned long raw_filwidth_value = 0;
  #endif

  /**
   * Harware PWM or TC
   */
  #if HOTENDS > 0
    if (pwm_heater_hd[0])
      pwm_heater_hd[0] = HAL::AnalogWrite(HEATER_0_PIN, thermalManager.soft_pwm[0], HEATER_PWM_FREQ);
    #if HOTENDS > 1
      if (pwm_heater_hd[1])
        pwm_heater_hd[1] = HAL::AnalogWrite(HEATER_1_PIN, thermalManager.soft_pwm[1], HEATER_PWM_FREQ);
      #if HOTENDS > 2
        if (pwm_heater_hd[2])
          pwm_heater_hd[2] = HAL::AnalogWrite(HEATER_2_PIN, thermalManager.soft_pwm[2], HEATER_PWM_FREQ);
        #if HOTENDS > 3
          if (pwm_heater_hd[3])
            pwm_heater_hd[3] = HAL::AnalogWrite(HEATER_3_PIN, thermalManager.soft_pwm[3], HEATER_PWM_FREQ);
        #endif
      #endif
    #endif
  #endif

  #if HAS_HEATER_BED && HAS_TEMP_BED
    if (pwm_bed_hd)
      pwm_bed_hd = HAL::AnalogWrite(HEATER_BED_PIN, thermalManager.soft_pwm_bed, HEATER_PWM_FREQ);
  #endif

  #if HAS_HEATER_CHAMBER && HAS_TEMP_CHAMBER
    if (pwm_chamber_hd)
      pwm_chamber_hd = HAL::AnalogWrite(HEATER_CHAMBER_PIN, thermalManager.soft_pwm_chamber, HEATER_PWM_FREQ);
  #endif

  #if HAS_COOLER && HAS_TEMP_COOLER
    if (pwm_cooler_hd)
      pwm_cooler_hd = HAL::AnalogWrite(COOLER_PIN, thermalManager.soft_pwm_cooler, HEATER_PWM_FREQ);
  #endif

  #if HAS_FAN0
    if (pwm_fan_hd[0])
      pwm_fan_hd[0] = HAL::AnalogWrite(FAN_PIN, fanSpeeds[0], FAN_PWM_FREQ);
  #endif
  #if HAS_FAN1
    if (pwm_fan_hd[1])
      pwm_fan_hd[1] = HAL::AnalogWrite(FAN1_PIN, fanSpeeds[1], FAN_PWM_FREQ);
  #endif
  #if HAS_FAN2
    if (pwm_fan_hd[2])
      pwm_fan_hd[2] = HAL::AnalogWrite(FAN2_PIN, fanSpeeds[2], FAN_PWM_FREQ);
  #endif
  #if HAS_FAN3
    if (pwm_fan_hd[3])
      pwm_fan_hd[3] = HAL::AnalogWrite(FAN3_PIN, fanSpeeds[3], FAN_PWM_FREQ);
  #endif
  #if HAS_CONTROLLERFAN
    if (pwm_controller_hd)
      pwm_controller_hd = HAL::AnalogWrite(CONTROLLERFAN_PIN, controller_fanSpeeds, FAN_PWM_FREQ);
  #endif

  /**
   * Standard PWM modulation
   */
  if (pwm_count_heater == 0) {
    #if HOTENDS > 0
      if (!pwm_heater_hd[0] && (pwm_heater_pos[0] = (thermalManager.soft_pwm[0] & HEATER_PWM_MASK)) > 0)
        WRITE_HEATER_0(HIGH);
      #if HOTENDS > 1
        if (!pwm_heater_hd[1] && (pwm_heater_pos[1] = (thermalManager.soft_pwm[1] & HEATER_PWM_MASK)) > 0)
          WRITE_HEATER_1(HIGH);
        #if HOTENDS > 2
          if (!pwm_heater_hd[2] && (pwm_heater_pos[2] = (thermalManager.soft_pwm[2] & HEATER_PWM_MASK)) > 0)
            WRITE_HEATER_2(HIGH);
          #if HOTENDS > 3
            if (!pwm_heater_hd[3] && (pwm_heater_pos[3] = (thermalManager.soft_pwm[3] & HEATER_PWM_MASK)) > 0)
              WRITE_HEATER_0(HIGH);
          #endif
        #endif
      #endif
    #endif

    #if HAS_HEATER_BED && HAS_TEMP_BED
      if (!pwm_bed_hd && (pwm_bed_pos = (thermalManager.soft_pwm_bed & HEATER_PWM_MASK)) > 0)
        WRITE_HEATER_BED(HIGH);
    #endif

    #if HAS_HEATER_CHAMBER && HAS_TEMP_CHAMBER
      if (!pwm_chamber_hd && (pwm_chamber_pos = (thermalManager.soft_pwm_chamber & HEATER_PWM_MASK)) > 0)
        WRITE_HEATER_CHAMBER(HIGH);
    #endif

    #if HAS_COOLER && !ENABLED(FAST_PWM_COOLER) && HAS_TEMP_COOLER
      if (!pwm_cooler_hd && (pwm_cooler_pos = (thermalManager.soft_pwm_cooler & HEATER_PWM_MASK)) > 0)
        WRITE_COOLER(HIGH);
    #endif

  }

  if (pwm_count_fan == 0) {
    #if HAS_FAN0
      if (!pwm_fan_hd[0] && (pwm_fan_pos[0] = (fanSpeeds[0] & FAN_PWM_MASK)) > 0)
        WRITE_FAN(HIGH);
    #endif
    #if HAS_FAN1
      if (!pwm_fan_hd[1] && (pwm_fan_pos[1] = (fanSpeeds[1] & FAN_PWM_MASK)) > 0)
        WRITE_FAN1(HIGH);
    #endif
    #if HAS_FAN2
      if (!pwm_fan_hd[2] && (pwm_fan_pos[2] = (fanSpeeds[2] & FAN_PWM_MASK)) > 0)
        WRITE_FAN2(HIGH);
    #endif
    #if HAS_FAN3
      if (!pwm_fan_hd[3] && (pwm_fan_pos[3] = (fanSpeeds[3] & FAN_PWM_MASK)) > 0)
        WRITE_FAN3(HIGH);
    #endif
    #if HAS_CONTROLLERFAN
      if (!pwm_controller_hd && (pwm_controller_pos = (controller_fanSpeeds & FAN_PWM_MASK)) > 0)
        WRITE(CONTROLLERFAN_PIN, HIGH);
    #endif
  }

  #if HOTENDS > 0
    if (!pwm_heater_hd[0] && pwm_heater_pos[0] == pwm_count_heater && pwm_heater_pos[0] != HEATER_PWM_MASK) WRITE_HEATER_0(LOW);
    #if HOTENDS > 1
      if (!pwm_heater_hd[1] && pwm_heater_pos[1] == pwm_count_heater && pwm_heater_pos[1] != HEATER_PWM_MASK) WRITE_HEATER_1(LOW);
      #if HOTENDS > 2
        if (!pwm_heater_hd[2] && pwm_heater_pos[2] == pwm_count_heater && pwm_heater_pos[2] != HEATER_PWM_MASK) WRITE_HEATER_2(LOW);
        #if HOTENDS > 3
          if (!pwm_heater_hd[3] && pwm_heater_pos[3] == pwm_count_heater && pwm_heater_pos[3] != HEATER_PWM_MASK) WRITE_HEATER_3(LOW);
        #endif
      #endif
    #endif
  #endif

  #if HAS_HEATER_BED && HAS_TEMP_BED
    if (!pwm_bed_hd && pwm_bed_pos == pwm_count_heater && pwm_bed_pos != HEATER_PWM_MASK) WRITE_HEATER_BED(LOW);
  #endif

  #if HAS_HEATER_CHAMBER && HAS_TEMP_CHAMBER
    if (!pwm_chamber_hd && pwm_chamber_pos == pwm_count_heater && pwm_chamber_pos != HEATER_PWM_MASK) WRITE_HEATER_CHAMBER(LOW);
  #endif

  #if HAS_COOLER && !ENABLED(FAST_PWM_COOLER) && HAS_TEMP_COOLER
    if (!pwm_cooler_hd && pwm_cooler_pos == pwm_count_heater && pwm_cooler_pos != HEATER_PWM_MASK) WRITE_COOLER(LOW);
  #endif

  #if ENABLED(FAN_KICKSTART_TIME)
    if (fanKickstart == 0)
  #endif
  {
    #if HAS_FAN0
      if (!pwm_fan_hd[0] && pwm_fan_pos[0] == pwm_count_fan && pwm_fan_pos[0] != FAN_PWM_MASK)
        WRITE_FAN(LOW);
    #endif
    #if HAS_FAN1
      if (!pwm_fan_hd[1] && pwm_fan_pos[1] == pwm_count_fan && pwm_fan_pos[1] != FAN_PWM_MASK)
        WRITE_FAN1(LOW);
    #endif
    #if HAS_FAN2
      if (!pwm_fan_hd[2] && pwm_fan_pos[2] == pwm_count_fan && pwm_fan_pos[2] != FAN_PWM_MASK)
        WRITE_FAN2(LOW);
    #endif
    #if HAS_FAN3
      if (!pwm_fan_hd[3] && pwm_fan_pos[3] == pwm_count_fan && pwm_fan_pos[3] != FAN_PWM_MASK)
        WRITE_FAN3(LOW);
    #endif
    #if HAS_CONTROLLERFAN
      if (!pwm_controller_hd && pwm_controller_pos == pwm_count_fan && controller_fanSpeeds != FAN_PWM_MASK)
        WRITE(CONTROLLERFAN_PIN, LOW);
    #endif
  }

  // Calculation cycle approximate a 100ms
  cycle_100ms++;
  if (cycle_100ms >= 390) {
    cycle_100ms = 0;
    HAL::execute_100ms = true;
    #if ENABLED(FAN_KICKSTART_TIME)
      if (fanKickstart) fanKickstart--;
    #endif
  }

  // read analog values
  #if ANALOG_INPUTS > 0

    if ((ADC->ADC_ISR & adcEnable) == adcEnable) { // conversion finished?
      adcCounter++;
      for (int i = 0; i < ANALOG_INPUTS; i++) {
        int32_t cur = ADC->ADC_CDR[HAL::pinToAdcChannel(AnalogInputChannels[i])];
        if (i != MCU_SENSOR_INDEX) cur = (cur >> (2 - ANALOG_REDUCE_BITS)); // Convert to 10 bit result
        AnalogInputRead[i] += cur;
        adcSamplesMin[i] = min(adcSamplesMin[i], cur);
        adcSamplesMax[i] = max(adcSamplesMax[i], cur);
        if (adcCounter >= NUM_ADC_SAMPLES) { // store new conversion result
          AnalogInputRead[i] = AnalogInputRead[i] + (1 << (OVERSAMPLENR - 1)) - (adcSamplesMin[i] + adcSamplesMax[i]);
          adcSamplesMin[i] = 100000;
          adcSamplesMax[i] = 0;
          AnalogSamplesSum[i] -= AnalogSamples[i][adcSamplePos];
          AnalogSamplesSum[i] += (AnalogSamples[i][adcSamplePos] = AnalogInputRead[i] >> OVERSAMPLENR);
          HAL::AnalogInputValues[i] = AnalogSamplesSum[i] / MEDIAN_COUNT;
          AnalogInputRead[i] = 0;
        } // adcCounter >= NUM_ADC_SAMPLES
      } // for i
      if (adcCounter >= NUM_ADC_SAMPLES) {
        adcCounter = 0;
        adcSamplePos++;
        if (adcSamplePos >= MEDIAN_COUNT) {
          adcSamplePos = 0;
          Analog_is_ready = true;
        }
      }
      ADC->ADC_CR = ADC_CR_START; // reread values
    }

    // Update the raw values if they've been read. Else we could be updating them during reading.
    if (Analog_is_ready) thermalManager.set_current_temp_raw();

  #endif

  pwm_count_heater  += HEATER_PWM_STEP;
  pwm_count_fan     += FAN_PWM_STEP;

  #if ENABLED(BABYSTEPPING)
    LOOP_XYZ(axis) {
      int curTodo = thermalManager.babystepsTodo[axis]; //get rid of volatile for performance

      if (curTodo > 0) {
        stepper.babystep((AxisEnum)axis,/*fwd*/true);
        thermalManager.babystepsTodo[axis]--; //fewer to do next time
      }
      else if (curTodo < 0) {
        stepper.babystep((AxisEnum)axis,/*fwd*/false);
        thermalManager.babystepsTodo[axis]++; //fewer to do next time
      }
    }
  #endif //BABYSTEPPING

  #if ENABLED(PINS_DEBUGGING)
    extern bool endstop_monitor_flag;
    // run the endstop monitor at 15Hz
    static uint8_t endstop_monitor_count = 16;  // offset this check from the others
    if (endstop_monitor_flag) {
      endstop_monitor_count += _BV(1);  //  15 Hz
      endstop_monitor_count &= 0x7F;
      if (!endstop_monitor_count) endstops.endstop_monitor();  // report changes in endstop status
    }
  #endif

  #if ENABLED(ENDSTOP_INTERRUPTS_FEATURE)

    extern volatile uint8_t e_hit;

    if (e_hit && ENDSTOPS_ENABLED) {
      endstops.update();  // call endstop update routine
      e_hit--;
    }
  #endif

  _ENABLE_ISRs(); // re-enable ISRs

}

#endif // ARDUINO_ARCH_SAM
