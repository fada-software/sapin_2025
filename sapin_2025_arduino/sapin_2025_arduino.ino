//==============================================================================
//Projet : sapin de noel 2025 avec 77 LED WS2812B
//Date: 28/11/2025
//Author: fada-software
//IDE : Arduino V2.3.6
//Board : package ESP32 3.3.0 by Espressif, choose "ESP32C3 Dev Module"
//USB CDC On Boot : Enabled
//JTAG Adapter : Integrated USB JTAG
//Partition scheme : Default 4MB with spiffs
//Library to install in IDE : Adafruit Neopixel 1.15.1
//couleur hue : https://learn.adafruit.com/adafruit-neopixel-uberguide/arduino-library-use
//==============================================================================
#include <Adafruit_NeoPixel.h>

// #define PROTO_2025                                               //!< Uncomment this line for PROTO 2025 configuration values

#define NUM_LEDS                        77                          //!< Total number of leds
#define NB_LED_BASE                     21                          //!< Number of leds of base ring
#define NB_LED_ETAGE_1                  18                          //!< Number of leds of first ring
#define NB_LED_ETAGE_2                  15                          //!< Number of leds of second ring
#define NB_LED_ETAGE_3                  12                          //!< Number of leds of third ring
#define NB_LED_ETAGE_4                  9                           //!< Number of leds of fourth ring
#define NB_LED_ETOILE                   2                           //!< Number of leds of star

#define PIN_LED_BUILTIN                 8                           //!< Pin ESP32C3 super mini sapin 2025
#define PIN_LDR                         A1                          //!< Pin ESP32C3 super mini sapin 2025
#define PIN_MODE_BUTTON                 0                           //!< Pin ESP32C3 super mini sapin 2025
#define PIN_LUM_BUTTON                  21                          //!< Pin ESP32C3 super mini sapin 2025
#define PIN_LED_NUM                     7                           //!< Pin ESP32C3 super mini sapin 2025

#ifdef PROTO_2025
// Values for PROTO 2025
#define LUM_BRIGHTNESS_MIN_VAL          ((unsigned char) 10)        //!< Brightness minimum value
#define LUM_BRIGHTNESS_MAX_VAL          ((unsigned char) 60)        //!< Brightness minimum value
#define LUM_BRIGHTNESS_LOW_VALUE        LUM_BRIGHTNESS_MIN_VAL      //!< Brightness low value
#define LUM_BRIGHTNESS_MEDIUM_VALUE     ((unsigned char) 25)        //!< Brightness medium value
#define LUM_BRIGHTNESS_HIGH_VALUE       LUM_BRIGHTNESS_MAX_VAL      //!< Brightness high value
#define LUM_AUTO_MODE_ADC_THRESHOLD     ((unsigned int) 2500)       //!< ADC threshold //1500 for nex, 2500 before
#else
// Values for RELEASE 2025
#define LUM_BRIGHTNESS_MIN_VAL          ((unsigned char) 14)        //!< Brightness minimum value
#define LUM_BRIGHTNESS_MAX_VAL          ((unsigned char) 60)        //!< Brightness minimum value
#define LUM_BRIGHTNESS_LOW_VALUE        LUM_BRIGHTNESS_MIN_VAL      //!< Brightness low value
#define LUM_BRIGHTNESS_MEDIUM_VALUE     ((unsigned char) 30)        //!< Brightness medium value
#define LUM_BRIGHTNESS_HIGH_VALUE       LUM_BRIGHTNESS_MAX_VAL      //!< Brightness high value
#define LUM_AUTO_MODE_ADC_THRESHOLD     ((unsigned int) 1500)       //!< ADC threshold //1500 for nex, 2500 before
#endif

#define PROGRAM_DURATION_SEC            8                           //!< Program time in sec
#define PROGRAM_HOLD_FOR_AUTO_MSEC      1000                        //!< Mode button hold time (in ms) to switch to auto on a program
#define LUM_HOLD_FOR_AUTO_MSEC          1000                        //!< Lum button hold time (in ms) to switch to auto
#define PROGRAM_NB                      12                          //!< Number of programs

