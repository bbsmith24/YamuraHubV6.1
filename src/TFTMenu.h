#include <cstring>
#include <cstdint>
#include <TFT_eSPI.h>     // https://github.com/Bodmer/TFT_eSPI Graphics and font library for ST7735 driver chip
#include "Free_Fonts.h"  // Include the header file attached to this sketch

#define BUTTON_COUNT 3
#define BOUNCE_DELAY 50
#define BUTTON_DEBOUNCE_DELAY 20  // ms
#define BUTTON_RELEASED 0
#define BUTTON_PRESSED 1

class TFTMenu 
{
public:    
    struct UserButton {
        int buttonPin = 0;
        bool buttonReleased = false;
        bool buttonPressed = false;
        byte buttonLast = BUTTON_RELEASED;
        String buttonName = "button";
        unsigned long pressDuration = 0;
        unsigned long releaseDuration = 0;
        unsigned long lastChange = 0;
        int buttonState = 0;
        int lastButtonState = 0;
        int lastDebounceTime = 0;
        bool buttonChanged = false;
    };
    struct MenuChoice {
        String description;
        int result;
    };
    static const int buttonCount = BUTTON_COUNT;

    // button structure list
    UserButton buttons[BUTTON_COUNT];
    // TFT display pointer (set by caller)
    TFT_eSPI* tftDisplay = nullptr;
    // initial state of caller, used to set initial menu selection
    int deviceState = 0;
    // font dimensions and position used for menu display
    int screenRotation = 1;
    int screenSize[4];
    int8_t fontHeight = 12;
    int fontWidth;
    int textPosition[2] = { 5, 12 };

    // current time used for button debounce and press duration tracking
    unsigned long currentMillis = 0;
    // banner string displayed at bottom of menu
    String bannerString = "Menu selection";


    // menuing function defs
    int MenuSelect(int fontSize, MenuChoice choices[], int menuCount, int initialSelect);
    // debounce function defs
    void WaitForAnyButton();
    void CheckButtons(unsigned long curTime);
    void CheckButton(unsigned long curTime, int buttonNum);
    void NotImplementedScreen(String messageStr);
    void DisplayBanner();
    void SetFont(int fontSize);
    void SetScreenRotation(int rotVal);
    void TFTInitialization();
    void DrawString(String messageStr, int xPos, int yPos, int font);
};