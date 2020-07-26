/* 
 * File:   newmain.c
 * PIC Micro Controller: PIC16f1459 using internal oscillator 
 * Author: Zylopfa
 * Description:
 *   Contraption with 1 big red button,
 *   Mp3 Player Module
 *   Potentiometer to set time interval.
 * 
 *   Mp3 module power and its Play/Pause button and 
 *   the big red buttons LED are controlled through
 *   3 mosfets.
 * 
 *   Potentiometer is read in the loop constantly (if changed more than +-1 global is updated)
 * 
 *   Clicking the big read button starts the one and only track on the sd card
 *   by giving power to the mp3 board through mosfet.
 * 
 *   After the initial seconds of instructions, the track is paused by pressing Play/Pause,
 *   by activating mosfet.
 * 
 *   A new timer is started with random amount based on the POTMETER reading stored.
 *   When this timer times out, the track is un-paused and we set another timer
 *   to stop the track playing and power off the mp3 module.
 *     
 *   At any time can you press the button, in order to stop the timing, the red light
 *   will turn off then, and you can start it again at your leasure.
 * 
 *   At the end of the horn sound the led will turn off and you can start a 
 *   new game by pressing the big red button again!
 * 
 *   DEBUG:
 *    set global variable DEBUG = 1, in order to initialize UART 9600 8N1,
 *    in order to read debug message in your terminal program.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <xc.h>
#include <time.h>


typedef unsigned short  uint16_t;
#define __DEFINED_uint16_t

// 4 is 4 seconds
// 1 is 150.75 ms
// 4 sec module start up time 
#define TMR1_INTRO_TICKER_FACTOR    64
#define TMR1_HORN_TICKER_FACTOR    27

#define _XTAL_FREQ 16000000

#pragma config FOSC = INTOSC //Intrenal RC oscillator Enabled
#pragma config WDTE = OFF    //Watch Dog Timer Disabled.
#pragma config PWRTE= ON     //Power Up Timer Enabled
#pragma config MCLRE = OFF   //MCLR pin function Disabled
#pragma config CP = OFF      //Flash Program Memory Code Protection Disabled
#pragma config BOREN = ON    //Brown-out Reset disabled
#pragma config CLKOUTEN = OFF//CLKOUT function is disabled.
#pragma config IESO = OFF    //Internal/External Switchover Mode is disabled
#pragma config FCMEN = OFF   //Fail-Safe Clock Monitor is disabled
#pragma config WRT = OFF     //Flash Memory Self-Write Protection Off
#pragma config CPUDIV = CLKDIV3//CPU system clock divided by 3
#pragma config USBLSCLK = 48MHz //System clock expects 48 MHz, FS/LS USB CLKENs divide-by is set to 8
#pragma config PLLMULT = 3x //3x Output Frequency Selected
#pragma config PLLEN = ENABLED //3x or 4x PLL Enabled
#pragma config STVREN = OFF //Stack Overflow or Underflow will not cause a Reset
#pragma config BORV = HI //Brown-out Reset Voltage (Vbor), high trip point selected.
#pragma config LPBOR = OFF //Low-Power BOR is disabled
#pragma config LVP = OFF //High-voltage on MCLR/VPP must be used for programming

// RA4, AN3
#define POT1   PORTAbits.RA4
#define POT1TRIS TRISAbits.TRISA4

#define REDLED  PORTBbits.RB4
#define REDLEDTRIS TRISBbits.TRISB4
#define REDLEDLAT LATBbits.LATB4

#define REDBUTTON PORTAbits.RA0

#define MP3PWRUP PORTBbits.RB6
#define MP3PWRUPTRIS TRISBbits.TRISB6
#define MP3PWRUPLAT LATBbits.LATB6

#define MP3PAUSE PORTCbits.RC2
#define MP3PAUSETRIS TRISCbits.TRISC2
#define MP3PAUSELAT LATCbits.LATC2

volatile uint16_t timer1ReloadVal;
void (*TMR1_InterruptHandler)(void);
void TMR1_SetInterruptHandler(void (* InterruptHandler)(void));
void TMR1_DefaultInterruptHandler(void);
void TMR1_ISR(void);
void TMR1_WriteTimer(uint16_t timerVal);
void TMR1_CallBack(void);
void TMR1_Reload(void);

void TMR1_StartTimer(void);
void TMR1_StopTimer(void);
void seedRand();

volatile int TIME_INTRO_ELAPSED;
volatile int INTRO_PLAYED;
volatile int INTRO_FINISHED;
volatile int GAME_STOPPED;
volatile int HORN_END_STOPPED;

int analog_reading;
volatile int TMR1_INTERRUPT_TICKER_FACTOR;

void delay100ms(int n);
void resetBuffer();
void sendOnly(char* buffer, int size);
void turnMotor(unsigned char direction);

volatile unsigned char BTN_PRESSED;

int DEBUG = 0;


void __interrupt() general_int(void) {
          
    if (IOCAFbits.IOCAF0) {        
        if (IOCAPbits.IOCAP0) {
          IOCAF = 0;      
          __delay_ms(200); 
          BTN_PRESSED = 1;
          IOCAF = 0;                      
        }
    }
    
    if(PIE1bits.TMR1IE == 1 && PIR1bits.TMR1IF == 1) {
            TMR1_ISR();
    }     
}

void sendOnly(char* buffer, int size) {
  if (DEBUG == 0) {return;}    
  for (int i=0; i < size; i++) {
    while (!TXIF);
    TXREG = buffer[i];     
  }     
}

void initialize() {
    
    OSCCONbits.IRCF = 0b1111;
    OSCCONbits.SCS = 0b10;
    TRISCbits.TRISC5 = 0; // LED OUTPUT
    ANSELCbits.ANSC6 = 0;  // disable analog on the 2 buttons    
    ANSELCbits.ANSC7 = 0;
    
    TRISCbits.TRISC6 = 1; // Set BUTTON1 and BUTTON2 as inputs
    TRISCbits.TRISC7 = 1;
    
    ANSELCbits.ANSC2 = 0;  // disable analog on C2
    ANSELBbits.ANSB4 = 0;  // B4    
    MP3PAUSETRIS = 0;  // out put Pin13,pin14 (rb4,rc2)
    MP3PAUSE = 0;
    TRISBbits.TRISB4 = 0;
    
    if (DEBUG == 1) {
    
    RCSTAbits.SPEN = 1; // enable serial port    
    ANSELBbits.ANSB5 = 0; // disable analog on b5 (RX)
    TRISBbits.TRISB5 = 1;  
    TRISBbits.TRISB7 = 0;    
    
    TXSTAbits.TX9 = 0; // 8 bit transmission (9 if set)
    TXSTAbits.SYNC = 0; // Async mode selected
    TXSTAbits.TXEN = 1; // enable transmit
    RCSTAbits.CREN = 1; // enable continous reception
       
    BRGH = 0;
    BRG16 = 0;
    SPBRG = 25;   // 9200 at 16mhz
    PIE1bits.RCIE = 1; // USART receive intrpt enable
    
    }
    
    INTCONbits.PEIE = 1;    
    INTCONbits.GIE = 1; 

    // fixed voltage
    FVRCONbits.FVREN = 0;
    //FVRCONbits.CDAFVR = 0b10;
    //FVRCONbits.ADFVR = 0b10;
    
    // bir red led config also its switch
    REDLEDTRIS = 0; // output big red led
    REDLED = 0;    
    REDBUTTON = 0; // this pin ra0 is always input only, hence no tris
    BTN_PRESSED = 0;
        
    //shtf
    POT1TRIS = 1; // input
    ANSELAbits.ANSA4 = 1; // select analog
    ADCON1bits.ADFM = 1; // right justify
    ADCON1bits.ADCS = 0b111;  // frc clock source
    ADCON1bits.ADPREF = 0b00; // vref+ connected to vdd
    ADCON0bits.CHS = 0b00011;  // saelect an3
    
    MP3PWRUP = 0;
    MP3PWRUPTRIS = 0; // output
    
    
}

void delay100ms(int n) {    
    for (int i =0; i < n; i++) {
        __delay_ms(50);       
    } 
    
}



void TMR1_Initialize(void)
{
    //Set the Timer to the options selected in the GUI

    //T1GSS T1G_pin; TMR1GE disabled; T1GTM disabled; T1GPOL low; T1GGO_nDONE done; T1GSPM disabled; 
    T1GCON = 0x00;

    //TMR1H 245; 
    TMR1H = 0x00;

    //TMR1L 212; 
    TMR1L = 0x00;

    // Clearing IF flag before enabling the interrupt.
    PIR1bits.TMR1IF = 0;

    // Load the TMR value to reload variable
    timer1ReloadVal=(uint16_t)((TMR1H << 8) | TMR1L);

    // Enabling TMR1 interrupt.
    PIE1bits.TMR1IE = 1;

    // Set Default Interrupt Handler
    TMR1_SetInterruptHandler(TMR1_DefaultInterruptHandler);

    // T1CKPS 1:8; T1OSCEN disabled; nT1SYNC synchronize; TMR1CS FOSC/4; TMR1ON enabled; 0x31
    // 0b0011 0101
    T1CON = 0b00110101;
}

void TMR1_SetInterruptHandler(void (* InterruptHandler)(void)){
    TMR1_InterruptHandler = InterruptHandler;
}

void TMR1_DefaultInterruptHandler(void){
    // add your TMR1 interrupt custom code
    // or set custom function using TMR1_SetInterruptHandler()
    TIME_INTRO_ELAPSED = 1;
    
    
}
static volatile unsigned int CountCallBack = 0;
void TMR1_ISR(void)
{
    // Clear the TMR1 interrupt flag
    PIR1bits.TMR1IF = 0;
    TMR1_WriteTimer(timer1ReloadVal);

    // callback function - called every 4th pass
    if (++CountCallBack >= TMR1_INTERRUPT_TICKER_FACTOR)
    {
        // ticker function call
        TMR1_CallBack();
        // reset ticker counter
        CountCallBack = 0;
    }
}

void TMR1_WriteTimer(uint16_t timerVal)
{
        // Stop the Timer by writing to TMRxON bit
        T1CONbits.TMR1ON = 0;
        // Write to the Timer1 register
        TMR1H = (timerVal >> 8);
        TMR1L = timerVal;
        // Start the Timer after writing to the register
        T1CONbits.TMR1ON =1;
}

void TMR1_CallBack(void)
{
    if(TMR1_InterruptHandler)
    {
        TMR1_InterruptHandler();
    }
}

void TMR1_StartTimer(void)
{
    T1CONbits.TMR1ON = 1;
}

void TMR1_StopTimer(void)
{
    T1CONbits.TMR1ON = 0;
}

void TMR1_Reload(void)
{
    TMR1_WriteTimer(timer1ReloadVal);
    CountCallBack = 0;
}


void seedRand() {
   time_t toc;          
   time(&toc);    
   srand((int)toc);    
}

void main(int argc, char** argv) {
    initialize();
    TMR1_Initialize();
    TMR1_StopTimer();
    
    TIME_INTRO_ELAPSED = 0;
    INTRO_PLAYED = 0;
    INTRO_FINISHED = 0;
    GAME_STOPPED = 0;
    HORN_END_STOPPED = 0;
    
    delay100ms(20);
    REDLED = 0;
                 
    seedRand();
    
    sendOnly("Zylopfa Foozball Timer\r\n", 24);    
    
    ADCON0bits.ADON = 1;
    INTCONbits.IOCIE = 1; // enable interrupt on change REDBUTTON
    IOCAPbits.IOCAP0 = 1; // Interrupt pin on rising edge REDBUTTON
        
    TMR1_INTERRUPT_TICKER_FACTOR = TMR1_INTRO_TICKER_FACTOR; // factor for instructions
        
    while (42) {
        
        ADCON0bits.GO_nDONE=1;
        ADCON0bits.GO = 1;
        while(ADCON0bits.GO_nDONE==1);
        int new_analog_reading= ADRESL + (ADRESH *256);        
        if ( (new_analog_reading > analog_reading+1) || (new_analog_reading < analog_reading-1)) {      
            unsigned char readbuffer[20] = {0};
            sprintf(readbuffer,"%d\r\n",new_analog_reading);
            sendOnly(readbuffer,strlen(readbuffer));
            analog_reading = new_analog_reading;
        }
        
        if (BTN_PRESSED) {
            unsigned char readbuffer[20] = {0};
            REDLEDLAT ^= 1;
            
            if (REDLED == 1) {
                MP3PWRUP = 1;
                INTRO_PLAYED = 0;
                INTRO_FINISHED = 0;
                TIME_INTRO_ELAPSED = 0;
                GAME_STOPPED = 0;
                HORN_END_STOPPED = 0;
                
                // wait some seconds and then pause   
                TMR1_INTERRUPT_TICKER_FACTOR = TMR1_INTRO_TICKER_FACTOR;
                TMR1_Reload();                
                                                
                unsigned char readbuffer[22] = {0};
                sprintf(readbuffer,"MP3 ON rst. timer!\r\n");                        
                sendOnly(readbuffer,strlen(readbuffer));
            } else {
                TMR1_StopTimer();
                INTRO_PLAYED = 0; 
                INTRO_FINISHED = 0;
                TIME_INTRO_ELAPSED = 0;  
                GAME_STOPPED = 0;
                MP3PWRUP = 0;
                unsigned char readbuffer[20] = {0};
                sprintf(readbuffer,"MP3 OFF!\r\n");                        
                sendOnly(readbuffer,strlen(readbuffer));                
            }
            
            sprintf(readbuffer,"BUTTON PRESSED!\r\n");                        
            sendOnly(readbuffer,strlen(readbuffer));
            BTN_PRESSED = 0;
        }
        
        if (TIME_INTRO_ELAPSED == 1 && INTRO_PLAYED == 0 ) {
          TMR1_StopTimer();
          MP3PAUSE = 1;
          delay100ms(6);
          MP3PAUSE = 0;          
          INTRO_PLAYED = 1;
          TIME_INTRO_ELAPSED = 0;
          unsigned char readbuffer[20] = {0};            
          sprintf(readbuffer,"INTRO FIN!\r\n");                        
          sendOnly(readbuffer,strlen(readbuffer));            
        }
        
        if (INTRO_PLAYED == 1 && TIME_INTRO_ELAPSED == 0 && GAME_STOPPED == 0) { 
            // set random timeout for the game to stop
            seedRand(); // TBI
            // 27 = 4 sec;
            // 27*30 // 2 min ungefair
            int randNr = rand() % ((27*30) + ((analog_reading+1) * 4));            
            int seconds = (randNr / 27) * 4;
            
            unsigned char readbuffer[20] = {0};            
            sprintf(readbuffer,"SECS: %d, %d !\r\n",seconds,randNr);                        
            sendOnly(readbuffer,strlen(readbuffer));  
            
            TMR1_INTERRUPT_TICKER_FACTOR = randNr; 
            TIME_INTRO_ELAPSED = 0;
            TMR1_Reload();           
            INTRO_FINISHED = 0;
            INTRO_PLAYED = 1;
            GAME_STOPPED = 1;                         
        }
                
        if (GAME_STOPPED == 1 && TIME_INTRO_ELAPSED == 1 && HORN_END_STOPPED == 0 ) {                   
          MP3PAUSE = 1;
          delay100ms(6);
          MP3PAUSE = 0;  
                    
          unsigned char readbuffer[20] = {0};            
          sprintf(readbuffer,"HORN INI!\r\n");                        
          sendOnly(readbuffer,strlen(readbuffer));            
                    
          TMR1_INTERRUPT_TICKER_FACTOR = TMR1_HORN_TICKER_FACTOR; 
          TIME_INTRO_ELAPSED = 0;
          TMR1_Reload();           
          INTRO_FINISHED = 1;
          INTRO_PLAYED = 1;
          HORN_END_STOPPED = 1;
        }        
        if (HORN_END_STOPPED == 1 && TIME_INTRO_ELAPSED == 1) {
            
          TMR1_StopTimer();
          TIME_INTRO_ELAPSED = 0;
          TMR1_INTERRUPT_TICKER_FACTOR = TMR1_INTRO_TICKER_FACTOR;
          unsigned char readbuffer[20] = {0};            
          sprintf(readbuffer,"HORN FIN!\r\n");                        
          sendOnly(readbuffer,strlen(readbuffer));   
          
          MP3PAUSE = 1;
          delay100ms(6);
          MP3PAUSE = 0;  
          
          // reset all
          MP3PWRUP = 0;   
          INTRO_PLAYED = 0;
          INTRO_FINISHED = 0;
          TIME_INTRO_ELAPSED = 0;
          GAME_STOPPED = 0;
          HORN_END_STOPPED = 0;
          REDLED = 0;            
        }
            
    }
    
}