// Parameter 1 = number of pixels in strip
// Parameter 2 = Arduino pin number (most are valid)
// Parameter 3 = pixel type flags, add together as needed:
//   NEO_KHZ800  800 KHz bitstream (most NeoPixel products w/WS2812 LEDs)
//   NEO_KHZ400  400 KHz (classic 'v1' (not v2) FLORA pixels, WS2811 drivers)
//   NEO_GRB     Pixels are wired for GRB bitstream (most NeoPixel products)
//   NEO_RGB     Pixels are wired for RGB bitstream (v1 FLORA pixels, not v2)
//   NEO_RGBW    Pixels are wired for RGBW bitstream (NeoPixel RGBW products)
Adafruit_NeoPixel leds = Adafruit_NeoPixel(NUM_LEDS, PIN_LED_NUM, NEO_GRB + NEO_KHZ800);

//! \brief LED Brightness status
typedef enum LED_BrightnessStatus
{
    E_LED_BRIGHTNESS_HIGH = 0,
    E_LED_BRIGHTNESS_MEDIUM,
    E_LED_BRIGHTNESS_LOW,
    E_LED_BRIGHTNESS_AUTO,
    E_LED_BRIGHTNESS_NB
} t_LED_BrightnessStatus;

int i = 0;
unsigned char hue = 0;
unsigned char begin_hue = 0;
unsigned int adc_value = 0;

// Fade to black variables
int getcolors = 0;
int FadeR = 0;
int FadeG = 0;
int FadeB = 0;

hw_timer_t *timer = NULL;

//! \brief Regroup variables for task functions
typedef struct SAPIN_MainContext
{
    TaskHandle_t programTask;                           //!< Task handle
    bool programLocked;                                 //!< Program lock status
    unsigned char program;                              //!< Program id
    t_LED_BrightnessStatus ledBrightnessStatus;         //!< Brightness status
    unsigned char brightness_value;                     //!< Brightness value
    int sec_counter;                                    //!< Secondes counter
} t_SAPIN_MainContext;

int modeButtonStartPressed = 0;
int lumButtonStartPressed = 0;
unsigned char programTemp;                              //!< Program id (temp) to detect a program change during task execution

static t_SAPIN_MainContext SAPIN_MainContext;


void setup()
{
    // Set PIN
    delay(1000);
    pinMode(PIN_LED_BUILTIN, OUTPUT);
    digitalWrite(PIN_LED_BUILTIN, LOW);   // turn the LED off by making the voltage LOW
    pinMode(PIN_MODE_BUTTON, INPUT);
    pinMode(PIN_LUM_BUTTON, INPUT);

    // Set context
    memset(&SAPIN_MainContext, 0, sizeof(t_SAPIN_MainContext));
    SAPIN_MainContext.brightness_value = LUM_BRIGHTNESS_MIN_VAL;
    SAPIN_MainContext.program = 0;
    SAPIN_MainContext.ledBrightnessStatus = E_LED_BRIGHTNESS_AUTO;
    SAPIN_MainContext.sec_counter = 0;

    // Init leds driver
    leds.begin();
    leds.setBrightness(SAPIN_MainContext.brightness_value);
    leds.show(); // Initialize all pixels to 'off'

    // Init serial
    Serial.begin(115200);

    // Init interrupt timer for secondes counter
    timer = timerBegin(1000000); //timer frequency 1MHz
    timerAttachInterrupt(timer, &timer_isr);
    timerAlarm(timer, 1000000, true, 0); //timer event 1000000 / 1MHz = 1s
    
    // Create program task
    xTaskCreate(programTaskLoop, "ProgramTaskLoop", 1024, NULL, 2, &SAPIN_MainContext.programTask);

    // Force brightness configuration
    led_brightness_set();
}

