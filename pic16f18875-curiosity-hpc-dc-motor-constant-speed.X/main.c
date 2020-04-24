/**
  Generated Main Source File

  Company:
    Microchip Technology Inc.

  File Name:
    main.c

  Summary:
    This is the main file generated using PIC10 / PIC12 / PIC16 / PIC18 MCUs

  Description:
    This header file provides implementations for driver APIs for all modules selected in the GUI.
    Generation Information :
        Product Revision  :  PIC10 / PIC12 / PIC16 / PIC18 MCUs - 1.79.0
        Device            :  PIC16F18875
        Driver Version    :  2.00
*/

/*
    (c) 2018 Microchip Technology Inc. and its subsidiaries. 
    
    Subject to your compliance with these terms, you may use Microchip software and any 
    derivatives exclusively with Microchip products. It is your responsibility to comply with third party 
    license terms applicable to your use of third party software (including open source software) that 
    may accompany Microchip software.
    
    THIS SOFTWARE IS SUPPLIED BY MICROCHIP "AS IS". NO WARRANTIES, WHETHER 
    EXPRESS, IMPLIED OR STATUTORY, APPLY TO THIS SOFTWARE, INCLUDING ANY 
    IMPLIED WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY, AND FITNESS 
    FOR A PARTICULAR PURPOSE.
    
    IN NO EVENT WILL MICROCHIP BE LIABLE FOR ANY INDIRECT, SPECIAL, PUNITIVE, 
    INCIDENTAL OR CONSEQUENTIAL LOSS, DAMAGE, COST OR EXPENSE OF ANY KIND 
    WHATSOEVER RELATED TO THE SOFTWARE, HOWEVER CAUSED, EVEN IF MICROCHIP 
    HAS BEEN ADVISED OF THE POSSIBILITY OR THE DAMAGES ARE FORESEEABLE. TO 
    THE FULLEST EXTENT ALLOWED BY LAW, MICROCHIP'S TOTAL LIABILITY ON ALL 
    CLAIMS IN ANY WAY RELATED TO THIS SOFTWARE WILL NOT EXCEED THE AMOUNT 
    OF FEES, IF ANY, THAT YOU HAVE PAID DIRECTLY TO MICROCHIP FOR THIS 
    SOFTWARE.
*/

#include "mcc_generated_files/mcc.h"

/*
                         Main application
 */
#define HOLES_PER_REV 24          /* holes per revolution */
#define SECONDS_PER_MIN 60
#define MICROSECONDS_PER_SECOND 1000000.0
#define PER_STEPS_TO_MICROS 32.0  /* SMT time step is set to 32 microseconds */
#define RPM_GAP_TOLERANCE  100    /* sampling rate is higher than RPM sensor, so there is a timeout */

#define PWM_DUTY_MIN 0
#define PWM_DUTY_MAX 511

#define ADC_POT_MIN  0
#define ADC_POT_MAX  1023

#define RPM_MIN      0
#define RPM_MAX      100.0

#define OUTPUT_MIN   0.0
#define OUTPUT_MAX   1.0

#define INTEGRAL_MIN -10.0
#define INTEGRAL_MAX  70.0

/* PI step time in s */
#define DT 0.02
/* PI constants */
#define KP 0.007
#define KI 0.007

volatile uint32_t sharedPeriod;
volatile uint16_t sharedPot;
volatile bool     sharedTimerFlag;


void AnalogHandler(void)
{
    sharedPot = ADCC_GetFilterValue();
}

/* This is configured to trigger once at every 20 ms */
void TimerHandler(void)
{
    sharedTimerFlag = 1;
}

void WaitForTimer(void)
{
    while(sharedTimerFlag == 0)
        ;
    sharedTimerFlag = 0;
}

float ComputeRpm(uint32_t period)
{
    float rpm;
    static uint32_t lastPeriod = 0;
    static uint8_t waitingCounter = RPM_GAP_TOLERANCE;
    
    /* this timeout is implemented because ComputeRpm function 
     * is called more often than SMT interrupt, especially at low RPM  */
    if(period == 0)
    {
        if(waitingCounter > 0)
        {
            waitingCounter--;
            period = lastPeriod;
        }
        else
            period = 0;
    }
    else
    {
        lastPeriod = period;
        waitingCounter = RPM_GAP_TOLERANCE;
    }
    /* compute RPM (rotations per minute) */
    if(period != 0)
        rpm = (MICROSECONDS_PER_SECOND * SECONDS_PER_MIN)\
            / (HOLES_PER_REV * PER_STEPS_TO_MICROS * (float)period);
    else
        rpm = 0;
    return rpm;
}

/* scales the integer ADC value to a float needed by PI */
float ComputeRef(uint16_t pot)
{
    float k = (RPM_MAX - RPM_MIN) / (float)(ADC_POT_MAX - ADC_POT_MIN);
    return RPM_MAX - (k*(float)pot);
}

/* scales the float value of the PI output to an integer for PWM duty cycle */
uint16_t ComputeDuty(float output)
{
    float k = (float)(PWM_DUTY_MAX - PWM_DUTY_MIN) / (OUTPUT_MAX - OUTPUT_MIN);
    return PWM_DUTY_MIN + (uint16_t)(k*(float)output);
}

/* Proportional-integral with output's saturation */
float ComputePI(float refVal, float feedbackVal)
{
    float output, error;
    static float integral = 0;

    error = refVal - feedbackVal;
    integral += error * DT;
    
    /* integral term is saturated to avoid integral windup */
    if(integral > INTEGRAL_MAX)      integral = INTEGRAL_MAX;
    else if(integral < INTEGRAL_MIN) integral = INTEGRAL_MIN;
    
    /* compute the output */
    output = KP * error + KI * integral;
    
    /* saturation: the output is bounded into [OUTPUT_MIN ... OUTPUT_MAX] */
    if(output > OUTPUT_MAX)      output = OUTPUT_MAX;
    else if(output < OUTPUT_MIN) output = OUTPUT_MIN;
    
    return output;
}

void main(void)
{   
    uint32_t periodVal;
    uint16_t potVal, dutyVal;
    float    reference_rpm, feedback_rpm, output;

    /* initialize the device */
    SYSTEM_Initialize();
    TMR4_SetInterruptHandler(TimerHandler);
    ADCC_SetADIInterruptHandler(AnalogHandler);
    ADCC_StartConversion(Pot);

    /* Enable the Global Interrupts */
    INTERRUPT_GlobalInterruptEnable();

    /* Enable the Peripheral Interrupts */
    INTERRUPT_PeripheralInterruptEnable();
    
    while (1)
    {
        /* atomic access to shared variables */
        INTERRUPT_GlobalInterruptDisable();
        potVal    = sharedPot;
        periodVal = sharedPeriod;
        sharedPeriod = 0;
        INTERRUPT_GlobalInterruptEnable();
        
        reference_rpm = ComputeRef(potVal);
        feedback_rpm  = ComputeRpm(periodVal);
        output        = ComputePI(reference_rpm, feedback_rpm);
        dutyVal       = ComputeDuty(output);
        PWM6_LoadDutyValue( dutyVal );

        WaitForTimer();
    }
}
/**
 End of File
*/