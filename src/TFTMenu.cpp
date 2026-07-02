#include <Arduino.h>
#include "TFTMenu.h"

//
// return next state as selection from choices array
// initialSelect defines the item in choices selected at the start of user input
//
int TFTMenu::MenuSelect(int fontSize, MenuChoice choices[], int menuCount, int initialSelect) 
{
    #ifdef DEBUG_EXTRA_VERBOSE
    Serial.println("MenuSelect called");
    #endif
    char outStr[256];
    // location of text
    textPosition[0] = 5;
    textPosition[1] = 0;
    unsigned long currentMillis = millis();
    // find initial selection
    int selection = initialSelect;
    for (int selIdx = 0; selIdx < menuCount; selIdx++) 
    {
        if (choices[selIdx].result == initialSelect) 
        {
            selection = selIdx;
        }
    }
    // reset buttons
    for (int btnIdx = 0; btnIdx < BUTTON_COUNT; btnIdx++) 
    {
        buttons[btnIdx].buttonReleased = false;
    }
    // erase screen, draw banner
    #ifdef DEBUG_EXTRA_VERBOSE
    Serial.println("MenuSelect clear screen and draw banner");
    #endif
    tftDisplay->fillScreen(TFT_WHITE);
    DisplayBanner();
    tftDisplay->setFreeFont(FSS12);
    fontHeight = tftDisplay->fontHeight(GFXFF);
    textPosition[0] = 50;
    textPosition[1] = 20;
    #ifdef DEBUG_EXTRA_VERBOSE
    Serial.println("setTextColor(TFT_BLACK, TFT_WHITE)");
    #endif
    tftDisplay->setTextColor(TFT_BLACK, TFT_WHITE);

    // loop until selection is made
    SetFont(fontSize);
    int linesToDisplay = (tftDisplay->height() - 10) / fontHeight;
    // range of selections to display (allow scrolling)
    int displayRange[2] = { 0, linesToDisplay - 1 };
    displayRange[1] = (menuCount < linesToDisplay ? menuCount : linesToDisplay) - 1;
    // display menu
    #ifdef DEBUG_EXTRA_VERBOSE
    Serial.println("setTextColor(TFT_BLACK, TFT_WHITE)");
    #endif
    tftDisplay->setTextColor(TFT_BLACK, TFT_WHITE);
    while (true) 
    {
        textPosition[0] = 5;
        textPosition[1] = 0;
        for (int menuIdx = displayRange[0]; menuIdx <= displayRange[1]; menuIdx++) 
        {
            sprintf(outStr, "selection %d: %s", menuIdx, choices[menuIdx].description.c_str());
            #ifdef DEBUG_EXTRA_VERBOSE
            Serial.println(outStr);
            #endif
            tftDisplay->fillRect(textPosition[0], textPosition[1], tftDisplay->width(), fontHeight, TFT_WHITE);
            if (menuIdx == selection) 
            {
                tftDisplay->drawString(">", textPosition[0], textPosition[1], GFXFF);
                sprintf(outStr, ">>>%s", choices[menuIdx].description.c_str());
            }
            else
            {
                sprintf(outStr, "   %s", choices[menuIdx].description.c_str());
            }
            textPosition[0] = fontWidth;
            sprintf(outStr, "   %s", choices[menuIdx].description.c_str());
            tftDisplay->drawString(outStr, textPosition[0], textPosition[1], GFXFF);
            textPosition[0] = 0;
            textPosition[1] += fontHeight;
        }
        while (true) 
        {
            currentMillis = millis();
            CheckButtons(currentMillis);
            // selection made, set state and break
            if (buttons[0].buttonReleased) 
            {
                #ifdef DEBUG_EXTRA_VERBOSE
                Serial.print(">>>>>MenuSelect button 0 released, selection index ");
                Serial.print(selection);
                Serial.print(" made, returning result: ");
                Serial.println(choices[selection].result);
                #endif
                buttons[0].buttonReleased = false;
                return choices[selection].result;
            }
            // change selection down, break
            else if (buttons[1].buttonReleased) 
            {
                buttons[1].buttonReleased = false;
                selection = (selection + 1) < menuCount ? (selection + 1) : 0;
                #ifdef DEBUG_EXTRA_VERBOSE
                Serial.print("MenuSelect button 1 released, changing selection down to index ");
                Serial.println(selection);
                #endif

                // handle loop back to start
                if (selection < displayRange[0]) 
                {
                    displayRange[0] = selection;
                    displayRange[1] = displayRange[0] + linesToDisplay - 1;
                }
                // show next line at bottom
                else if (selection > displayRange[1]) 
                {
                    displayRange[1] = selection;
                    displayRange[0] = displayRange[1] - linesToDisplay + 1;
                }
                break;
            }
            // change selection up, break
            else if (buttons[2].buttonReleased) 
            {
                buttons[2].buttonReleased = false;
                selection = (selection - 1) >= 0 ? (selection - 1) : menuCount - 1;
                #ifdef DEBUG_EXTRA_VERBOSE
                Serial.print("MenuSelect button 2 released, changing selection up to index ");
                Serial.println(selection);
                #endif
                // handle loop back to start
                if (selection < displayRange[0]) 
                {
                    displayRange[0] = selection;
                    displayRange[1] = displayRange[0] + linesToDisplay - 1;
                }
                // show next line at bottom
                else if (selection > displayRange[1]) 
                {
                    displayRange[1] = selection;
                    displayRange[0] = displayRange[1] - linesToDisplay + 1;
                }
                break;
            }
            // else if (buttons[3].buttonReleased) 
            // {
            //     Serial.println("MenuSelect button 3 released, selection made");
            //     buttons[3].buttonReleased = false;
            // }
            delay(100);
        }
    }
    #ifdef DEBUG_EXTRA_VERBOSE
    Serial.print("MenuSelect result: ");
    Serial.println(choices[selection].result);
    #endif
    return choices[selection].result;
}
//
// check all buttons for new press or release
//
void TFTMenu::CheckButtons(unsigned long curTime) 
{
    byte curState;
    for (byte btnIdx = 0; btnIdx < BUTTON_COUNT; btnIdx++) 
    {
        curState = digitalRead(buttons[btnIdx].buttonPin);
        if (((curTime - buttons[btnIdx].lastChange) > BUTTON_DEBOUNCE_DELAY) && (curState != buttons[btnIdx].buttonLast)) 
        {
            buttons[btnIdx].lastChange = curTime;
            if (curState == BUTTON_PRESSED) 
            {
                buttons[btnIdx].buttonPressed = 1;
                buttons[btnIdx].buttonReleased = 0;
                buttons[btnIdx].buttonLast = BUTTON_PRESSED;
                buttons[btnIdx].pressDuration = 0;
            }
            else if (curState == BUTTON_RELEASED) 
            {
                buttons[btnIdx].buttonPressed = 0;
                buttons[btnIdx].buttonReleased = 1;
                buttons[btnIdx].buttonLast = BUTTON_RELEASED;
                buttons[btnIdx].releaseDuration = 0;
            }
        } 
        else 
        {
            if (curState == BUTTON_PRESSED) 
            {
                buttons[btnIdx].pressDuration = curTime - buttons[btnIdx].lastChange;
            } 
            else if (curState == BUTTON_RELEASED) 
            {
                buttons[btnIdx].releaseDuration = curTime - buttons[btnIdx].lastChange;
            }
        }
    }
}
//
// check one buttons for new press or release
//
void TFTMenu::CheckButton(unsigned long curTime, int buttonNum) 
{
    byte curState;
    curState = digitalRead(buttons[buttonNum].buttonPin);
    if (((curTime - buttons[buttonNum].lastChange) > BUTTON_DEBOUNCE_DELAY) && (curState != buttons[buttonNum].buttonLast)) 
    {
        buttons[buttonNum].lastChange = curTime;
        if (curState == BUTTON_PRESSED) 
        {
            buttons[buttonNum].buttonPressed = 1;
            buttons[buttonNum].buttonReleased = 0;
            buttons[buttonNum].buttonLast = BUTTON_PRESSED;
            buttons[buttonNum].pressDuration = 0;
        }
        else if (curState == BUTTON_RELEASED) 
        {
            buttons[buttonNum].buttonPressed = 0;
            buttons[buttonNum].buttonReleased = 1;
            buttons[buttonNum].buttonLast = BUTTON_RELEASED;
            buttons[buttonNum].releaseDuration = 0;
        }
    } 
    else 
    {
        if (curState == BUTTON_PRESSED) 
        {
            buttons[buttonNum].pressDuration = curTime - buttons[buttonNum].lastChange;
        }
        else if (curState == BUTTON_RELEASED) 
        {
            buttons[buttonNum].releaseDuration = curTime - buttons[buttonNum].lastChange;
        }
    }
}
//
//
//
void TFTMenu::WaitForAnyButton()
{
    bool continuePress = false;
    while (true)
    {
        currentMillis = millis();
        CheckButtons(currentMillis);
        // selection made, set state and break
        for (int btnIdx = 0; btnIdx < BUTTON_COUNT; btnIdx++)
        {
            if (buttons[btnIdx].buttonReleased) 
            {
                continuePress = true;
                buttons[btnIdx].buttonReleased = false;
                break;
            }
        }
        if (continuePress)
        {
            break;
        }
    }
}
//
//
//
void TFTMenu::NotImplementedScreen(String messageStr)
{
    #ifdef DEBUG_EXTRA_VERBOSE
    Serial.println("NotImplementedScreen() - displaying not implemented screen");
    #endif
    tftDisplay->fillScreen(TFT_WHITE);
    DisplayBanner();
    tftDisplay->setFreeFont(FSS12);
    fontHeight = tftDisplay->fontHeight(GFXFF);
    textPosition[0] = 50;
    textPosition[1] = 20;
    #ifdef DEBUG_EXTRA_VERBOSE
    Serial.println("setTextColor(TFT_BLACK, TFT_WHITE)");
    #endif
    tftDisplay->setTextColor(TFT_BLACK, TFT_WHITE);

    tftDisplay->drawString("Menu handler for", textPosition[0], textPosition[1], GFXFF);
    textPosition[1] += fontHeight;
    tftDisplay->drawString(messageStr.c_str(), textPosition[0], textPosition[1], GFXFF);
    textPosition[1] += fontHeight;
    tftDisplay->drawString("Not implemented", textPosition[0], textPosition[1], GFXFF);
    textPosition[1] += fontHeight;
    textPosition[1] += fontHeight;
    tftDisplay->drawString("Press any button to continue", textPosition[0], textPosition[1], GFXFF);
    textPosition[1] += fontHeight;

    WaitForAnyButton();
}
//
//
//
void TFTMenu::DisplayBanner()
{
    #ifdef DEBUG_EXTRA_VERBOSE
    Serial.println("DisplayBanner() - Displaying banner on TFT display...");
    Serial.println("setTextColor(~TFT_BLACK, ~TFT_YELLOW)");
    #endif
    tftDisplay->setTextColor(~TFT_BLACK, ~TFT_YELLOW);
    SetFont(9);
    int xPos = tftDisplay->width() / 2;
    int yPos = tftDisplay->height() - fontHeight / 2;
    tftDisplay->setTextDatum(BC_DATUM);
    tftDisplay->drawString(bannerString, xPos, yPos, GFXFF);
    tftDisplay->setTextDatum(TL_DATUM);
    tftDisplay->setFreeFont(FSS12);
    fontHeight = tftDisplay->fontHeight(GFXFF);
    #ifdef DEBUG_EXTRA_VERBOSE
    Serial.println("setTextColor(TFT_BLACK, TFT_WHITE)");
    #endif
    tftDisplay->setTextColor(TFT_BLACK, TFT_WHITE);
}
//
//
//
void TFTMenu::SetFont(int fontSize)
{
  switch (fontSize) 
  {
    case 9:
      tftDisplay->setFreeFont(FSS9);
      break;
    case 12:
      tftDisplay->setFreeFont(FSS12);
      break;
    case 18:
      tftDisplay->setFreeFont(FSS18);
      break;
    case 24:
      tftDisplay->setFreeFont(FSS24);  // was FSS18
      break;
    default:
      tftDisplay->setFreeFont(FSS12);
      break;
  }
  fontHeight = tftDisplay->fontHeight(GFXFF);
  fontWidth = tftDisplay->textWidth("X");
}
//
// Set the TFT screen rotation and update global variables accordingly
//
void TFTMenu::SetScreenRotation(int rotVal) 
{
    char outStr[256];
    #ifdef DEBUG_EXTRA_VERBOSE
    Serial.println("SetScreenRotation() - Setting screen rotation...");
    #endif
    tftDisplay->setRotation(rotVal);
    screenRotation = rotVal;
    if ((rotVal == 0) || (rotVal == 2)) 
    {
        screenSize[0] = TFT_WIDTH;
        screenSize[1] = TFT_HEIGHT;
    } 
    else if ((rotVal == 1) || (rotVal == 3)) 
    {
        screenSize[0] = TFT_HEIGHT;
        screenSize[1] = TFT_WIDTH;
    }
    #ifdef DEBUG_EXTRA_VERBOSE
    sprintf(outStr, "SetScreen(%d)", rotVal);
    Serial.println(outStr);
    sprintf(outStr, "Screen size\tW %d\tH %d", screenSize[0], screenSize[1]);
    Serial.println(outStr);
    #endif
}
//
//
//
void TFTMenu::TFTInitialization()
{
    #ifdef DEBUG_EXTRA_VERBOSE
    Serial.println("\nInitialize TFT Display");
    Serial.println("init()");
    #endif
    tftDisplay->init();
    #ifdef DEBUG_EXTRA_VERBOSE
    Serial.println("invertDisplay(true)");
    #endif
    tftDisplay->invertDisplay(true);  // invert colors, not orientation
    #ifdef DEBUG_EXTRA_VERBOSE
    Serial.println("fillScreen(TFT_WHITE)");
    #endif
    tftDisplay->fillScreen(TFT_WHITE);
    SetScreenRotation(1);
    #ifdef DEBUG_EXTRA_VERBOSE
    Serial.println("Initialized TFT Display");
    #endif
    DisplayBanner();
    delay(2000);
}
//
//
//
void TFTMenu::DrawString(String messageStr, int xPos, int yPos, int font)
{
    #ifdef DEBUG_EXTRA_VERBOSE
    Serial.println("DrawString() - Drawing string on TFT display...");
    Serial.println("DrawString() - Drawing string on TFT display...");
    #endif
    SetFont(font);
    tftDisplay->drawString(messageStr, textPosition[0], textPosition[1], GFXFF);
    textPosition[1] += fontHeight;
}