//sous-programme d'interruption du timer 1Hz
//----------------------------------------------------------------------------
//! \brief   Timer isr
//!
//! - <b>Description</b> : Timer isr set to 1Hz to have a secondes counter and switch program
//!
//! - <b>Process</b> : <pre>
//!         Increment secondes counter
//!         If secondes counter >= program max time and program is not locked
//!             Switch to next program
//!             Reset secondes counter to 0
//! </pre>
//! - <b>Constraints when used by another function</b> : None.
//! - <b>Used variables</b> :
//! \param  None.
//! \return None.
//----------------------------------------------------------------------------
void ARDUINO_ISR_ATTR timer_isr()
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    digitalWrite(PIN_LED_BUILTIN, !digitalRead(PIN_LED_BUILTIN)); //le LED bleue de l'ESP32 change d'état toutes les secondes
    SAPIN_MainContext.sec_counter++;
    if ((SAPIN_MainContext.sec_counter >= PROGRAM_DURATION_SEC) && (SAPIN_MainContext.programLocked == false))
    {
        program_next();
        SAPIN_MainContext.sec_counter = 0;
    }
    //Serial.println(SAPIN_MainContext.sec_counter);
}


//----------------------------------------------------------------------------
//! \brief   Program task loop
//!
//! - <b>Description</b> : Program task loop calling the program execution function
//!
//! - <b>Process</b> : <pre>
//!         Call the program execution function indefinitely </pre>
//! - <b>Constraints when used by another function</b> : None.
//! - <b>Used variables</b> :
//! \param  parameter   Not used.
//! \return None.
//----------------------------------------------------------------------------
void programTaskLoop(void * parameter)
{
    (void) parameter;

    while (1)
    {
        program_execution();
    }
}


//----------------------------------------------------------------------------
//! \brief   Loop
//!
//! - <b>Description</b> : Loop function monitoring buttons
//!
//! - <b>Process</b> : <pre>
//!         If MODE button is pressed:
//!             Monitoring of the press time
//!             If pressing time is > 1s:
//!                 Enable rotation by setting program lock to false
//!             Else:
//!                 If program rotation is not locked:
//!                     Lock program rotation
//!                 Else:
//!                     Switch to next program
//!         If LUM button is pressed:
//!             Monitoring of the press time
//!             If pressing time is > 1s:
//!                 Update brightness status to AUTO
//!             Else:
//!                 Update brightness status to next status
//!         Set the brightness </pre>
//! - <b>Constraints when used by another function</b> : None.
//! - <b>Used variables</b> :
//! \param  None.
//! \return None.
//----------------------------------------------------------------------------
void loop(){ 

    if (digitalRead(PIN_MODE_BUTTON))
    {
        modeButtonStartPressed = millis();
        while(digitalRead(PIN_MODE_BUTTON))
        {

        };
        if ((millis() - modeButtonStartPressed) >= PROGRAM_HOLD_FOR_AUTO_MSEC)
        {
            SAPIN_MainContext.programLocked = false;
            Serial.print("Program rotation enabled");
        }
        else
        {
            if (SAPIN_MainContext.programLocked == false)
            {
                SAPIN_MainContext.programLocked = true;
                Serial.print("Program rotation disabled");
            }
            else
            {
                //vTaskDelete(SAPIN_MainContext.programTask);
                program_next();
                //xTaskCreate(programTaskLoop, "ProgramTaskLoop", 1024, NULL, 2, &SAPIN_MainContext.programTask);
            }
        }
    }

    if (digitalRead(PIN_LUM_BUTTON))
    {
        lumButtonStartPressed = millis();
        while(digitalRead(PIN_LUM_BUTTON))
        {

        };
        if ((millis() - lumButtonStartPressed) >= LUM_HOLD_FOR_AUTO_MSEC)
        {
            Serial.print("Set brightness to AUTO");
            led_brightness_update_status(true);
        }
        else
        {
            led_brightness_update_status(false);
        }
    }
}


//----------------------------------------------------------------------------
//! \brief   Program execution
//!
//! - <b>Description</b> : Program execution function
//!
//! - <b>Process</b> : <pre>
//!         If brightness status is set to AUTO
//!             Update brightness
//!         Execute program depending on program id </pre>
//! - <b>Constraints when used by another function</b> : None.
//! - <b>Used variables</b> :
//! \param  None.
//! \return None.
//----------------------------------------------------------------------------
void program_execution()
{
    programTemp = SAPIN_MainContext.program;

    if (SAPIN_MainContext.ledBrightnessStatus == E_LED_BRIGHTNESS_AUTO)
    {
        led_brightness_update_auto();
    }
    
    //########################  random ###################################
    if (SAPIN_MainContext.program == 0) {
        led_brightness_fade_to_black_by(10); // fade everything out
        leds.setPixelColor(random(NUM_LEDS), leds.ColorHSV(random(65535), 255, 255));
        leds.show();
        vTaskDelay(30);
    }

    //########################  points jaunes qui flash sur fond violet ###################################
    if (SAPIN_MainContext.program == 1) {
        for(int i = 0; i < NUM_LEDS; i++) {
            leds.setPixelColor(i, leds.ColorHSV(175*256, 255, 200)); //violet pas trop fort
        }
        for(int i = 0; i < 10; i++) {   //10 LED au hasard
            leds.setPixelColor(random(NUM_LEDS), leds.ColorHSV(42*256, 255, 255)); //jaune
        }
        leds.show();
        vTaskDelay(50);
    }
    
    // //########################  anneaux qui montent ###################################
    if (SAPIN_MainContext.program == 2) {
        for(int i = 0; i < NB_LED_BASE; i++) {   
            leds.setPixelColor(i, leds.ColorHSV(hue*256, 255, 255));
        }
        leds.show();
        hue+=16;
        led_brightness_fade_all();
        for(int i = 0; i < NB_LED_ETAGE_1; i++) {   
            leds.setPixelColor(NB_LED_BASE+i, leds.ColorHSV(hue*256, 255, 255));
        }
        leds.show();
        hue+=16;
        led_brightness_fade_all();
        for(int i = 0; i < NB_LED_ETAGE_2; i++) {   
            leds.setPixelColor(NB_LED_BASE+NB_LED_ETAGE_1+i, leds.ColorHSV(hue*256, 255, 255));
        }
        leds.show();
        hue+=16;
        led_brightness_fade_all();
        for(int i = 0; i < NB_LED_ETAGE_3; i++) {   
            leds.setPixelColor(NB_LED_BASE+NB_LED_ETAGE_1+NB_LED_ETAGE_2+i, leds.ColorHSV(hue*256, 255, 255));
        }
        leds.show();
        hue+=16;
        led_brightness_fade_all();
        for(int i = 0; i < NB_LED_ETAGE_4; i++) {   
            leds.setPixelColor(NB_LED_BASE+NB_LED_ETAGE_1+NB_LED_ETAGE_2+NB_LED_ETAGE_3+i, leds.ColorHSV(hue*256, 255, 255));
        }
        leds.show();
        hue+=16;
        led_brightness_fade_all();
        for(int i = 0; i < NB_LED_ETOILE; i++) {   
            leds.setPixelColor(NB_LED_BASE+NB_LED_ETAGE_1+NB_LED_ETAGE_2+NB_LED_ETAGE_3+NB_LED_ETAGE_4+i, leds.ColorHSV(hue*256, 255, 255));
        }
        leds.show();
        hue+=16;
        led_brightness_fade_all();
    }

    //########################  spirale qui descend ###################################
    if (SAPIN_MainContext.program == 3) {
        for(int i = NUM_LEDS-1; i >= 0; i--) {
            if (SAPIN_MainContext.program == 3)
            {
                led_brightness_fade_to_black_by(5); // fade everything out
                leds.setPixelColor(i, leds.ColorHSV(i*256, 255, 255)); //de 0 à 64 : rouge / orange / jaune uniquement
                leds.show();
                vTaskDelay(20);
            }   
        }
    }

    //########################  disque chromatique ###################################
    if (SAPIN_MainContext.program == 4) {
        hue=begin_hue++;
        for(int i = 0; i < NUM_LEDS; i++) {   
            leds.setPixelColor(i, leds.ColorHSV(hue*256, 255, 255));
            hue++;
        }
        leds.show();
        vTaskDelay(5);
    }

    //########################  points blancs qui flash sur fond rouge ###################################
    if (SAPIN_MainContext.program == 5) {
        for(int i = 0; i < NUM_LEDS; i++) {
            leds.setPixelColor(i, leds.Color(255, 0, 0)); //rouge
        }
        for(int i = 0; i < 10; i++) {   //10 LED au hasard
            leds.setPixelColor(random(NUM_LEDS), leds.Color(255, 255, 255)); //blanc
        }
        leds.show();
        vTaskDelay(50);
    }

    // //########################  points qui tournent ###################################
     if (SAPIN_MainContext.program == 6) {
        hue=begin_hue-4;
        for(int i = 0; i < NUM_LEDS-3; i+=3) {   
            leds.setPixelColor(i, leds.ColorHSV(hue*256, 255, 255));
            hue+=9;
            leds.setPixelColor(i+1, leds.ColorHSV(hue*256, 255, 0));
            leds.setPixelColor(i+2, leds.ColorHSV(hue*256, 255, 0));
        }
        leds.show();
        vTaskDelay(200);
        hue=begin_hue-4;
        for(int i = 1; i < NUM_LEDS-3; i+=3) {   
            leds.setPixelColor(i, leds.ColorHSV(hue*256, 255, 255));
            hue+=9;
            leds.setPixelColor(i+1, leds.ColorHSV(hue*256, 255, 0));
            leds.setPixelColor(i+2, leds.ColorHSV(hue*256, 255, 0));
        }
        leds.show();
        vTaskDelay(200);
        hue=begin_hue-4;
        for(int i = 2; i < NUM_LEDS-3; i+=3) {   
            leds.setPixelColor(i, leds.ColorHSV(hue*256, 255, 255));
            hue+=9;
            leds.setPixelColor(i+1, leds.ColorHSV(hue*256, 255, 0));
            leds.setPixelColor(i+2, leds.ColorHSV(hue*256, 255, 0));
        }
        leds.show();
        vTaskDelay(200);
    }

    // //########################  anneaux qui descendent ###################################
    if (SAPIN_MainContext.program == 7) {
        for(int i = 0; i < NB_LED_ETOILE; i++) {   
            leds.setPixelColor(NB_LED_BASE+NB_LED_ETAGE_1+NB_LED_ETAGE_2+NB_LED_ETAGE_3+NB_LED_ETAGE_4+i, leds.ColorHSV(random(65535), 255, 255));
        }
        leds.show();
        hue+=16;
        led_brightness_fade_all();
        for(int i = 0; i < NB_LED_ETAGE_4; i++) {   
            leds.setPixelColor(NB_LED_BASE+NB_LED_ETAGE_1+NB_LED_ETAGE_2+NB_LED_ETAGE_3+i, leds.ColorHSV(random(65535), 255, 255));
        }
        leds.show();
        hue+=16;
        if (SAPIN_MainContext.program == 7)
        {
            led_brightness_fade_all();
        }
        for(int i = 0; i < NB_LED_ETAGE_3; i++) {   
            leds.setPixelColor(NB_LED_BASE+NB_LED_ETAGE_1+NB_LED_ETAGE_2+i, leds.ColorHSV(random(65535), 255, 255));
        }
        leds.show();
        hue+=16;
        if (SAPIN_MainContext.program == 7)
        {
            led_brightness_fade_all();
        }
        for(int i = 0; i < NB_LED_ETAGE_2; i++) {   
            leds.setPixelColor(NB_LED_BASE+NB_LED_ETAGE_1+i, leds.ColorHSV(random(65535), 255, 255));
        }
        leds.show();
        hue+=16;
        if (SAPIN_MainContext.program == 7)
        {
            led_brightness_fade_all();
        }
        for(int i = 0; i < NB_LED_ETAGE_1; i++) {   
            leds.setPixelColor(NB_LED_BASE+i, leds.ColorHSV(random(65535), 255, 255));
        }
        leds.show();
        hue+=16;
        if (SAPIN_MainContext.program == 7)
        {
            led_brightness_fade_all();
        }
        for(int i = 0; i < NB_LED_BASE; i++) {   
            leds.setPixelColor(i, leds.ColorHSV(random(65535), 255, 255));
        }
        leds.show();
        hue+=16;
        led_brightness_fade_all();
    }

    //########################  spirale qui monte ###################################
    if (SAPIN_MainContext.program == 8) {
        for(int i = 0; i < NUM_LEDS; i++) {
            if (SAPIN_MainContext.program == 8)
            {
                led_brightness_fade_to_black_by(5); // fade everything out
                leds.setPixelColor(i, leds.ColorHSV(hue*256, 255, 255));
                leds.show();
                hue++;
                vTaskDelay(20);
            }   
        }
    }
    
    // //########################  arc en ciel ###################################
    if (SAPIN_MainContext.program == 9) {
        for(int i = 0; i < NUM_LEDS; i++) {   
            leds.setPixelColor(i, leds.ColorHSV(hue*256, 255, 255));
        }
        leds.show();
        vTaskDelay(30);
        hue+=5;
    }

    // //########################  points rouges qui flash sur fond vert ###################################
    if (SAPIN_MainContext.program == 10) {
        for(int i = 0; i < NUM_LEDS; i++) {
            leds.setPixelColor(i, leds.ColorHSV(105*256, 255, 150)); //turquoise pas trop fort
        }
        for(int i = 0; i < 10; i++) {   //10 LED au hasard
            leds.setPixelColor(random(NUM_LEDS), leds.Color(255, 0, 0)); //rouge
        }
        leds.show();
        vTaskDelay(50);
    }

    // //########################  battement cardiaque ###################################
    if (SAPIN_MainContext.program == 11) {
        for(int i = 0; i < 50; i++) {
            led_brightness_fade_to_black_heartbeat(5); // fade everything out
            leds.show();
            vTaskDelay(1);
        }
        if (SAPIN_MainContext.program == 11)
        {
            vTaskDelay(700);
            for(int i = 0; i < NUM_LEDS; i++) {
                leds.setPixelColor(i, leds.Color(255, 0, 0)); //rouge
            }
            leds.show();
            for(int i = 0; i < 50; i++) {
                led_brightness_fade_to_black_heartbeat(5); // fade everything out
                leds.show();
                vTaskDelay(1);
            }
            vTaskDelay(50);
            for(int i = 0; i < NUM_LEDS; i++) {
                leds.setPixelColor(i, leds.Color(255, 0, 0)); //rouge
            }
            leds.show();
        }
    }

    // //########################  neige ###################################
    if (SAPIN_MainContext.program == 12) {
        for(int i = 0; i < 10; i++) {   //10 LED au hasard
            leds.setPixelColor(random(NUM_LEDS), leds.Color(255, 255, 255)); //blanc
        }
        leds.show();
        vTaskDelay(200);
        led_brightness_fade_to_black_by(40); // fade everything out
    }

    // Turn off all leds if program has change during function execution
    if (programTemp != SAPIN_MainContext.program)
    {
        led_brightness_turn_off();
    }
}


//----------------------------------------------------------------------------
//! \brief   Program next
//!
//! - <b>Description</b> : Switch to the next program
//!
//! - <b>Process</b> : <pre>
//!         If program id reached the maximum
//!             reset program to 
//!         Else:
//!             increment program
//!         Reset secondes counter to  </pre>
//! - <b>Constraints when used by another function</b> : None.
//! - <b>Used variables</b> :
//! \param  None.
//! \return None.
//----------------------------------------------------------------------------
void program_next()
{
    if (SAPIN_MainContext.program >= PROGRAM_NB)
    {
        SAPIN_MainContext.program = 0;
    }
    else
    {
        SAPIN_MainContext.program++;
    }
    SAPIN_MainContext.sec_counter = 0;
    Serial.print("program = ");
    Serial.println(SAPIN_MainContext.program);
}


//----------------------------------------------------------------------------
//! \brief   Fade to black
//!
//! - <b>Description</b> : Set colors using Adafruit Neopixel lib
//!
//! - <b>Process</b> : <pre>
//!         Set colors </pre>
//! - <b>Constraints when used by another function</b> : None.
//! - <b>Used variables</b> :
//! \param  None.
//! \return None.
//----------------------------------------------------------------------------
void led_brightness_fade_to_black_by(unsigned char val)
{
    val = val * SAPIN_MainContext.brightness_value / 255;
    if (val == 0)
    {
        val = 1;
    }
    for(int i = 0; i < NUM_LEDS; i++)
    {
        getcolors = leds.getPixelColor(i);
        FadeR = (getcolors >> 16) & 0xFF;
        FadeG = (getcolors >> 8) & 0xFF;
        FadeB = getcolors & 0xFF;
        FadeR = FadeR-val;
        if(FadeR < 0)
        {
            FadeR = 0;
        }
        FadeG = FadeG-val;
        if(FadeG < 0)
        {
            FadeG = 0;
        }
        FadeB = FadeB-val;
        if(FadeB < 0)
        {
            FadeB = 0;
        }
        leds.setPixelColor(i, leds.Color(FadeR, FadeG, FadeB));
    }
}

//----------------------------------------------------------------------------
//! \brief   Fade to black
//!
//! - <b>Description</b> : Set colors using Adafruit Neopixel lib
//!
//! - <b>Process</b> : <pre>
//!         Set colors </pre>
//! - <b>Constraints when used by another function</b> : None.
//! - <b>Used variables</b> :
//! \param  None.
//! \return None.
//----------------------------------------------------------------------------
void led_brightness_fade_to_black_heartbeat(unsigned char val)
{
    val = val * SAPIN_MainContext.brightness_value / 255;
    if (val == 0)
    {
        val = 1;
    }
    for(int i = 0; i < NUM_LEDS; i++)
    {
        getcolors = leds.getPixelColor(i);
        FadeR = (getcolors >> 16) & 0xFF;
        FadeG = (getcolors >> 8) & 0xFF;
        FadeB = getcolors & 0xFF;
        FadeR = FadeR-val;
        if(FadeR < 35)
        {
            FadeR = 35;
        }
        FadeG = FadeG-val;
        if(FadeG < 0)
        {
            FadeG = 0;
        }
        FadeB = FadeB-val;
        if(FadeB < 0)
        {
            FadeB = 0;
        }
        leds.setPixelColor(i, leds.Color(FadeR, FadeG, FadeB));
    }
}

//----------------------------------------------------------------------------
//! \brief   Turn off
//!
//! - <b>Description</b> : Turn off all leds
//!
//! - <b>Process</b> : <pre>
//!         Turn off </pre>
//! - <b>Constraints when used by another function</b> : None.
//! - <b>Used variables</b> :
//! \param  None.
//! \return None.
//----------------------------------------------------------------------------
void led_brightness_turn_off()
{
    for(int i = 0; i < NUM_LEDS; i++)
    {
        leds.setPixelColor(i, leds.Color(0, 0, 0));
    }
    leds.show();
}


//----------------------------------------------------------------------------
//! \brief   Fade all
//!
//! - <b>Description</b> : Lower the brightness for programs with rigns
//!
//! - <b>Process</b> : <pre>
//!         Lower the brightness </pre>
//! - <b>Constraints when used by another function</b> : None.
//! - <b>Used variables</b> :
//! \param  None.
//! \return None.
//----------------------------------------------------------------------------
void led_brightness_fade_all()
{
    for(int i = 0; i < 4; i++)
    {
        led_brightness_fade_to_black_by(20); // fade everything out
        leds.show();
        vTaskDelay(30);
    }
}


//----------------------------------------------------------------------------
//! \brief   Led brightness update status
//!
//! - <b>Description</b> : Update the selected brightness status
//!
//! - <b>Process</b> : <pre>
//!         If set in auto mode:
//!             Stay in auto
//!         Else:
//!             Set brightness to next available status (High, Medium, Low) 
//!         Set the brightness </pre>
//! - <b>Constraints when used by another function</b> : None.
//! - <b>Used variables</b> :
//! \param  automatic   Parameters to tell that product is set in auto mode
//! \return None.
//----------------------------------------------------------------------------
void led_brightness_update_status(bool automatic)
{
    if (automatic == true)
    {
        SAPIN_MainContext.ledBrightnessStatus = E_LED_BRIGHTNESS_AUTO;
    }
    else
    {
        SAPIN_MainContext.ledBrightnessStatus = (t_LED_BrightnessStatus)(((int)SAPIN_MainContext.ledBrightnessStatus) + 1);
        if (SAPIN_MainContext.ledBrightnessStatus >= E_LED_BRIGHTNESS_AUTO)
        {
            SAPIN_MainContext.ledBrightnessStatus = E_LED_BRIGHTNESS_HIGH;
        }
    }
    led_brightness_set();
}


//----------------------------------------------------------------------------
//! \brief   Led brightness set
//!
//! - <b>Description</b> : Set the global brightness value depending on selected brightness status
//!
//! - <b>Process</b> : <pre>
//!         If HIGH:
//!             Set brightness to 60
//!         If MEDIUM:
//!             Set brightness to 25
//!         If LOW:
//!             Set brightness to 10
//!         Else
//!             Do nothing </pre>
//! - <b>Constraints when used by another function</b> : None.
//! - <b>Used variables</b> :
//! \param  None.
//! \return None.
//----------------------------------------------------------------------------
void led_brightness_set()
{
    switch(SAPIN_MainContext.ledBrightnessStatus)
    {
        case E_LED_BRIGHTNESS_HIGH:
            Serial.println("HIGH");
            SAPIN_MainContext.brightness_value = LUM_BRIGHTNESS_HIGH_VALUE;
            leds.setBrightness(SAPIN_MainContext.brightness_value);
            break;
        case E_LED_BRIGHTNESS_MEDIUM:
            Serial.println("MEDIUM");
            SAPIN_MainContext.brightness_value = LUM_BRIGHTNESS_MEDIUM_VALUE;
            leds.setBrightness(SAPIN_MainContext.brightness_value);
            break;
        case E_LED_BRIGHTNESS_LOW:
            Serial.println("LOW");
            SAPIN_MainContext.brightness_value = LUM_BRIGHTNESS_LOW_VALUE;
            leds.setBrightness(SAPIN_MainContext.brightness_value);
            break;
        case E_LED_BRIGHTNESS_AUTO:
            Serial.println("AUTO");
            break;
        default:
            break;
    }  
}

//----------------------------------------------------------------------------
//! \brief   Led brightness update auto
//!
//! - <b>Description</b> : Reconfigure the brightness depending on sensor returned value
//!
//! - <b>Process</b> : <pre>
//!         Read ADC value
//!         If adc value in < to adc threshold
//!             set brightness to min value
//!         Else
//!             Compute brightness value </pre>
//! - <b>Constraints when used by another function</b> : None.
//! - <b>Used variables</b> :
//! \param  None.
//! \return None.
//----------------------------------------------------------------------------
void led_brightness_update_auto()
{
    adc_value = analogRead(PIN_LDR); // 0 = obscurité, 4095 = lumière du jour. ~2500 posé sur le bureau
    if (adc_value < LUM_AUTO_MODE_ADC_THRESHOLD)
    {
        SAPIN_MainContext.brightness_value = LUM_BRIGHTNESS_MIN_VAL;
    }
    else
    { //variation de 10 à 60
      SAPIN_MainContext.brightness_value = adc_value * (LUM_BRIGHTNESS_MAX_VAL - LUM_BRIGHTNESS_MIN_VAL) / (4095.0 - LUM_AUTO_MODE_ADC_THRESHOLD) - (LUM_AUTO_MODE_ADC_THRESHOLD * (LUM_BRIGHTNESS_MAX_VAL - LUM_BRIGHTNESS_MIN_VAL) / (4095.0 - LUM_AUTO_MODE_ADC_THRESHOLD) - LUM_BRIGHTNESS_MIN_VAL); 
    }
    leds.setBrightness(SAPIN_MainContext.brightness_value);
    Serial.println(SAPIN_MainContext.brightness_value);
    
}
