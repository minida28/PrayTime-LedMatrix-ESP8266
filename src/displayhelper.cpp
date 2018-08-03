#include "displayhelper.h"
// #include "timehelper.h"
// #include "sholathelper.h"
// #include <ESP8266WiFi.h>
// #include "encoderhelper.h"
// #include "mqtt.h"

#define PRINTPORT Serial

#define PRINT(fmt, ...)                          \
    {                                            \
        static const char pfmt[] PROGMEM = fmt;  \
        PRINTPORT.printf_P(pfmt, ##__VA_ARGS__); \
    }

#define DEBUGPORT Serial

// #define RELEASE

#ifndef RELEASE
#define DEBUGLOG(fmt, ...)                       \
    {                                            \
        static const char pfmt[] PROGMEM = fmt;  \
        DEBUGPORT.printf_P(pfmt, ##__VA_ARGS__); \
    }
#define DEBUGLOGLN(fmt, ...)                     \
    {                                            \
        static const char pfmt[] PROGMEM = fmt;  \
        static const char rn[] PROGMEM = "\r\n"; \
        DEBUGPORT.printf_P(pfmt, ##__VA_ARGS__); \
        DEBUGPORT.printf_P(rn);                  \
    }
#else
#define DEBUGLOG(...)
#define DEBUGLOGLN(...)
#endif

HUB08 matrix(pinA, pinB, C, D, OE, R1, STB, CLK);

ledMatrixSettings _ledMatrixSettings;

uint16_t wText, hText;

char bufSECONDMATRIX[3];

unsigned int scrollSpeed;

byte currentPageMode0;
byte currentPageMode0_old = 255;
byte currentPageMode1;
byte currentPageMode1_old = 255;
byte currentPageMode2;
byte currentPageMode2_old = 255;
//byte OPERATING_MODE;
//byte OPERATING_MODE_old = 255;
byte MODE;
byte MODE_old = 255;
OperatingMode OPERATING_MODE_old;

bool enterEditModeFromShortcut = false;
bool blinkColon;

int X;

void refreshDisplay(int times)
{
    // analogWrite(2, 200);
    // static int state;
    // static unsigned long start;
    matrix.on();
    for (int i = 0; i < times * (matrix.height() + 0); i++)
    //for (int i = 0; i < times * 16; i++)
    {
        matrix.scan();
    }

    // workaround to give more time for Led on the last row to lit
    //delayMicroseconds(50);
    matrix.off();
}

void step3()
{
    static int staticX = 64;
    static int count;

    int times = 1;

    if (_ledMatrixSettings.scrollrow_0 == true || _ledMatrixSettings.scrollrow_1 == true)
    {

        if (_ledMatrixSettings.scrollspeed > 25)
        {
            //times = 1;
            count++;
            //int maxCount = 6; // scan rate 30
            //int maxCount = 16;
            int maxCount = _ledMatrixSettings.scrollspeed - 20;
            if (count < maxCount)
            {
                //process_runningled_page();
                refreshDisplay(times);
            }
            else if (count == maxCount)
            {
                staticX = staticX - 1;

                if (staticX < -wText)
                {
                    staticX = 64;
                }

                X = staticX;

                count = 0;
                //refreshDisplay(1);
                process_runningled_page();
                refreshDisplay(times);
            }
        }
        else
        {
            if (_ledMatrixSettings.scrollspeed > 20 && _ledMatrixSettings.scrollspeed <= 25)
            {
                times = 2;
            }
            count = 0;
            staticX = staticX - 1;

            if (staticX < -wText)
            {
                staticX = 64;
            }

            X = staticX;
            process_runningled_page();
            refreshDisplay(times);
        }
    }
    else
    {
        count = 0;
        X = 0;
        staticX = 64;
        process_runningled_page();
        refreshDisplay(times);
    }

    //refreshDisplay(3);
}

void AlarmTrigger()
{
    // DEBUGLOG("%s\r\n", __PRETTY_FUNCTION__);
    static boolean callAlarmSholatType1 = LOW;

    if ((HOUR == 0 && MINUTE == 10) && SECOND == 0)
    {
        callAlarmSholatType1 = HIGH;
    }
    if ((HOUR == 0 && MINUTE == 5) && SECOND == 0)
    {
        callAlarmSholatType1 = HIGH;
    }

    if (callAlarmSholatType1 == HIGH)
    {
        static byte count = 0;
        if (count < 3)
        {
            alarmState = HIGH;
            count++;
        }
        else if (count >= 3)
        {
            alarmState = LOW;
            callAlarmSholatType1 = LOW;
            count = 0;
        }
    }

    //sound the Alarm when current timestamp match
    //with one of sholattime's timestamp
    static boolean callAlarmSholatType2 = LOW;
    uint32_t t = now;
    for (int i = 0; i < TimesCount; i++)
    {
        if (i != Sunset)
        {
            if (t == sholat.timestampSholatTimesToday[i])
            {
                callAlarmSholatType2 = HIGH;
            }
        }
    }

    if (callAlarmSholatType2 == HIGH)
    {
        tone10 = HIGH;
        static byte count = 0;
        if (count < 4)
        {
            duration = 500;
            count++;
        }
        else if (count == 4)
        {
            duration = 2000;
            count++;
        }
        else if (count >= 5)
        {
            callAlarmSholatType2 = LOW;
            count = 0;
        }
    }
}

uint8_t PageAutomaticMode()
{
    // DEBUGLOG("%s\r\n", __PRETTY_FUNCTION__);
    uint32_t adzanEndTime = currentSholatTime + (60 * _ledMatrixSettings.adzanwaittime);
    uint32_t iqamahTime = adzanEndTime + (60 * _ledMatrixSettings.iqamahwaittime);
    if (_ledMatrixSettings.pagemode == Automatic)
    {
        if (atof(bufWatt) >= 2400)
        {
            currentPageMode0 = 4;
            return 0;
        }

        else if ((nextSholatTime - now) <= (60 * 50))
        {
            currentPageMode0 = 8;
            // PRINT("%s nextSholatTime= %u now= %u diff= %d\r\n", "Case A", nextSholatTime, now, nextSholatTime - now);
            return 1;
        }

        else if (now < adzanEndTime)
        {
            currentPageMode0 = 2;
            PRINT("now: %u, adzanEndTime: %u\r\n", now, adzanEndTime);
            return 2;
        }

        else if (now <= iqamahTime)
        {
            currentPageMode0 = 11;
            // Serial.println("now <= iqamahTime");
            return 3;
        }

        else if (now <= currentSholatTime + (60 * 30))
        {
            currentPageMode0 = 2;
            // PRINT("%s currentSholatTime= %u now= %u diff= %d\r\n", "Case B", currentSholatTime, now, now - currentSholatTime);
            return 4;
        }

        else if (atof(bufWatt) >= 2000)
        {
            currentPageMode0 = 4;
            return 5;
        }
        else
        {
            currentPageMode0 = _ledMatrixSettings.pagemode0;
            return 6;
        }
    }
    else
    {
        MODE = _ledMatrixSettings.operatingmode;
        currentPageMode0 = _ledMatrixSettings.pagemode0;
        currentPageMode1 = _ledMatrixSettings.pagemode1;
        currentPageMode2 = _ledMatrixSettings.pagemode2;
        return 7;
    }
}

void load_provinces_file(uint16_t index)
{

    // DEBUGLOG("%s\r\n", __PRETTY_FUNCTION__);

    File file = SPIFFS.open(FPSTR(pgm_provinces_file), "r");
    if (!file)
    {
        DEBUGLOG("Failed to open provinces list file");
        //return F("failed");
        //    return false;
        file.close();
        return;
    }

    // this is going to get the number of bytes in the file and give us the value in an integer
    int size = file.size();
    char buf[36];

    int16_t x1Temp, y1Temp;
    uint16_t wTemp, hTemp;

    wText = 0;

    uint16_t cityIndex = index;

    // iterate csv files
    for (int i = 0; i < cityIndex; i++)
    {
        while (file.available())
        {
            if (file.find('\n'))
            {
                break;
            }
        }
    }

    int offset = 0;
    while (file.available())
    {
        char rc = file.read();
        if (rc != '\r')
        {
            buf[offset] = rc;
            offset++;
        }
        else
        {
            buf[offset] = '\0'; // terminate the string
            break;
        }
    }

    // split the data into its parts
    char bufTemp[strlen(buf) + 1];
    strlcpy(bufTemp, buf, sizeof(bufTemp));

    char *token; // this is used by strtok() as an index

    int len;

    // store province ID
    token = strtok(bufTemp, ",");
    len = strlen(token) + 1;
    char provId[len];
    strlcpy(provId, token, sizeof(provId));

    // store province NAME
    token = strtok(NULL, ",");
    len = strlen(token) + 1;
    char provName[len];
    strlcpy(provName, token, sizeof(provName));

    // PRINT("province ID: %s, name: %s\r\n", provId, provName);

    // PRINT("startPos: %d, endPos: %d, wFirstChar: %d, offset: %d, string: %s\r\n", startPos, endPos, wFirstChar, offset, buf);

    matrix.print(provName);

    // char* chr = const_cast<char*>(contents.c_str());
    // matrix.getTextBounds(chr, 0, 0, &x1Temp, &y1Temp, &wTemp, &hTemp);
    matrix.getTextBounds(buf, 0, 0, &x1Temp, &y1Temp, &wTemp, &hTemp);
    // matrix.getTextBounds(contents, 0, 0, &x1Temp, &y1Temp, &wTemp, &hTemp);
    wText = wTemp;

    file.close();
}

void load_regencies_file(uint16_t selectedProvId)
{

    // DEBUGLOG("%s\r\n", __PRETTY_FUNCTION__);

    File file = SPIFFS.open(FPSTR(pgm_regencies_file), "r");
    if (!file)
    {
        DEBUGLOG("Failed to open provinces list file");
        //return F("failed");
        //    return false;
        file.close();
        return;
    }

    // this is going to get the number of bytes in the file and give us the value in an integer
    uint32_t size = file.size();

    char buf[36];

    int16_t x1Temp, y1Temp;
    uint16_t wTemp, hTemp;

    uint16_t wFirstChar;

    wText = 0;

    // uint16_t regenciesIndex = index;

    int num = 0;

    uint32_t offset = 0;
    uint32_t pos = file.position(); // start position
    uint32_t start = millis();

    file.seek(11479, SeekSet);

    while (num < 2)
    {
        char rc = file.read();
        if (rc == '\n')
        {
            num++;
        }

        while (num == 2)
        {
            char rc = file.read();
            if (rc != '\r')
            {
                buf[offset] = rc;
                offset++;
            }
            else
            {
                buf[offset] = '\0'; // terminate the string
                PRINT("\r\n%s\r\n\r\n", buf);
                break;
            }
        }
    }

    // split the data into its parts
    char bufTemp[strlen(buf) + 1];
    strlcpy(bufTemp, buf, sizeof(bufTemp));

    char *token; // this is used by strtok() as an index

    int len;

    // store regency ID
    token = strtok(bufTemp, ",");
    len = strlen(token) + 1;
    char regencyId[len];
    strlcpy(regencyId, token, sizeof(regencyId));

    // store province ID
    token = strtok(NULL, ",");
    len = strlen(token) + 1;
    char provId[len];
    strlcpy(provId, token, sizeof(provId));

    // store regency NAME
    token = strtok(NULL, ",");
    len = strlen(token) + 1;
    char regencyName[len];
    strlcpy(regencyName, token, sizeof(regencyName));

    // PRINT("province ID: %s, name: %s\r\n", provId, provName);

    // PRINT("startPos: %d, endPos: %d, wFirstChar: %d, offset: %d, string: %s\r\n", startPos, endPos, wFirstChar, offset, buf);

    matrix.print(regencyName);

    uint32_t end = millis();
    PRINT("\r\nnum: %d, Loading time: %d ms\r\n\r\n", num, end - start);

    /*
    // split the data into its parts
    char bufTemp[strlen(buf) + 1];
    strlcpy(bufTemp, buf, sizeof(bufTemp));

    char *token; // this is used by strtok() as an index

    int len;

    // store regency ID
    token = strtok(bufTemp, ",");
    len = strlen(token) + 1;
    char regencyId[len];
    strlcpy(regencyId, token, sizeof(regencyId));

    // store province ID
    token = strtok(NULL, ",");
    len = strlen(token) + 1;
    char provId[len];
    strlcpy(provId, token, sizeof(provId));

    // store regency NAME
    token = strtok(NULL, ",");
    len = strlen(token) + 1;
    char regencyName[len];
    strlcpy(regencyName, token, sizeof(regencyName));

    PRINT("regencyID ID:%s, provID:%s, name:%s\r\n", regencyId, provId, regencyName);

    matrix.print(regencyName);

    //char* chr = const_cast<char*>(contents.c_str());
    //matrix.getTextBounds(chr, 0, 0, &x1Temp, &y1Temp, &wTemp, &hTemp);
    matrix.getTextBounds(buf, 0, 0, &x1Temp, &y1Temp, &wTemp, &hTemp);
    //matrix.getTextBounds(contents, 0, 0, &x1Temp, &y1Temp, &wTemp, &hTemp);
    wText = wTemp;
    */

    file.close();
}

void DisplaySetup()
{
    matrix.begin();
    matrix.off();
    matrix.setTextWrap(0);
}

void DisplayLoop()
{
    scrollSpeed = _ledMatrixSettings.scrollspeed;
    if (scrollSpeed > 25)
    {
        scrollSpeed = 11;
    }

    unsigned long Timer = millis();
    static unsigned long lastTimer;

    if (millis() - lastTimer > scrollSpeed)
    {
        step3();
        lastTimer = Timer;
    }
}

void PreparePage(char *contents, const GFXfont *font, int16_t *x1, int16_t *y1)
{
    matrix.setFont(font);

    //char *str;
    //str = const_cast<char *>(contents.c_str());

    int16_t x1Temp, y1Temp;
    uint16_t w1Temp, h1Temp;

    //matrix.getTextBounds(str, 0, 0, &x1Temp, &y1Temp, &w1Temp, &h1Temp);
    matrix.getTextBounds(contents, 0, 0, &x1Temp, &y1Temp, &w1Temp, &h1Temp);
    *x1 = (matrix.width() - w1Temp) / 2;
    *y1 = (matrix.height() - h1Temp) / 2 + h1Temp - 1;
}

boolean slide_in_down_sec_digit_1(char chr, const GFXfont *font, int16_t x0, int16_t y0, uint16_t w, uint16_t h)
{
    //FONT
    //font = &FreeSerifBold9pt7b;
    //font = &UbuntuMono_B10pt8b;
    //font = &bold_led_board_7_regular4pt7b;
    //font = &RepetitionScrolling5pt8b;
    //font = &ChessType9pt7b;
    //font = &Org_01;
    //font = &FreeMonoBold12pt7b;
    //font = &FreeSansBold9pt7b;
    //font = &FreeSans9pt7b;

    matrix.setFont(font);

    int16_t x1 = x0;
    int16_t y1 = y0 - h + 1;

    //erase area and print next second
    matrix.fillRect(x0, y0 - h + 1, w, h, 0);
    matrix.setCursor(x0, y0);
    matrix.print(chr);

    //copy to buffer
    uint16_t buf[w * h];
    matrix.copyBuffer(x0, y0 - h + 1, w, h, buf);

    //erase area again :)
    matrix.fillRect(x0, y0 - h + 1, w, h, 0);

    static int16_t offset = 0;
    uint16_t index = 0;

    for (uint16_t yTemp = 0; yTemp < h; yTemp++)
    {
        for (uint16_t xTemp = 0; xTemp < w; xTemp++)
        {
            if (yTemp >= h - offset)
            {
                index = w * yTemp + xTemp;
                matrix.drawPixel(x1 + xTemp, y1 + yTemp - h + offset, buf[index]);
            }
            else
            {
                //matrix.drawPixel(x1 + xTemp, y1 + yTemp - h + offset, 0);
            }
        }
    }
    offset++;

    //reset offset and change to next animation
    if (offset > h)
    {
        offset = 0;
        return true;
    }
    else
    {
        return false;
    }
}

boolean slide_in_down_sec_digit_2(char chr, const GFXfont *font, int16_t x0, int16_t y0, uint16_t w, uint16_t h)
{
    //FONT
    //font = &FreeSerifBold9pt7b;
    //font = &UbuntuMono_B10pt8b;
    //font = &bold_led_board_7_regular4pt7b;
    //font = &RepetitionScrolling5pt8b;
    //font = &ChessType9pt7b;
    //font = &Org_01;
    //font = &FreeMonoBold12pt7b;
    //font = &FreeSansBold9pt7b;
    //font = &FreeSans9pt7b;

    matrix.setFont(font);

    int16_t x1 = x0;
    int16_t y1 = y0 - h + 1;

    //erase area and print next second
    matrix.fillRect(x0, y0 - h + 1, w, h, 0);
    matrix.setCursor(x0, y0);
    matrix.print(chr);

    //copy to buffer
    uint16_t buf[w * h];
    matrix.copyBuffer(x0, y0 - h + 1, w, h, buf);

    //erase area again :)
    matrix.fillRect(x0, y0 - h + 1, w, h, 0);

    static int16_t offset = 0;
    uint16_t index = 0;

    for (uint16_t yTemp = 0; yTemp < h; yTemp++)
    {
        for (uint16_t xTemp = 0; xTemp < w; xTemp++)
        {
            if (yTemp >= h - offset)
            {
                index = w * yTemp + xTemp;
                matrix.drawPixel(x1 + xTemp, y1 + yTemp - h + offset, buf[index]);
            }
            else
            {
                //matrix.drawPixel(x1 + xTemp, y1 + yTemp - h + offset, 0);
            }
        }
    }
    offset++;

    //reset offset and change to next animation
    if (offset > h)
    {
        offset = 0;
        return true;
    }
    else
    {
        return false;
    }
}

boolean slide_in_down_min_digit_1(char chr, const GFXfont *font, int16_t x0, int16_t y0, uint16_t w, uint16_t h)
{
    //FONT
    //font = &FreeSerifBold9pt7b;
    //font = &UbuntuMono_B10pt8b;
    //font = &bold_led_board_7_regular4pt7b;
    //font = &RepetitionScrolling5pt8b;
    //font = &ChessType9pt7b;
    //font = &Org_01;
    //font = &FreeMonoBold12pt7b;
    //font = &FreeSansBold9pt7b;
    //font = &FreeSans9pt7b;

    matrix.setFont(font);

    int16_t x1 = x0;
    int16_t y1 = y0 - h + 1;

    //erase area and print next second
    matrix.fillRect(x0, y0 - h + 1, w, h, 0);
    matrix.setCursor(x0, y0);
    matrix.print(chr);

    //copy to buffer
    uint16_t buf[w * h];
    matrix.copyBuffer(x0, y0 - h + 1, w, h, buf);

    //erase area again :)
    matrix.fillRect(x0, y0 - h + 1, w, h, 0);

    static int16_t offset = 0;
    uint16_t index = 0;

    for (uint16_t yTemp = 0; yTemp < h; yTemp++)
    {
        for (uint16_t xTemp = 0; xTemp < w; xTemp++)
        {
            if (yTemp >= h - offset)
            {
                index = w * yTemp + xTemp;
                matrix.drawPixel(x1 + xTemp, y1 + yTemp - h + offset, buf[index]);
            }
            else
            {
                //matrix.drawPixel(x1 + xTemp, y1 + yTemp - h + offset, 0);
            }
        }
    }
    offset++;

    //reset offset and change to next animation
    if (offset > h)
    {
        offset = 0;
        return true;
    }
    else
    {
        return false;
    }
}

boolean slide_in_down_min_digit_2(char chr, const GFXfont *font, int16_t x0, int16_t y0, uint16_t w, uint16_t h)
{
    //FONT
    //font = &FreeSerifBold9pt7b;
    //font = &UbuntuMono_B10pt8b;
    //font = &bold_led_board_7_regular4pt7b;
    //font = &RepetitionScrolling5pt8b;
    //font = &ChessType9pt7b;
    //font = &Org_01;
    //font = &FreeMonoBold12pt7b;
    //font = &FreeSansBold9pt7b;
    //font = &FreeSans9pt7b;

    matrix.setFont(font);

    int16_t x1 = x0;
    int16_t y1 = y0 - h + 1;

    //erase area and print next second
    matrix.fillRect(x0, y0 - h + 1, w, h, 0);
    matrix.setCursor(x0, y0);
    matrix.print(chr);

    //copy to buffer
    uint16_t buf[w * h];
    matrix.copyBuffer(x0, y0 - h + 1, w, h, buf);

    //erase area again :)
    matrix.fillRect(x0, y0 - h + 1, w, h, 0);

    static int16_t offset = 0;
    uint16_t index = 0;

    for (uint16_t yTemp = 0; yTemp < h; yTemp++)
    {
        for (uint16_t xTemp = 0; xTemp < w; xTemp++)
        {
            if (yTemp >= h - offset)
            {
                index = w * yTemp + xTemp;
                matrix.drawPixel(x1 + xTemp, y1 + yTemp - h + offset, buf[index]);
            }
            else
            {
                //matrix.drawPixel(x1 + xTemp, y1 + yTemp - h + offset, 0);
            }
        }
    }
    offset++;

    //reset offset and change to next animation
    if (offset > h)
    {
        offset = 0;
        return true;
    }
    else
    {
        return false;
    }
}

boolean slide_in_down_hour_digit_1(char chr, const GFXfont *font, int16_t x0, int16_t y0, uint16_t w, uint16_t h)
{
    //FONT
    //font = &FreeSerifBold9pt7b;
    //font = &UbuntuMono_B10pt8b;
    //font = &bold_led_board_7_regular4pt7b;
    //font = &RepetitionScrolling5pt8b;
    //font = &ChessType9pt7b;
    //font = &Org_01;
    //font = &FreeMonoBold12pt7b;
    //font = &FreeSansBold9pt7b;
    //font = &FreeSans9pt7b;

    matrix.setFont(font);

    int16_t x1 = x0;
    int16_t y1 = y0 - h + 1;

    //erase area and print next second
    matrix.fillRect(x0, y0 - h + 1, w, h, 0);
    matrix.setCursor(x0, y0);
    matrix.print(chr);

    //copy to buffer
    uint16_t buf[w * h];
    matrix.copyBuffer(x0, y0 - h + 1, w, h, buf);

    //erase area again :)
    matrix.fillRect(x0, y0 - h + 1, w, h, 0);

    static int16_t offset = 0;
    uint16_t index = 0;

    for (uint16_t yTemp = 0; yTemp < h; yTemp++)
    {
        for (uint16_t xTemp = 0; xTemp < w; xTemp++)
        {
            if (yTemp >= h - offset)
            {
                index = w * yTemp + xTemp;
                matrix.drawPixel(x1 + xTemp, y1 + yTemp - h + offset, buf[index]);
            }
            else
            {
                //matrix.drawPixel(x1 + xTemp, y1 + yTemp - h + offset, 0);
            }
        }
    }
    offset++;

    //reset offset and change to next animation
    if (offset > h)
    {
        offset = 0;
        return true;
    }
    else
    {
        return false;
    }
}

boolean slide_in_down_hour_digit_2(char chr, const GFXfont *font, int16_t x0, int16_t y0, uint16_t w, uint16_t h)
{
    //FONT
    //font = &FreeSerifBold9pt7b;
    //font = &UbuntuMono_B10pt8b;
    //font = &bold_led_board_7_regular4pt7b;
    //font = &RepetitionScrolling5pt8b;
    //font = &ChessType9pt7b;
    //font = &Org_01;
    //font = &FreeMonoBold12pt7b;
    //font = &FreeSansBold9pt7b;
    //font = &FreeSans9pt7b;

    matrix.setFont(font);

    int16_t x1 = x0;
    int16_t y1 = y0 - h + 1;

    //erase area and print next second
    matrix.fillRect(x0, y0 - h + 1, w, h, 0);
    matrix.setCursor(x0, y0);
    matrix.print(chr);

    //copy to buffer
    uint16_t buf[w * h];
    matrix.copyBuffer(x0, y0 - h + 1, w, h, buf);

    //erase area again :)
    matrix.fillRect(x0, y0 - h + 1, w, h, 0);

    static int16_t offset = 0;
    uint16_t index = 0;

    for (uint16_t yTemp = 0; yTemp < h; yTemp++)
    {
        for (uint16_t xTemp = 0; xTemp < w; xTemp++)
        {
            if (yTemp >= h - offset)
            {
                index = w * yTemp + xTemp;
                matrix.drawPixel(x1 + xTemp, y1 + yTemp - h + offset, buf[index]);
            }
            else
            {
                //matrix.drawPixel(x1 + xTemp, y1 + yTemp - h + offset, 0);
            }
        }
    }
    offset++;

    //reset offset and change to next animation
    if (offset > h)
    {
        offset = 0;
        return true;
    }
    else
    {
        return false;
    }
}

boolean slide_out_down_sec_digit_1(char chr, const GFXfont *font, int16_t x0, int16_t y0, uint16_t w, uint16_t h)
{
    //FONT
    //font = &FreeSerifBold9pt7b;
    //font = &UbuntuMono_B10pt8b;
    //font = &bold_led_board_7_regular4pt7b;
    //font = &RepetitionScrolling5pt8b;
    //font = &ChessType9pt7b;
    //font = &Org_01;
    //font = &FreeMonoBold12pt7b;
    //font = &FreeSansBold9pt7b;
    //font = &FreeSans9pt7b;

    matrix.setFont(font);

    int16_t x1 = x0;
    int16_t y1 = y0 - h + 1;

    //erase area and print next second
    matrix.fillRect(x0, y0 - h + 1, w, h, 0);
    matrix.setCursor(x0, y0);
    matrix.print(chr);

    //copy to buffer
    uint16_t buf[w * h];
    matrix.copyBuffer(x0, y0 - h + 1, w, h, buf);

    //erase area again :)
    matrix.fillRect(x0, y0 - h + 1, w, h, 0);

    static int16_t offset = 0;
    uint16_t index = 0;

    for (uint16_t yTemp = 0; yTemp < h; yTemp++)
    {
        for (uint16_t xTemp = 0; xTemp < w; xTemp++)
        {
            if (yTemp < h - offset)
            {
                index = w * yTemp + xTemp;
                matrix.drawPixel(x1 + xTemp, y1 + yTemp + offset, buf[index]);
            }
            else
            {
                // matrix.drawPixel(x1 + xTemp, y1 + yTemp - h + offset, 0);
            }
        }
    }
    offset++;

    //reset offset and change to next animation
    if (offset > h)
    {
        offset = 0;
        return true;
    }
    else
    {
        return false;
    }
}

boolean slide_out_down_sec_digit_2(char chr, const GFXfont *font, int16_t x0, int16_t y0, uint16_t w, uint16_t h)
{
    //FONT
    //font = &FreeSerifBold9pt7b;
    //font = &UbuntuMono_B10pt8b;
    //font = &bold_led_board_7_regular4pt7b;
    //font = &RepetitionScrolling5pt8b;
    //font = &ChessType9pt7b;
    //font = &Org_01;
    //font = &FreeMonoBold12pt7b;
    //font = &FreeSansBold9pt7b;
    //font = &FreeSans9pt7b;

    matrix.setFont(font);

    int16_t x1 = x0;
    int16_t y1 = y0 - h + 1;

    //erase area and print next second
    matrix.fillRect(x0, y0 - h + 1, w, h, 0);
    matrix.setCursor(x0, y0);
    matrix.print(chr);

    //copy to buffer
    uint16_t buf[w * h];
    matrix.copyBuffer(x0, y0 - h + 1, w, h, buf);

    //erase area again :)
    matrix.fillRect(x0, y0 - h + 1, w, h, 0);

    static int16_t offset = 0;
    uint16_t index = 0;

    for (uint16_t yTemp = 0; yTemp < h; yTemp++)
    {
        for (uint16_t xTemp = 0; xTemp < w; xTemp++)
        {
            if (yTemp < h - offset)
            {
                index = w * yTemp + xTemp;
                matrix.drawPixel(x1 + xTemp, y1 + yTemp + offset, buf[index]);
            }
            else
            {
                // matrix.drawPixel(x1 + xTemp, y1 + yTemp - h + offset, 0);
            }
        }
    }
    offset++;

    //reset offset and change to next animation
    if (offset > h)
    {
        offset = 0;
        return true;
    }
    else
    {
        return false;
    }
}

boolean slide_out_down_min_digit_1(char chr, const GFXfont *font, int16_t x0, int16_t y0, uint16_t w, uint16_t h)
{
    //FONT
    //font = &FreeSerifBold9pt7b;
    //font = &UbuntuMono_B10pt8b;
    //font = &bold_led_board_7_regular4pt7b;
    //font = &RepetitionScrolling5pt8b;
    //font = &ChessType9pt7b;
    //font = &Org_01;
    //font = &FreeMonoBold12pt7b;
    //font = &FreeSansBold9pt7b;
    //font = &FreeSans9pt7b;

    matrix.setFont(font);

    int16_t x1 = x0;
    int16_t y1 = y0 - h + 1;

    //erase area and print next second
    matrix.fillRect(x0, y0 - h + 1, w, h, 0);
    matrix.setCursor(x0, y0);
    matrix.print(chr);

    //copy to buffer
    uint16_t buf[w * h];
    matrix.copyBuffer(x0, y0 - h + 1, w, h, buf);

    //erase area again :)
    matrix.fillRect(x0, y0 - h + 1, w, h, 0);

    static int16_t offset = 0;
    uint16_t index = 0;

    for (uint16_t yTemp = 0; yTemp < h; yTemp++)
    {
        for (uint16_t xTemp = 0; xTemp < w; xTemp++)
        {
            if (yTemp < h - offset)
            {
                index = w * yTemp + xTemp;
                matrix.drawPixel(x1 + xTemp, y1 + yTemp + offset, buf[index]);
            }
            else
            {
                // matrix.drawPixel(x1 + xTemp, y1 + yTemp - h + offset, 0);
            }
        }
    }
    offset++;

    //reset offset and change to next animation
    if (offset > h)
    {
        offset = 0;
        return true;
    }
    else
    {
        return false;
    }
}

boolean slide_out_down_min_digit_2(char chr, const GFXfont *font, int16_t x0, int16_t y0, uint16_t w, uint16_t h)
{
    //FONT
    //font = &FreeSerifBold9pt7b;
    //font = &UbuntuMono_B10pt8b;
    //font = &bold_led_board_7_regular4pt7b;
    //font = &RepetitionScrolling5pt8b;
    //font = &ChessType9pt7b;
    //font = &Org_01;
    //font = &FreeMonoBold12pt7b;
    //font = &FreeSansBold9pt7b;
    //font = &FreeSans9pt7b;

    matrix.setFont(font);

    int16_t x1 = x0;
    int16_t y1 = y0 - h + 1;

    //erase area and print next second
    matrix.fillRect(x0, y0 - h + 1, w, h, 0);
    matrix.setCursor(x0, y0);
    matrix.print(chr);

    //copy to buffer
    uint16_t buf[w * h];
    matrix.copyBuffer(x0, y0 - h + 1, w, h, buf);

    //erase area again :)
    matrix.fillRect(x0, y0 - h + 1, w, h, 0);

    static int16_t offset = 0;
    uint16_t index = 0;

    for (uint16_t yTemp = 0; yTemp < h; yTemp++)
    {
        for (uint16_t xTemp = 0; xTemp < w; xTemp++)
        {
            if (yTemp < h - offset)
            {
                index = w * yTemp + xTemp;
                matrix.drawPixel(x1 + xTemp, y1 + yTemp + offset, buf[index]);
            }
            else
            {
                // matrix.drawPixel(x1 + xTemp, y1 + yTemp - h + offset, 0);
            }
        }
    }
    offset++;

    //reset offset and change to next animation
    if (offset > h)
    {
        offset = 0;
        return true;
    }
    else
    {
        return false;
    }
}

boolean slide_out_down_hour_digit_1(char chr, const GFXfont *font, int16_t x0, int16_t y0, uint16_t w, uint16_t h)
{
    //FONT
    //font = &FreeSerifBold9pt7b;
    //font = &UbuntuMono_B10pt8b;
    //font = &bold_led_board_7_regular4pt7b;
    //font = &RepetitionScrolling5pt8b;
    //font = &ChessType9pt7b;
    //font = &Org_01;
    //font = &FreeMonoBold12pt7b;
    //font = &FreeSansBold9pt7b;
    //font = &FreeSans9pt7b;

    matrix.setFont(font);

    int16_t x1 = x0;
    int16_t y1 = y0 - h + 1;

    //erase area and print next second
    matrix.fillRect(x0, y0 - h + 1, w, h, 0);
    matrix.setCursor(x0, y0);
    matrix.print(chr);

    //copy to buffer
    uint16_t buf[w * h];
    matrix.copyBuffer(x0, y0 - h + 1, w, h, buf);

    //erase area again :)
    matrix.fillRect(x0, y0 - h + 1, w, h, 0);

    static int16_t offset = 0;
    uint16_t index = 0;

    for (uint16_t yTemp = 0; yTemp < h; yTemp++)
    {
        for (uint16_t xTemp = 0; xTemp < w; xTemp++)
        {
            if (yTemp < h - offset)
            {
                index = w * yTemp + xTemp;
                matrix.drawPixel(x1 + xTemp, y1 + yTemp + offset, buf[index]);
            }
            else
            {
                // matrix.drawPixel(x1 + xTemp, y1 + yTemp - h + offset, 0);
            }
        }
    }
    offset++;

    //reset offset and change to next animation
    if (offset > h)
    {
        offset = 0;
        return true;
    }
    else
    {
        return false;
    }
}

boolean slide_out_down_hour_digit_2(char chr, const GFXfont *font, int16_t x0, int16_t y0, uint16_t w, uint16_t h)
{
    //FONT
    //font = &FreeSerifBold9pt7b;
    //font = &UbuntuMono_B10pt8b;
    //font = &bold_led_board_7_regular4pt7b;
    //font = &RepetitionScrolling5pt8b;
    //font = &ChessType9pt7b;
    //font = &Org_01;
    //font = &FreeMonoBold12pt7b;
    //font = &FreeSansBold9pt7b;
    //font = &FreeSans9pt7b;

    matrix.setFont(font);

    int16_t x1 = x0;
    int16_t y1 = y0 - h + 1;

    //erase area and print next second
    matrix.fillRect(x0, y0 - h + 1, w, h, 0);
    matrix.setCursor(x0, y0);
    matrix.print(chr);

    //copy to buffer
    uint16_t buf[w * h];
    matrix.copyBuffer(x0, y0 - h + 1, w, h, buf);

    //erase area again :)
    matrix.fillRect(x0, y0 - h + 1, w, h, 0);

    static int16_t offset = 0;
    uint16_t index = 0;

    for (uint16_t yTemp = 0; yTemp < h; yTemp++)
    {
        for (uint16_t xTemp = 0; xTemp < w; xTemp++)
        {
            if (yTemp < h - offset)
            {
                index = w * yTemp + xTemp;
                matrix.drawPixel(x1 + xTemp, y1 + yTemp + offset, buf[index]);
            }
            else
            {
                // matrix.drawPixel(x1 + xTemp, y1 + yTemp - h + offset, 0);
            }
        }
    }
    offset++;

    //reset offset and change to next animation
    if (offset > h)
    {
        offset = 0;
        return true;
    }
    else
    {
        return false;
    }
}

void process_runningled_page()
{
    // if (MODE != MODE_old)
    // {
    //   MODE_old = MODE;
    //   digitalClockDisplay();
    //   Serial.print(F("> "));
    //   if (MODE == 0)
    //   {
    //     Serial.println(F("MODE 0 [Normal Mode]"));
    //   }
    //   else if (MODE == 1)
    //   {
    //     Serial.println(F("MODE 1 [Config Mode]"));
    //   }
    //   else if (MODE == 2)
    //   {
    //     Serial.println(F("MODE 2 [Edit Mode]"));
    //   }
    // }

    if (MODE == 0)
    {
        if (MODE != MODE_old)
        {
            MODE_old = MODE;
            Serial.println(F("MODE 0 [Normal Mode]"));
        }

        // static bool refresh;
        if (currentPageMode0_old != currentPageMode0)
        {
            currentPageMode0_old = currentPageMode0;
            Serial.print(F("NORMAL MODE, PAGE "));
            Serial.println(currentPageMode0);

            // refresh = true;
            matrix.clearDisplay();
        }

        // -------------------------------------------------------------------
        // MODE 0, PAGE 0, JAM DAN TANGGAL
        // -------------------------------------------------------------------
        if (currentPageMode0 == 0)
        {

            matrix.clearDisplay();

            matrix.setTextWrap(0);

            int16_t x0Temp, y0Temp, x1Temp, y1Temp;
            uint16_t w0Temp, h0Temp, w1Temp, h1Temp;

            int c;
            int row0 = 6;
            int row1 = 15;
            int startCol = 1;
            int x0 = X;
            // int x1 = X;

            if (_ledMatrixSettings.scrollrow_0 == false)
            {
                x0 = 0;
            }

            if (_ledMatrixSettings.scrollrow_1 == false)
            {
                // x1 = 0;
            }

            // -------------------------------------------------------------------
            // ROW 0
            // -------------------------------------------------------------------

            startCol = 15; //override start colum for Row0

            //hour
            c = DISP_REFF + startCol + x0;
            matrix.setFont(&bold_led_board_7_regular4pt7b);
            matrix.setCursor(c, row0);
            if (hourLocal < 10)
            {
                matrix.print("0");
            }
            matrix.print(hourLocal);

            //colon depan
            char bufHour[2];
            dtostrf(hourLocal, 2, 0, bufHour);
            matrix.getTextBounds(bufHour, 0, 0, &x0Temp, &y0Temp, &w0Temp, &h0Temp);
            c = c + w0Temp + 1;
            matrix.setFont(&bold_led_board_7_regular4pt7b);
            matrix.setCursor(c, row0);

            if (secLocal < 30)
            {
                matrix.fillRect(c, row0 - 1, 2, 2, 1);

                // if (blinkColon) {
                if (state1000ms)
                {
                    matrix.fillRect(c, row0 - 4, 2, 2, 1);
                }
                // }
            }
            else
            {
                matrix.fillRect(c, row0 - 4, 2, 2, 1);

                // if (blinkColon) {
                if (state1000ms)
                {
                    matrix.fillRect(c, row0 - 1, 2, 2, 1);
                }
                // }
            }

            //minute
            matrix.getTextBounds((char *)":", 0, 0, &x0Temp, &y0Temp, &w0Temp, &h0Temp);
            c = c + w0Temp + 1;
            matrix.setFont(&bold_led_board_7_regular4pt7b);
            matrix.setCursor(c, row0);
            if (minLocal < 10)
            {
                matrix.print("0");
            }
            matrix.print(minLocal);

            // -------------------------------------------------------------------
            // ROW 1
            // -------------------------------------------------------------------
            // int weekDay = wdayLocal;
            int digitDay = mdayLocal;
            int digitMonth = monthLocal;
            int digitYear = yearLocal;
            // char weekdayBuf[1];
            char dayBuf[2];
            char monthBuf[2];
            char yearBuf[4];
            itoa(digitDay, dayBuf, 10);
            itoa(digitMonth, monthBuf, 10);
            itoa(digitYear, yearBuf, 10);

            static int page;
            int pageMax = 9;

            //Construct the strings
            char contents[32];
            if (page == 0)
            {
                sprintf(contents, "%s", _configLocation.city);
            }
            if (page == 1)
            {
                sprintf(contents, "%s", dayStr(wdayLocal));
            }
            else if (page == 2)
            {
                sprintf(contents, "%d-%s", mdayLocal, monthShortStr(monthLocal));
            }
            else if (page == 3)
            {
                //String degSymbol = "\xB0";
                sprintf(contents, "%.2f%cC", rtcTemperature, char(176));
            }
            else if (page == 4)
            {
                sprintf(contents, "%s %s", sholatNameStr(Fajr), sholatTimeArray[Fajr]);
            }
            else if (page == 5)
            {
                sprintf(contents, "%s %s", sholatNameStr(Sunrise), sholatTimeArray[Sunrise]);
            }
            else if (page == 6)
            {
                sprintf(contents, "%s %s", sholatNameStr(Dhuhr), sholatTimeArray[Dhuhr]);
            }
            else if (page == 7)
            {
                sprintf(contents, "%s %s", sholatNameStr(Asr), sholatTimeArray[Asr]);
            }
            else if (page == 8)
            {
                sprintf(contents, "%s %s", sholatNameStr(Maghrib), sholatTimeArray[Maghrib]);
            }
            else if (page == 9)
            {
                sprintf(contents, "%s %s", sholatNameStr(Isha), sholatTimeArray[Isha]);
            }

            //get text bounds
            const GFXfont *font;
            font = &RepetitionScrolling5pt8b;

            matrix.setFont(font);

            if (font == &TomThumb)
            {
                row1 = 15;
            }
            else if (font == &bold_led_board_7_regular4pt7b)
            {
                row1 = 12;
            }
            else if (font == &ChessType9pt7b)
            {
                row1 = 12;
            }
            else if (font == &FiveBySeven5pt7b)
            {
                row1 = 14;
            }
            else if (font == &F14LED7pt8b)
            {
                row1 = 12;
            }
            else if (font == &RepetitionScrolling5pt8b)
            {
                row1 = 15;
            }
            else if (font == &Org_01)
            {
                row1 = 14;
            }
            else
            {
                row1 = 9;
            }

            matrix.getTextBounds(contents, 0, 0, &x1Temp, &y1Temp, &w1Temp, &h1Temp);
            //wText = w1Temp;
            wText = matrix.width();

            startCol = (matrix.width() - w1Temp) / 2;

            //Print to led matrix

            static byte animation;

            //slide-in Left
            static int x = matrix.width();
            if (animation == 0)
            {
                //x = matrix.width();
                if (x > startCol)
                {
                    x--;
                }
                else if (x == startCol)
                {
                    animation = 1;
                }
            }
            if (animation == 1)
            {
                static byte count;
                count++;
                if (count == 80)
                {
                    count = 0;
                    animation = 2;
                }
            }
            if (animation == 2)
            {
                //x = startCol;
                x--;
                if (x == -w1Temp)
                {
                    animation = 0;
                    x = matrix.width();
                    if (page < pageMax)
                    {
                        page++;
                    }
                    else
                    {
                        page = 0;
                    }
                    //Serial.print(F("page="));
                    //Serial.println(page);
                }
            }

            matrix.setCursor(x, row1);
            matrix.print(contents);
        }

        // -------------------------------------------------------------------
        // MODE 0, PAGE 1, JAM / MENIT MENUJU SHOLAT
        // -------------------------------------------------------------------
        else if (currentPageMode0 == 1)
        {

            matrix.clearDisplay();
            matrix.setTextWrap(0);
            matrix.setFont(&TomThumb);

            int16_t x1Temp, y1Temp;
            uint16_t wTemp, hTemp;

            int c;
            int row0 = 6;
            int row1 = 14;
            byte startCol = 1;
            int x0 = X;
            int x1 = X;

            if (_ledMatrixSettings.scrollrow_0 == false)
            {
                x0 = 0;
            }

            if (_ledMatrixSettings.scrollrow_1 == false)
            {
                x1 = 0;
            }

            char textMin[] = "min";

            if (HOUR != 0)
            {
                // HOUR
                c = DISP_REFF + startCol + x0;
                matrix.setCursor(c, row0);
                matrix.println(bufHOUR);
                matrix.getTextBounds(bufHOUR, 0, 0, &x1Temp, &y1Temp, &wTemp, &hTemp);

                // String "jam"
                char textHour[] = "jam";
                c = c + wTemp - 3;
                matrix.setCursor(c, row0);
                matrix.println(textHour);
                matrix.getTextBounds(textHour, 0, 0, &x1Temp, &y1Temp, &wTemp, &hTemp);

                // MINUTE
                c = c + wTemp - 1;
                matrix.setCursor(c, row0);
                matrix.println(bufMINUTE);
                matrix.getTextBounds(bufMINUTE, 0, 0, &x1Temp, &y1Temp, &wTemp, &hTemp);

                // String "min"
                //char textMin[] = "min";
                c = c + wTemp - 3;
                matrix.setCursor(c, row0);
                matrix.println(textMin);
                matrix.getTextBounds(textMin, 0, 0, &x1Temp, &y1Temp, &wTemp, &hTemp);

                //SECOND
                matrix.getTextBounds(textMin, 0, 0, &x1Temp, &y1Temp, &wTemp, &hTemp);
                c = c + wTemp - 1;
                matrix.setCursor(c, row0);
                matrix.println(bufSECONDMATRIX);

                //String "detik"
                char textSec[] = "dtk";
                matrix.getTextBounds(bufSECONDMATRIX, 0, 0, &x1Temp, &y1Temp, &wTemp, &hTemp);
                c = c + wTemp - 3;
                matrix.setCursor(c, row0);
                matrix.println(textSec);
            }

            else if (HOUR == 0)
            {

                matrix.clearDisplay();

                //MINUTE
                c = DISP_REFF + startCol + x0;
                matrix.setCursor(c, row0);
                matrix.println(bufMINUTE);

                //String "min"
                matrix.getTextBounds(bufMINUTE, 0, 0, &x1Temp, &y1Temp, &wTemp, &hTemp);
                c = c + wTemp - 3;
                matrix.setCursor(c, row0);
                matrix.println(textMin);

                //SECOND
                matrix.getTextBounds(textMin, 0, 0, &x1Temp, &y1Temp, &wTemp, &hTemp);
                c = c + wTemp - 1;
                matrix.setCursor(c, row0);
                matrix.println(bufSECONDMATRIX);

                //String "detik"
                char textSec[] = "detik";
                matrix.getTextBounds(bufSECONDMATRIX, 0, 0, &x1Temp, &y1Temp, &wTemp, &hTemp);
                c = c + wTemp - 3;
                matrix.setCursor(c, row0);
                matrix.println(textSec);
            }

            //String Time Name
            char textTo[] = "menuju";
            c = DISP_REFF + startCol + x1;
            matrix.setCursor(c, row1);
            matrix.print(textTo);

            // TimeName[NEXTTIMEID]
            matrix.getTextBounds(textTo, 1, 14, &x1Temp, &y1Temp, &wTemp, &hTemp);
            c = c + wTemp - 2;

            int16_t x1Temp1, y1Temp1;
            uint16_t wTemp1, hTemp1;

            // char JUMUAH[] = "JUMU'AH";

            // if (weekmdayLocal == 6 && NEXTTIMEID == Dhuhr)
            // {
            //   matrix.getTextBounds(JUMUAH, 0, 0, &x1Temp1, &y1Temp1, &wTemp1, &hTemp1);
            // }
            // else
            // {
            //   matrix.getTextBounds(sholatNameStr(NEXTTIMEID), 0, 0, &x1Temp1, &y1Temp1, &wTemp1, &hTemp1);
            // }

            matrix.getTextBounds(sholatNameStr(NEXTTIMEID), 0, 0, &x1Temp1, &y1Temp1, &wTemp1, &hTemp1);

            //animate time name
            static byte count;
            byte maxCount = 50 / (_ledMatrixSettings.scrollspeed + 1);
            if (count < maxCount)
            {
                count++;
            }
            else if (count >= maxCount)
            {
                count = 0;
            }

            static byte animation;
            static byte i;

            //scroll Up
            if (animation == 0)
            {
                if (count == 0)
                {
                    row1 = row1 + hTemp1 - i;
                    i++;
                    if (i == hTemp1)
                    {
                        animation = 2;
                        i = 0;
                    }
                }
                else
                {
                    row1 = row1 + hTemp1 - i;
                }
            }

            //scroll Down
            else if (animation == 1)
            {
                if (count == 0)
                {
                    row1 = row1 + i;
                    i++;
                    if (i == hTemp1)
                    {
                        animation = 3;
                        i = 0;
                    }
                }
                else
                {
                    row1 = row1 + i;
                }
            }

            //no animation after scroll Up
            else if (animation == 2)
            {
                static byte count;
                count++;
                if (count == 120)
                {
                    count = 0;
                    animation = 1;
                }
            }

            //no animation after scroll Down
            else if (animation == 3)
            {
                static byte count;
                count++;
                row1 = row1 + hTemp1;
                if (count == 20)
                {
                    count = 0;
                    animation = 0;
                }
            }

            matrix.setCursor(c, row1);

            // if (weekmdayLocal == 6 && NEXTTIMEID == Dhuhr)
            // {
            //   matrix.print(JUMUAH);
            // }
            // else
            // {
            //   matrix.print(sholatNameStr(NEXTTIMEID));
            // }

            matrix.print(sholatNameStr(NEXTTIMEID));

            wText = DISP_REFF + wTemp + wTemp1;

            //matrix.fillRect(c, row1+3, wTemp1, hTemp1, 1);
            //c = matrix.getCursorX();
            matrix.fillRect(c, 14, wTemp1, hTemp1, 0);

            //matrix.fillScreen(1);
        }

        // -------------------------------------------------------------------
        // MODE 0, PAGE 2, WAKTU SHOLAT SAAT INI
        // -------------------------------------------------------------------
        else if (currentPageMode0 == 2)
        {

            matrix.clearDisplay();
            matrix.setTextWrap(0);

            int c;
            int row0 = 6;
            int row1 = 14;
            // byte startCol = 1;
            // int x0 = X;
            // int x1 = X;

            // if (_ledMatrixSettings.scrollrow_0 == false)
            // {
            //   x0 = 0;
            // }

            // if (_ledMatrixSettings.scrollrow_1 == false)
            // {
            //   x1 = 0;
            // }

            // -------------------------------------------------------------------
            // ROW 1
            // -------------------------------------------------------------------

            char buf[15];
            const char *strRow1 = PSTR("MASUK WAKTU");

            strcpy_P(buf, strRow1);

            int16_t x0Temp, y0Temp;
            uint16_t w0Temp, h0Temp;

            matrix.setFont(&TomThumb);
            matrix.getTextBounds(buf, 0, 0, &x0Temp, &y0Temp, &w0Temp, &h0Temp);
            //wText = w0Temp;

            //center
            c = (64 - w0Temp) / 2;

            //Print to led matrix
            matrix.setCursor(c, row0);
            matrix.print(buf);

            // -------------------------------------------------------------------
            // ROW 2
            // -------------------------------------------------------------------

            strlcpy(buf, sholatNameStr(CURRENTTIMEID), sizeof(buf));

            int16_t x1Temp, y1Temp;
            uint16_t w1Temp, h1Temp;

            matrix.setFont(&bold_led_board_7_regular4pt7b);
            matrix.getTextBounds(buf, 0, 0, &x1Temp, &y1Temp, &w1Temp, &h1Temp);
            //wText = w0Temp;

            //center
            c = (64 - w1Temp) / 2;

            //Print to led matrix
            matrix.setCursor(c, row1);
            matrix.print(buf);
        }

        // -------------------------------------------------------------------
        // MODE 0, PAGE 3, JAM SAJA
        // -------------------------------------------------------------------
        else if (currentPageMode0 == 3)
        {
            byte typePage3 = 1;

            if (typePage3 == 0)
            {
                matrix.clearDisplay();
                matrix.setTextWrap(0);
                matrix.setFont(&TomThumb);

                // int16_t x1Temp, y1Temp;
                // uint16_t wTemp, hTemp;

                int c;
                // int row0 = 6;
                int row1 = 13;
                byte startCol = 1;

                c = DISP_REFF + startCol;

                //matrix.setFont(&UbuntuMono_B10pt8b);
                //matrix.setFont(&bold_led_board_7_regular4pt7b);
                //matrix.setFont(&RepetitionScrolling5pt8b);
                //matrix.setFont(&ChessType9pt7b);
                //matrix.setFont(&FreeSerifBold9pt7b);
                //matrix.setFont(&Org_01);
                //matrix.setFont(&FreeMonoBold12pt7b);
                //matrix.setFont(&FreeSansBold9pt7b);
                //matrix.setFont(&FreeSans9pt7b);

                const GFXfont *font;

                font = &FreeSerifBold9pt7b;

                matrix.setFont(font);

                // //break time element for NOW time
                // time_t t = localTime;
                // tmElements_t tmSet;
                // breakTime(t, tmSet);

                //array for time Now
                char bufNow[6];

                //format (add leading '0' if  less than 10) and store in the array
                sprintf(bufNow, "%02d%02d%02d", hourLocal, minLocal, secLocal);

                // print Colon
                matrix.fillRect(c + 19, row1 - 9, 2, 2, 1);
                matrix.fillRect(c + 19, row1 - 3, 2, 2, 1);
                //matrix.fillRect(c + 41, row1 - 8, 2, 2, 1);
                //matrix.fillRect(c + 41, row1 - 1, 2, 2, 1);

                // print Hour
                matrix.setCursor(c, row1);
                matrix.print(bufNow[0]);
                matrix.print(bufNow[1]);

                //  //if Hour is les than 10, erase the leading zero
                //  if (bufNow[0] == '0') {
                //    matrix.fillRect(1, 2, 9, 12, 0);
                //  }

                // print Minute
                c = matrix.getCursorX();
                matrix.setCursor(c + 4, row1);
                matrix.print(bufNow[2]);
                matrix.print(bufNow[3]);

                // print Second
                c = matrix.getCursorX();

                font = &bold_led_board_7_regular4pt7b;
                matrix.setFont(font);

                matrix.setCursor(c + 4, row1);
                matrix.print(bufNow[4]);

                if (font == &FreeSerifBold9pt7b)
                {
                    matrix.setCursor(c + 13, row1);
                }
                if (font == &bold_led_board_7_regular4pt7b)
                {
                    matrix.setCursor(c + 12, row1);
                }

                matrix.print(bufNow[5]);

                //revert font again
                matrix.setFont(&FreeSerifBold9pt7b);

                //      // initial animation: slide-out-down
                //      static uint8_t animation = 2;

                //array for 1 second ahead
                static char bufTimeFuture[6];

                //counter to determine when to trigger the animation
                int sec;
                static int second_old = 0;
                sec = bufNow[5];
                // static unsigned long currMillis;
                static uint16_t count = 0;
                if (sec != second_old)
                {
                    second_old = sec;
                    // currMillis = millis();
                    count = 0;
                }

                static boolean animateSecDigit1;
                static boolean animateSecDigit2;
                static boolean animateMinDigit1;
                static boolean animateMinDigit2;
                static boolean animateHourDigit1;
                static boolean animateHourDigit2;

                if (count == 1000 / _ledMatrixSettings.scrollspeed - 25)
                {
                    //break time element
                    uint32_t t_future = localTime + 1;
                    RtcDateTime dt;
                    dt.InitWithEpoch32Time(t_future);

                    //format (add leading '0' if less than 10) and store in the array
                    sprintf(bufTimeFuture, "%02d%02d%02d", dt.Hour(), dt.Minute(), dt.Second());

                    //animateSecDigit2 = true;

                    if (bufNow[5] == '9')
                    {
                        //animateSecDigit1 = true;
                        if (bufNow[4] == '5')
                        {
                            animateMinDigit2 = true;
                            if (bufNow[3] == '9')
                            {
                                animateMinDigit1 = true;
                                if (bufNow[2] == '5')
                                {
                                    animateHourDigit2 = true;
                                    if (bufNow[1] == '3' || bufNow[1] == '9')
                                    {
                                        animateHourDigit1 = true;
                                    }
                                }
                            }
                        }
                    }
                }

                if (count == 1000 / _ledMatrixSettings.scrollspeed - 15)
                {

                    animateSecDigit2 = true;

                    if (bufNow[5] == '9')
                    {
                        animateSecDigit1 = true;
                    }
                }

                count++;

                if (animateSecDigit2)
                {
                    static int animation = 0;

                    //slide-out-down
                    if (animation == 0)
                    {
                        //boolean slide_in_down (char chr, const GFXfont *font, int16_t x0, int16_t y0, uint16_t w, uint16_t h);
                        boolean slideOutDown = slide_out_down_sec_digit_2(bufNow[5], &bold_led_board_7_regular4pt7b, 53, 13, 7, 7);
                        if (slideOutDown == true)
                        {
                            animation = 1;
                        }
                    }

                    //slide-in-down
                    if (animation == 1)
                    {
                        //boolean slide_in_down (char chr, const GFXfont *font, int16_t x0, int16_t y0, uint16_t w, uint16_t h);
                        boolean slideInDown = slide_in_down_sec_digit_2(bufTimeFuture[5], &bold_led_board_7_regular4pt7b, 53, 13, 7, 7);
                        if (slideInDown == true)
                        {
                            animateSecDigit2 = false;
                            animation = 0;
                        }
                    }
                }

                if (animateSecDigit1)
                {
                    static int animation = 0;

                    //slide-out-down
                    if (animation == 0)
                    {
                        //boolean slide_in_down (char chr, const GFXfont *font, int16_t x0, int16_t y0, uint16_t w, uint16_t h);
                        boolean slideOutDown = slide_out_down_sec_digit_1(bufNow[4], &bold_led_board_7_regular4pt7b, 45, 13, 7, 7);
                        if (slideOutDown == true)
                        {
                            animation = 1;
                        }
                    }

                    //slide-in-down
                    if (animation == 1)
                    {
                        //boolean slide_in_down (char chr, const GFXfont *font, int16_t x0, int16_t y0, uint16_t w, uint16_t h);
                        boolean slideInDown = slide_in_down_sec_digit_1(bufTimeFuture[4], &bold_led_board_7_regular4pt7b, 45, 13, 7, 7);
                        if (slideInDown == true)
                        {
                            animateSecDigit1 = false;
                            animation = 0;
                        }
                    }
                }

                if (animateMinDigit2)
                {
                    static int animation = 0;

                    //slide-out-down
                    if (animation == 0)
                    {
                        //boolean slide_in_down (char chr, const GFXfont *font, int16_t x0, int16_t y0, uint16_t w, uint16_t h);
                        boolean slideOutDown = slide_out_down_min_digit_2(bufNow[3], &FreeSerifBold9pt7b, 32, 13, 9, 12);
                        if (slideOutDown == true)
                        {
                            animation = 1;
                        }
                    }

                    //slide-in-down
                    if (animation == 1)
                    {
                        //boolean slide_in_down (char chr, const GFXfont *font, int16_t x0, int16_t y0, uint16_t w, uint16_t h);
                        boolean slideInDown = slide_in_down_min_digit_2(bufTimeFuture[3], &FreeSerifBold9pt7b, 32, 13, 9, 12);
                        if (slideInDown == true)
                        {
                            animateMinDigit2 = false;
                            animation = 0;
                        }
                    }
                }

                if (animateMinDigit1)
                {
                    static int animation = 0;

                    //slide-out-down
                    if (animation == 0)
                    {
                        //boolean slide_in_down (char chr, const GFXfont *font, int16_t x0, int16_t y0, uint16_t w, uint16_t h);
                        boolean slideOutDown = slide_out_down_min_digit_1(bufNow[2], &FreeSerifBold9pt7b, 23, 13, 9, 12);
                        if (slideOutDown == true)
                        {
                            animation = 1;
                        }
                    }

                    //slide-in-down
                    if (animation == 1)
                    {
                        //boolean slide_in_down (char chr, const GFXfont *font, int16_t x0, int16_t y0, uint16_t w, uint16_t h);
                        boolean slideInDown = slide_in_down_min_digit_1(bufTimeFuture[2], &FreeSerifBold9pt7b, 23, 13, 9, 12);
                        if (slideInDown == true)
                        {
                            animateMinDigit1 = false;
                            animation = 0;
                        }
                    }
                }

                if (animateHourDigit2)
                {
                    static int animation = 0;

                    //slide-out-down
                    if (animation == 0)
                    {
                        //boolean slide_in_down (char chr, const GFXfont *font, int16_t x0, int16_t y0, uint16_t w, uint16_t h);
                        boolean slideOutDown = slide_out_down_hour_digit_2(bufNow[1], &FreeSerifBold9pt7b, 10, 13, 9, 12);
                        if (slideOutDown == true)
                        {
                            animation = 1;
                        }
                    }

                    //slide-in-down
                    if (animation == 1)
                    {
                        //boolean slide_in_down (char chr, const GFXfont *font, int16_t x0, int16_t y0, uint16_t w, uint16_t h);
                        boolean slideInDown = slide_in_down_hour_digit_2(bufTimeFuture[1], &FreeSerifBold9pt7b, 10, 13, 9, 12);
                        if (slideInDown == true)
                        {
                            animateHourDigit2 = false;
                            animation = 0;
                        }
                    }
                }

                if (animateHourDigit1)
                {
                    static int animation = 0;

                    //slide-out-down
                    if (animation == 0)
                    {
                        //boolean slide_in_down (char chr, const GFXfont *font, int16_t x0, int16_t y0, uint16_t w, uint16_t h);
                        boolean slideOutDown = slide_out_down_hour_digit_1(bufNow[0], &FreeSerifBold9pt7b, 1, 13, 9, 12);
                        if (slideOutDown == true)
                        {
                            animation = 1;
                        }
                    }

                    //slide-in-down
                    if (animation == 1)
                    {
                        //boolean slide_in_down (char chr, const GFXfont *font, int16_t x0, int16_t y0, uint16_t w, uint16_t h);
                        boolean slideInDown = slide_in_down_hour_digit_1(bufTimeFuture[0], &FreeSerifBold9pt7b, 1, 13, 9, 12);
                        if (slideInDown == true)
                        {
                            animateHourDigit1 = false;
                            animation = 0;
                        }
                    }
                }
            }

            else if (typePage3 == 1)
            {
                matrix.clearDisplay();
                matrix.setTextWrap(0);
                matrix.setFont(&TomThumb);

                // int16_t x1Temp, y1Temp;
                // uint16_t wTemp, hTemp;

                int c;
                int row0 = 6;
                int row1 = 14;
                byte startCol = 1;

                c = DISP_REFF + startCol;

                //matrix.setFont(&UbuntuMono_B10pt8b);
                //matrix.setFont(&bold_led_board_7_regular4pt7b);
                //matrix.setFont(&RepetitionScrolling5pt8b);
                //matrix.setFont(&ChessType9pt7b);
                //matrix.setFont(&FreeSerifBold9pt7b);
                //matrix.setFont(&Org_01);
                //matrix.setFont(&FreeMonoBold12pt7b);
                //matrix.setFont(&FreeSansBold9pt7b);
                //matrix.setFont(&FreeSans9pt7b);
                //matrix.setFont(&FreeSerifBoldNumberOnly11pt7b);

                const GFXfont *font;

                font = &FreeSerifBoldNumberOnly11pt7b;

                matrix.setFont(font);

                //array for time Now
                char bufNow[7];

                //format (add leading '0' if less than 10) and store in the array
                sprintf(bufNow, "%02d%02d%02d", hourLocal, minLocal, secLocal);

                // print Colon
                matrix.fillRect(c + 22, row1 - 9, 2, 2, 1);
                matrix.fillRect(c + 22, row1 - 4, 2, 2, 1);
                //matrix.fillRect(c + 41, row1 - 8, 2, 2, 1);
                //matrix.fillRect(c + 41, row1 - 1, 2, 2, 1);

                // print Hour
                matrix.setCursor(c, row1);
                matrix.print(bufNow[0]);
                matrix.print(bufNow[1]);

                //  //if Hour is les than 10, erase the leading zero
                //  if (bufNow[0] == '0') {
                //    matrix.fillRect(1, 2, 9, 12, 0);
                //  }

                // print Minute
                c = matrix.getCursorX();
                matrix.setCursor(c + 3, row1);
                matrix.print(bufNow[2]);
                matrix.print(bufNow[3]);

                // print Second
                c = matrix.getCursorX();

                font = &bold_led_board_7_regular4pt7b;
                matrix.setFont(font);

                matrix.setCursor(c, row1);
                matrix.print(bufNow[4]);

                if (font == &FreeSerifBold9pt7b)
                {
                    matrix.setCursor(c + 13, row1);
                }
                if (font == &bold_led_board_7_regular4pt7b)
                {
                    matrix.setCursor(c + 8, row1);
                }
                matrix.print(bufNow[5]);

                // print AM/PM
                matrix.setCursor(c, row0);
                if (hourLocal < 12)
                {
                    //matrix.print("AM");
                }
                else
                {
                    //matrix.print("PM");
                }

                //revert font again
                font = &FreeSerifBoldNumberOnly11pt7b;
                matrix.setFont(font);

                //array for 1 second ahead
                static char bufTimeFuture[6];

                //counter to determine when to trigger the animation
                int sec;
                static int second_old = 0;
                sec = bufNow[5];
                // static unsigned long currMillis;
                static uint16_t count = 0;
                if (sec != second_old)
                {
                    second_old = sec;
                    // currMillis = millis();
                    count = 0;
                }

                static boolean animateSecDigit1;
                static boolean animateSecDigit2;
                static boolean animateMinDigit1;
                static boolean animateMinDigit2;
                static boolean animateHourDigit1;
                static boolean animateHourDigit2;

                if (count == 1000 / _ledMatrixSettings.scrollspeed - 31)
                {
                    //break time element
                    uint32_t t_future = localTime + 1;
                    RtcDateTime dt;
                    dt.InitWithEpoch32Time(t_future);

                    //format (add leading '0' if less than 10) and store in the array
                    sprintf(bufTimeFuture, "%02d%02d%02d", dt.Hour(), dt.Minute(), dt.Second());

                    //animateSecDigit2 = true;

                    if (bufNow[5] == '9')
                    {
                        //animateSecDigit1 = true;
                        if (bufNow[4] == '5')
                        {
                            animateMinDigit2 = true;
                            if (bufNow[3] == '9')
                            {
                                animateMinDigit1 = true;
                                if (bufNow[2] == '5')
                                {
                                    animateHourDigit2 = true;
                                    if (bufNow[1] == '3' || bufNow[1] == '9')
                                    {
                                        animateHourDigit1 = true;
                                    }
                                }
                            }
                        }
                    }
                }

                if (count == 1000 / _ledMatrixSettings.scrollspeed - 15)
                {

                    animateSecDigit2 = true;

                    if (bufNow[5] == '9')
                    {
                        animateSecDigit1 = true;
                    }
                }

                count++;

                if (animateSecDigit2)
                {
                    static int animation = 0;

                    //slide-out-down
                    if (animation == 0)
                    {
                        //boolean slide_in_down (char chr, const GFXfont *font, int16_t x0, int16_t y0, uint16_t w, uint16_t h);
                        boolean slideOutDown = slide_out_down_sec_digit_2(bufNow[5], &bold_led_board_7_regular4pt7b, 56, 14, 7, 7);
                        if (slideOutDown == true)
                        {
                            animation = 1;
                        }
                    }

                    //slide-in-down
                    if (animation == 1)
                    {
                        //boolean slide_in_down (char chr, const GFXfont *font, int16_t x0, int16_t y0, uint16_t w, uint16_t h);
                        boolean slideInDown = slide_in_down_sec_digit_2(bufTimeFuture[5], &bold_led_board_7_regular4pt7b, 56, 14, 7, 7);
                        if (slideInDown == true)
                        {
                            animateSecDigit2 = false;
                            animation = 0;
                        }
                    }
                }

                if (animateSecDigit1)
                {
                    static int animation = 0;

                    //slide-out-down
                    if (animation == 0)
                    {
                        //boolean slide_in_down (char chr, const GFXfont *font, int16_t x0, int16_t y0, uint16_t w, uint16_t h);
                        boolean slideOutDown = slide_out_down_sec_digit_1(bufNow[4], &bold_led_board_7_regular4pt7b, 48, 14, 7, 7);
                        if (slideOutDown == true)
                        {
                            animation = 1;
                        }
                    }

                    //slide-in-down
                    if (animation == 1)
                    {
                        //boolean slide_in_down (char chr, const GFXfont *font, int16_t x0, int16_t y0, uint16_t w, uint16_t h);
                        boolean slideInDown = slide_in_down_sec_digit_1(bufTimeFuture[4], &bold_led_board_7_regular4pt7b, 48, 14, 7, 7);
                        if (slideInDown == true)
                        {
                            animateSecDigit1 = false;
                            animation = 0;
                        }
                    }
                }

                if (animateMinDigit2)
                {
                    static int animation = 0;

                    //slide-out-down
                    if (animation == 0)
                    {
                        //boolean slide_in_down (char chr, const GFXfont *font, int16_t x0, int16_t y0, uint16_t w, uint16_t h);
                        boolean slideOutDown = slide_out_down_min_digit_2(bufNow[3], &FreeSerifBoldNumberOnly11pt7b, 37, 14, 11, 15);
                        if (slideOutDown == true)
                        {
                            animation = 1;
                        }
                    }

                    //slide-in-down
                    if (animation == 1)
                    {
                        //boolean slide_in_down (char chr, const GFXfont *font, int16_t x0, int16_t y0, uint16_t w, uint16_t h);
                        boolean slideInDown = slide_in_down_min_digit_2(bufTimeFuture[3], &FreeSerifBoldNumberOnly11pt7b, 37, 14, 11, 15);
                        if (slideInDown == true)
                        {
                            animateMinDigit2 = false;
                            animation = 0;
                        }
                    }
                }

                if (animateMinDigit1)
                {
                    static int animation = 0;

                    //slide-out-down
                    if (animation == 0)
                    {
                        //boolean slide_in_down (char chr, const GFXfont *font, int16_t x0, int16_t y0, uint16_t w, uint16_t h);
                        boolean slideOutDown = slide_out_down_min_digit_1(bufNow[2], &FreeSerifBoldNumberOnly11pt7b, 26, 14, 11, 15);
                        if (slideOutDown == true)
                        {
                            animation = 1;
                        }
                    }

                    //slide-in-down
                    if (animation == 1)
                    {
                        //boolean slide_in_down (char chr, const GFXfont *font, int16_t x0, int16_t y0, uint16_t w, uint16_t h);
                        boolean slideInDown = slide_in_down_min_digit_1(bufTimeFuture[2], &FreeSerifBoldNumberOnly11pt7b, 26, 14, 11, 15);
                        if (slideInDown == true)
                        {
                            animateMinDigit1 = false;
                            animation = 0;
                        }
                    }
                }

                if (animateHourDigit2)
                {
                    static int animation = 0;

                    //slide-out-down
                    if (animation == 0)
                    {
                        //boolean slide_in_down (char chr, const GFXfont *font, int16_t x0, int16_t y0, uint16_t w, uint16_t h);
                        boolean slideOutDown = slide_out_down_hour_digit_2(bufNow[1], &FreeSerifBoldNumberOnly11pt7b, 12, 14, 11, 15);
                        if (slideOutDown == true)
                        {
                            animation = 1;
                        }
                    }

                    //slide-in-down
                    if (animation == 1)
                    {
                        //boolean slide_in_down (char chr, const GFXfont *font, int16_t x0, int16_t y0, uint16_t w, uint16_t h);
                        boolean slideInDown = slide_in_down_hour_digit_2(bufTimeFuture[1], &FreeSerifBoldNumberOnly11pt7b, 12, 14, 11, 15);
                        if (slideInDown == true)
                        {
                            animateHourDigit2 = false;
                            animation = 0;
                        }
                    }
                }

                if (animateHourDigit1)
                {
                    static int animation = 0;

                    //slide-out-down
                    if (animation == 0)
                    {
                        //boolean slide_in_down (char chr, const GFXfont *font, int16_t x0, int16_t y0, uint16_t w, uint16_t h);
                        boolean slideOutDown = slide_out_down_hour_digit_1(bufNow[0], &FreeSerifBoldNumberOnly11pt7b, 1, 14, 11, 15);
                        if (slideOutDown == true)
                        {
                            animation = 1;
                        }
                    }

                    //slide-in-down
                    if (animation == 1)
                    {
                        //boolean slide_in_down (char chr, const GFXfont *font, int16_t x0, int16_t y0, uint16_t w, uint16_t h);
                        boolean slideInDown = slide_in_down_hour_digit_1(bufTimeFuture[0], &FreeSerifBoldNumberOnly11pt7b, 1, 14, 11, 15);
                        if (slideInDown == true)
                        {
                            animateHourDigit1 = false;
                            animation = 0;
                        }
                    }
                }
            }
        }

        // -------------------------------------------------------------------
        // MODE 0, PAGE 4, KWH 1
        // -------------------------------------------------------------------
        else if (currentPageMode0 == 4)
        {

            matrix.clearDisplay();

            matrix.setTextWrap(0);

            // int16_t x0Temp;
            // int16_t y0Temp;
            // int16_t x1Temp;
            // int16_t y1Temp;
            uint16_t w0Temp;
            // uint16_t h0Temp;
            uint16_t w1Temp;
            // uint16_t h1Temp;

            int c;
            int row0 = 7;
            int row1 = 15;
            byte startCol = 1;
            int x0 = X;
            int x1 = X;

            if (_ledMatrixSettings.scrollrow_0 == false)
            {
                x0 = 0;
            }

            if (_ledMatrixSettings.scrollrow_1 == false)
            {
                x1 = 0;
            }

            // Wattage
            c = DISP_REFF + startCol - 1 + x0;
            matrix.setFont(&FiveBySeven5pt7b);
            matrix.setCursor(c, row0);
            matrix.print(bufWatt);
            //matrix.getTextBounds(bufWatt, 0, 0, &x0Temp, &y0Temp, &w0Temp, &h0Temp);

            // String "Watt"
            char textWatt[] = "Watt";
            c = DISP_REFF + startCol + x1;
            matrix.setFont(&TomThumb);
            matrix.setCursor(c, row1);
            matrix.println(textWatt);
            //matrix.getTextBounds(textWatt, 0, 0, &x1Temp, &y1Temp, &w1Temp, &h1Temp);

            // Voltage
            c = c + 30 + x0;
            matrix.setFont(&TomThumb);
            matrix.setCursor(c, row0);
            matrix.print(bufVoltage);
            //w0Temp = matrix.getCursorX();

            // String "Volt"
            char textVolt[] = "Volt";
            c = c + -3 + x1;
            matrix.setFont(&TomThumb);
            matrix.setCursor(c, row1);
            matrix.print(textVolt);
            //matrix.getTextBounds(textWatt, 0, 0, &x1Temp, &y1Temp, &w1Temp, &h1Temp);
            //w1Temp = matrix.getCursorX();

            // Temperature
            c = c + 21 + x0;
            matrix.setFont(&TomThumb);
            matrix.setCursor(c, row0);

            char bufRtcTemp[5];
            matrix.print(dtostrf(rtcTemperature, 4, 1, bufRtcTemp));
            w0Temp = matrix.getCursorX();

            // String "degC"
            char textDegC[] = "degC";
            c = c + -1 + x1;
            matrix.setFont(&TomThumb);
            matrix.setCursor(c, row1);
            matrix.print(textDegC);
            //matrix.getTextBounds(textWatt, 0, 0, &x1Temp, &y1Temp, &w1Temp, &h1Temp);
            w1Temp = matrix.getCursorX();

            if (w0Temp > w1Temp)
            {
                wText = w0Temp;
            }
            else
            {
                wText = w1Temp;
            }
        }

        // -------------------------------------------------------------------
        // MODE 0, PAGE 5, LONG TEXT
        // -------------------------------------------------------------------
        else if (currentPageMode0 == 5)
        {

            matrix.setTextWrap(0);

            // int16_t x1Temp, y1Temp;
            // uint16_t wTemp, hTemp;

            int c;
            // int row0 = 6;
            // int row1 = 14;
            byte startCol = 1;

            if (_ledMatrixSettings.scrollrow_0 == false)
            {
                c = DISP_REFF + startCol;
            }

            if (_ledMatrixSettings.scrollrow_1 == false)
            {
                c = DISP_REFF + startCol;
            }

            c = DISP_REFF + startCol + X;

            matrix.clearDisplay();

            matrix.setTextSize(1);

            const GFXfont *font = &F14LED7pt8b;
            matrix.setFont(font);

            if (font == &TomThumb)
            {
                matrix.setCursor(c, 14);
            }
            else if (font == &bold_led_board_7_regular4pt7b)
            {
                matrix.setCursor(c, 12);
            }
            else if (font == &ChessType9pt7b)
            {
                matrix.setCursor(c, 12);
            }
            else if (font == &FiveBySeven5pt7b)
            {
                matrix.setCursor(c, 14);
            }
            else if (font == &F14LED7pt8b)
            {
                matrix.setCursor(c, 12);
            }

            load_running_text();

            // -------------------------------------------------------------------
            // Animation
            // -------------------------------------------------------------------
            boolean effect0 = 0;
            // boolean effect1 = 0;

            if (effect0 == 1)
            {
                static uint16_t scan = 0;
                //uint16_t scanMax = _ledMatrixSettings.scrollspeed / 7;
                uint16_t scanMax = _ledMatrixSettings.scrollspeed;
                if (scan < scanMax)
                {
                    scan++;
                }
                else if (scan >= scanMax)
                {
                    scan = 0;
                }

                //    int16_t x0 = 2;
                //    int16_t y0 = 3;
                //    int16_t x1 = 2;
                //    int16_t y1 = 3;
                //    uint16_t w = 14;
                //    uint16_t h = 10;
            }

            if (effect0 == 1)
            {
                int16_t x0 = 0;
                int16_t y0 = 0;
                int16_t x1 = 0;
                int16_t y1 = 0;
                uint16_t w = 64;
                uint16_t h = 16;

                static int16_t offset;
                static uint16_t offsetMax;

                static uint8_t animation = 6;

                uint16_t index;
                uint16_t buf[1024];

                matrix.copyBuffer(x0, y0, w, h, buf);
                matrix.fillRect(x0, y0, w, h, 0);

                for (uint16_t yTemp = 0; yTemp < h; yTemp++)
                {
                    for (uint16_t xTemp = 0; xTemp < w; xTemp++)
                    {

                        //slide-out-up
                        if (animation == 0)
                        {
                            offsetMax = h;
                            if (yTemp >= offset)
                            {
                                matrix.drawPixel(x1 + xTemp, y1 + yTemp - offset, buf[index]);
                            }
                            else
                            {
                                matrix.drawPixel(x1 + xTemp, y1 + yTemp - offset, 0);
                            }
                        }

                        //slide-out-down
                        else if (animation == 1)
                        {
                            offsetMax = h;
                            if (yTemp < h - offset)
                            {
                                matrix.drawPixel(x1 + xTemp, y1 + yTemp + offset, buf[index]);
                            }
                            else
                            {
                                matrix.drawPixel(x1 + xTemp, y1 + yTemp + offset, 0);
                            }
                        }

                        //slide-out-right
                        else if (animation == 2)
                        {
                            offsetMax = w;
                            if (xTemp < w - offset)
                            {
                                matrix.drawPixel(x1 + xTemp + offset, y1 + yTemp, buf[index]);
                            }
                            else
                            {
                                matrix.drawPixel(x1 + xTemp + offset, y1 + yTemp, 0);
                            }
                        }

                        //slide-out-to-left
                        else if (animation == 3)
                        {
                            offsetMax = w;
                            if (xTemp >= offset)
                            {
                                matrix.drawPixel(x1 + xTemp - offset, y1 + yTemp, buf[index]);
                            }
                            else
                            {
                                matrix.drawPixel(x1 + xTemp - offset, y1 + yTemp, 0);
                            }
                        }

                        //slide-in-right
                        else if (animation == 4)
                        {
                            offsetMax = w;
                            if (xTemp >= w - offset)
                            {
                                matrix.drawPixel(x1 + xTemp - w + offset, y1 + yTemp, buf[index]);
                            }
                            else
                            {
                                matrix.drawPixel(x1 + xTemp - w + offset, y1 + yTemp, 0);
                            }
                        }

                        //slide-in-up
                        else if (animation == 5)
                        {
                            offsetMax = h;
                            if (yTemp < offset)
                            {
                                matrix.drawPixel(x1 + xTemp, y1 + yTemp + h - offset, buf[index]);
                            }
                            else
                            {
                                matrix.drawPixel(x1 + xTemp, y1 + yTemp + h - offset, 0);
                            }
                        }

                        //slide-in-down
                        else if (animation == 6)
                        {
                            offsetMax = h;
                            if (yTemp >= h - offset)
                            {
                                matrix.drawPixel(x1 + xTemp, y1 + yTemp - h + offset, buf[index]);
                            }
                            else
                            {
                                matrix.drawPixel(x1 + xTemp, y1 + yTemp - h + offset, 0);
                            }
                        }

                        //slide-in-left
                        else if (animation == 7)
                        {
                            offsetMax = w;
                            if (xTemp < offset)
                            {
                                matrix.drawPixel(x1 + xTemp + w - offset, y1 + yTemp, buf[index]);
                            }
                            else
                            {
                                matrix.drawPixel(x1 + xTemp + w - offset, y1 + yTemp, 0);
                            }
                        }

                        index++;
                    }
                }

                offset++;

                //reset offset and change animation
                if (offset > offsetMax)
                {
                    offset = 0;
                    if (animation == 6)
                    {
                        animation = 3;
                    }
                    else if (animation == 3)
                    {
                        animation = 4;
                    }
                    else if (animation == 4)
                    {
                        animation = 1;
                    }
                    else if (animation == 1)
                    {
                        animation = 5;
                    }
                    else if (animation == 5)
                    {
                        animation = 2;
                    }
                    else if (animation == 2)
                    {
                        animation = 7;
                    }
                    else if (animation == 7)
                    {
                        animation = 0;
                    }
                    else if (animation == 0)
                    {
                        animation = 6;
                    }
                }
            }
        }
        // -------------------------------------------------------------------
        // MODE 0, PAGE 6, TEMPERATURE DAN JAM
        // -------------------------------------------------------------------
        else if (currentPageMode0 == 6)
        {

            matrix.clearDisplay();
            matrix.setTextWrap(0);

            // int16_t x1Temp, y1Temp;
            // uint16_t w1Temp, h1Temp;

            // char *str;
            // int16_t stopCol;
            // int16_t stopRow;
            // const GFXfont *font;

            uint8_t pageMax = 1;

            static uint8_t exitAnimation = 0;
            static uint8_t nextPageAnimation = 0;

            // ------------------------
            // CURRENT PAGE
            // ------------------------

            static int currentPage;

            //String strCurrentPage;

            uint8_t len = 20;
            char strCurrentPage[len];

            //Construct text
            if (currentPage == 0)
            {
                char bufRtcTemp[5];
                strcpy(strCurrentPage, dtostrf(rtcTemperature, 4, 1, bufRtcTemp));
                strcat(strCurrentPage, "\xB0");
                strcat(strCurrentPage, "C");
            }
            else if (currentPage == 1)
            {
                char bufHour[3];
                dtostrf(hourLocal, 1, 0, bufHour);
                strcpy(strCurrentPage, bufHour);

                //if (s_digit_2[0] % 2) { // do something odd
                if (blinkColon)
                {
                    if (state500ms == LOW)
                    {
                        strcat(strCurrentPage, " ");
                    }
                }
                else
                {
                    strcat(strCurrentPage, ":");
                }
                if (minLocal < 10)
                {
                    strcat(strCurrentPage, "0");
                }
                //strCurrentPage += minLocal;
                char buffer[3];
                strcat(strCurrentPage, itoa(minLocal, buffer, 10));
            }
            else if (currentPage == 2)
            {
                //strCurrentPage += bufWatt;
                strcat(strCurrentPage, bufWatt);
            }

            static int x_offset_currentPage = 0;
            static int y_offset_currentPage = 0;

            // ------------------------
            // IF NO ANIMATION
            // ------------------------
            static boolean animation = 0;

            if (animation == 0)
            {
                x_offset_currentPage = 0;
                y_offset_currentPage = 0;
                static unsigned int count;
                count++;
                unsigned int waitingTime = 0;
                if (currentPage == 0)
                {
                    waitingTime = 80 * 4;
                }
                else if (currentPage == 1)
                {
                    //waitingTime = 80 * 30;
                    waitingTime = 80 * 30;
                }
                else if (currentPage == 2)
                {
                    //waitingTime = 80 * 30;
                    waitingTime = 80 * 2;
                }
                if (count == waitingTime)
                {
                    //reset count
                    count = 0;
                    animation = 1;
                    nextPageAnimation = random(0, 3 + 1);
                    exitAnimation = random(0, 3 + 1);
                    //exitAnimation = nextPageAnimation;
                }

                int16_t x0_currentPage;
                int16_t y0_currentPage;

                if (currentPage == 0)
                {
                    PreparePage(strCurrentPage, &F14LED7pt8b, &x0_currentPage, &y0_currentPage);
                }
                else if (currentPage == 1)
                {
                    PreparePage(strCurrentPage, &UbuntuMono_B10pt8b, &x0_currentPage, &y0_currentPage);
                }
                if (currentPage == 2)
                {
                    PreparePage(strCurrentPage, &F14LED7pt8b, &x0_currentPage, &y0_currentPage);
                }

                matrix.setCursor(x0_currentPage + x_offset_currentPage, y0_currentPage + y_offset_currentPage);
                matrix.print(strCurrentPage);
            }

            // ------------------------
            // IF ANIMATION TURNED ON
            // ------------------------

            else if (animation == 1)
            {

                // ------------------------
                // CURRENT PAGE, ANIMATION TURNED ON
                // ------------------------

                int16_t max_offset_currentPage;
                int16_t currOffsetCurrentPage;

                //SLIDE OUT RIGHT
                if (exitAnimation == 0)
                {
                    x_offset_currentPage++;
                    currOffsetCurrentPage = x_offset_currentPage;
                    y_offset_currentPage = 0;
                    max_offset_currentPage = matrix.width();
                }
                //SLIDE OUT LEFT
                else if (exitAnimation == 1)
                {
                    x_offset_currentPage--;
                    currOffsetCurrentPage = x_offset_currentPage;
                    y_offset_currentPage = 0;
                    max_offset_currentPage = -matrix.width();
                }
                //SLIDE OUT UP
                else if (exitAnimation == 2)
                {
                    x_offset_currentPage = 0;
                    y_offset_currentPage--;
                    currOffsetCurrentPage = y_offset_currentPage;
                    max_offset_currentPage = -matrix.height();
                }
                //SLIDE OUT DOWN
                else if (exitAnimation == 3)
                {
                    x_offset_currentPage = 0;
                    y_offset_currentPage++;
                    currOffsetCurrentPage = y_offset_currentPage;
                    max_offset_currentPage = matrix.height();
                }
                //SLIDE OUT RIGHT UP
                else if (exitAnimation == 4)
                {
                    x_offset_currentPage++;
                    y_offset_currentPage--;
                    currOffsetCurrentPage = y_offset_currentPage;
                    max_offset_currentPage = matrix.height();
                }
                //SLIDE OUT LEFT UP
                else if (exitAnimation == 5)
                {
                    x_offset_currentPage--;
                    y_offset_currentPage--;
                    currOffsetCurrentPage = y_offset_currentPage;
                    max_offset_currentPage = matrix.height();
                }
                //SLIDE OUT RIGHT DOWN
                else if (exitAnimation == 6)
                {
                    x_offset_currentPage++;
                    y_offset_currentPage++;
                    currOffsetCurrentPage = y_offset_currentPage;
                    max_offset_currentPage = matrix.height();
                }
                //SLIDE OUT LEFT DOWN
                else if (exitAnimation == 7)
                {
                    x_offset_currentPage--;
                    y_offset_currentPage++;
                    currOffsetCurrentPage = y_offset_currentPage;
                    max_offset_currentPage = matrix.height();
                }

                int16_t x0_currentPage;
                int16_t y0_currentPage;
                if (currentPage == 0)
                {
                    PreparePage(strCurrentPage, &F14LED7pt8b, &x0_currentPage, &y0_currentPage);
                }
                else if (currentPage == 1)
                {
                    PreparePage(strCurrentPage, &UbuntuMono_B10pt8b, &x0_currentPage, &y0_currentPage);
                }
                else if (currentPage == 2)
                {
                    PreparePage(strCurrentPage, &F14LED7pt8b, &x0_currentPage, &y0_currentPage);
                }

                //if (x_offset_currentPage != max_offset_currentPage) {
                matrix.setCursor(x0_currentPage + x_offset_currentPage, y0_currentPage + y_offset_currentPage);
                matrix.print(strCurrentPage);
                //}

                // ------------------------
                // NEXT PAGE, ANIMATION TURNED ON
                // ------------------------

                uint8_t nextPage = currentPage + 1;
                if (nextPage > pageMax)
                {
                    nextPage = 0;
                }

                //String strNextPage;
                //strNextPage = "";

                char strNextPage[len];

                //Construct text for Page 0
                if (nextPage == 0)
                {
                    char bufRtcTemp[5];
                    strcpy(strNextPage, dtostrf(rtcTemperature, 4, 1, bufRtcTemp));
                    strcat(strNextPage, "\xB0");
                    strcat(strNextPage, "C");
                }
                else if (nextPage == 1)
                {
                    char bufHour[2];
                    dtostrf(hourLocal, 1, 0, bufHour);
                    strcpy(strNextPage, bufHour);

                    //if (s_digit_2[0] % 2) { // do something odd
                    if (blinkColon)
                    {
                        if (state500ms)
                        {
                            strcat(strNextPage, " ");
                        }
                    }
                    else
                    {
                        strcat(strNextPage, ":");
                    }

                    if (minLocal < 10)
                    {
                        strcat(strNextPage, "0");
                    }
                    char buffer[3];
                    strcat(strNextPage, itoa(minLocal, buffer, 10));
                }
                else if (nextPage == 2)
                {
                    strcpy(strNextPage, bufWatt);
                    strcat(strNextPage, " W");
                }

                //NEXT PAGE PARAMETER
                int16_t x_center, y_center;
                static int16_t x_offset_nextPage = 0;
                static int16_t y_offset_nextPage = 0;
                int16_t currOffsetNextPage = 0;
                int16_t max_offset_nextPage;

                int16_t x0_nextPage = 0;
                int16_t y0_nextPage = 0;

                //SLIDE IN RIGHT
                if (nextPageAnimation == 0)
                {

                    x_offset_nextPage++;
                    currOffsetNextPage = x_offset_nextPage;
                    y_offset_nextPage = 0;
                    max_offset_nextPage = matrix.width();

                    if (nextPage == 0)
                    {
                        PreparePage(strNextPage, &F14LED7pt8b, &x_center, &y_center);
                    }
                    else if (nextPage == 1)
                    {
                        PreparePage(strNextPage, &UbuntuMono_B10pt8b, &x_center, &y_center);
                    }
                    else if (nextPage == 2)
                    {
                        PreparePage(strNextPage, &F14LED7pt8b, &x_center, &y_center);
                    }

                    x0_nextPage = x_center + x_offset_nextPage - matrix.width();
                    y0_nextPage = y_center + y_offset_nextPage;
                }
                //SLIDE IN LEFT
                else if (nextPageAnimation == 1)
                {

                    x_offset_nextPage--;
                    currOffsetNextPage = x_offset_nextPage;
                    y_offset_nextPage = 0;
                    max_offset_nextPage = -matrix.width();

                    if (nextPage == 0)
                    {
                        PreparePage(strNextPage, &F14LED7pt8b, &x_center, &y_center);
                    }
                    else if (nextPage == 1)
                    {
                        PreparePage(strNextPage, &UbuntuMono_B10pt8b, &x_center, &y_center);
                    }
                    else if (nextPage == 2)
                    {
                        PreparePage(strNextPage, &F14LED7pt8b, &x_center, &y_center);
                    }

                    x0_nextPage = x_center + x_offset_nextPage + matrix.width();
                    y0_nextPage = y_center + y_offset_nextPage;
                }
                //SLIDE IN UP
                else if (nextPageAnimation == 2)
                {

                    x_offset_nextPage = 0;
                    y_offset_nextPage--;
                    currOffsetNextPage = y_offset_nextPage;
                    max_offset_nextPage = -matrix.height();

                    if (nextPage == 0)
                    {
                        PreparePage(strNextPage, &F14LED7pt8b, &x_center, &y_center);
                    }
                    else if (nextPage == 1)
                    {
                        PreparePage(strNextPage, &UbuntuMono_B10pt8b, &x_center, &y_center);
                    }
                    else if (nextPage == 2)
                    {
                        PreparePage(strNextPage, &F14LED7pt8b, &x_center, &y_center);
                    }

                    x0_nextPage = x_center + x_offset_nextPage;
                    y0_nextPage = y_center + y_offset_nextPage + matrix.height();
                }
                //SLIDE IN DOWN
                else if (nextPageAnimation == 3)
                {

                    x_offset_nextPage = 0;
                    y_offset_nextPage++;
                    currOffsetNextPage = y_offset_nextPage;
                    max_offset_nextPage = matrix.height();

                    if (nextPage == 0)
                    {
                        PreparePage(strNextPage, &F14LED7pt8b, &x_center, &y_center);
                    }
                    else if (nextPage == 1)
                    {
                        PreparePage(strNextPage, &UbuntuMono_B10pt8b, &x_center, &y_center);
                    }
                    else if (nextPage == 2)
                    {
                        PreparePage(strNextPage, &F14LED7pt8b, &x_center, &y_center);
                    }

                    x0_nextPage = x_center + x_offset_nextPage;
                    y0_nextPage = y_center + y_offset_nextPage - matrix.height();
                }
                //SLIDE IN RIGHT UP
                else if (nextPageAnimation == 4)
                {

                    x_offset_nextPage++;
                    y_offset_nextPage--;
                    currOffsetNextPage = y_offset_nextPage;
                    max_offset_nextPage = -matrix.height();

                    if (nextPage == 0)
                    {
                        PreparePage(strNextPage, &F14LED7pt8b, &x_center, &y_center);
                    }
                    else if (nextPage == 1)
                    {
                        PreparePage(strNextPage, &UbuntuMono_B10pt8b, &x_center, &y_center);
                    }
                    else if (nextPage == 2)
                    {
                        PreparePage(strNextPage, &F14LED7pt8b, &x_center, &y_center);
                    }

                    x0_nextPage = x_center + x_offset_nextPage - matrix.height();
                    y0_nextPage = y_center + y_offset_nextPage + matrix.height();
                }
                //SLIDE IN LEFT UP
                else if (nextPageAnimation == 5)
                {

                    x_offset_nextPage--;
                    y_offset_nextPage--;
                    currOffsetNextPage = y_offset_nextPage;
                    max_offset_nextPage = -matrix.height();

                    if (nextPage == 0)
                    {
                        PreparePage(strNextPage, &F14LED7pt8b, &x_center, &y_center);
                    }
                    else if (nextPage == 1)
                    {
                        PreparePage(strNextPage, &UbuntuMono_B10pt8b, &x_center, &y_center);
                    }
                    else if (nextPage == 2)
                    {
                        PreparePage(strNextPage, &F14LED7pt8b, &x_center, &y_center);
                    }

                    x0_nextPage = x_center + x_offset_nextPage + matrix.height();
                    y0_nextPage = y_center + y_offset_nextPage + matrix.height();
                }
                //SLIDE IN RIGHT DOWN
                else if (nextPageAnimation == 6)
                {

                    x_offset_nextPage++;
                    y_offset_nextPage++;
                    currOffsetNextPage = y_offset_nextPage;
                    max_offset_nextPage = matrix.height();

                    if (nextPage == 0)
                    {
                        PreparePage(strNextPage, &F14LED7pt8b, &x_center, &y_center);
                    }
                    else if (nextPage == 1)
                    {
                        PreparePage(strNextPage, &UbuntuMono_B10pt8b, &x_center, &y_center);
                    }
                    else if (nextPage == 2)
                    {
                        PreparePage(strNextPage, &F14LED7pt8b, &x_center, &y_center);
                    }

                    x0_nextPage = x_center + x_offset_nextPage - matrix.height();
                    y0_nextPage = y_center + y_offset_nextPage - matrix.height();
                }
                //SLIDE IN LEFT DOWN
                else if (nextPageAnimation == 7)
                {

                    x_offset_nextPage--;
                    y_offset_nextPage++;
                    currOffsetNextPage = y_offset_nextPage;
                    max_offset_nextPage = matrix.height();

                    if (nextPage == 0)
                    {
                        PreparePage(strNextPage, &F14LED7pt8b, &x_center, &y_center);
                    }
                    else if (nextPage == 1)
                    {
                        PreparePage(strNextPage, &UbuntuMono_B10pt8b, &x_center, &y_center);
                    }
                    else if (nextPage == 2)
                    {
                        PreparePage(strNextPage, &F14LED7pt8b, &x_center, &y_center);
                    }

                    x0_nextPage = x_center + x_offset_nextPage + matrix.height();
                    y0_nextPage = y_center + y_offset_nextPage - matrix.height();
                }

                if (abs(max_offset_nextPage) == abs(max_offset_currentPage))
                {
                    if (max_offset_nextPage == -max_offset_currentPage)
                    {
                        if (abs(currOffsetCurrentPage) <= abs(max_offset_currentPage))
                        {
                            x_offset_nextPage = 0;
                            y_offset_nextPage = 0;
                        }
                        if (currOffsetNextPage == max_offset_nextPage)
                        {
                            x_offset_nextPage = 0;
                            y_offset_nextPage = 0;
                            animation = 0;
                            currentPage++;
                            if (currentPage > pageMax)
                            {
                                currentPage = 0;
                            }
                        }
                    }
                    else if (currOffsetNextPage == max_offset_nextPage)
                    {
                        x_offset_nextPage = 0;
                        y_offset_nextPage = 0;
                        animation = 0;
                        currentPage++;
                        if (currentPage > pageMax)
                        {
                            currentPage = 0;
                        }
                    }
                }
                else if (abs(max_offset_nextPage) > abs(max_offset_currentPage))
                {
                    if (currOffsetNextPage == max_offset_nextPage)
                    {
                        x_offset_nextPage = 0;
                        y_offset_nextPage = 0;
                        animation = 0;
                        currentPage++;
                        if (currentPage > pageMax)
                        {
                            currentPage = 0;
                        }
                    }
                }
                else if (abs(max_offset_nextPage) < abs(max_offset_currentPage))
                {
                    if (abs(currOffsetCurrentPage) <= abs(max_offset_currentPage) - abs(max_offset_nextPage))
                    {
                        x_offset_nextPage = 0;
                        y_offset_nextPage = 0;
                    }
                    if (currOffsetNextPage == max_offset_nextPage)
                    {
                        x_offset_nextPage = 0;
                        y_offset_nextPage = 0;
                        animation = 0;
                        currentPage++;
                        if (currentPage > pageMax)
                        {
                            currentPage = 0;
                        }
                    }
                }
                else if (currOffsetNextPage == max_offset_nextPage)
                {
                    x_offset_nextPage = 0;
                    y_offset_nextPage = 0;
                    animation = 0;
                    currentPage++;
                    if (currentPage > pageMax)
                    {
                        currentPage = 0;
                    }
                }
                else
                {
                    Serial.println(F("tidak ketangkap"));
                }

                matrix.setCursor(x0_nextPage, y0_nextPage);
                matrix.print(strNextPage);
            }

            //    //draw page indicator (rectangle)
            //    if (page == 0) {
            //        matrix.drawRect(28, 0, 2, 2, 1);
            //        matrix.drawLine(34, 1, 35, 1, 1);
            //    }
            //    else if (page == 1) {
            //        matrix.drawLine(28, 1, 29, 1, 1);
            //        matrix.drawRect(34, 0, 2, 2, 1);
            //    }
        }

        // -------------------------------------------------------------------
        // MODE 0, PAGE 7, SANDBOX
        // -------------------------------------------------------------------
        else if (currentPageMode0 == 7)
        {

            matrix.setTextWrap(0);

            int16_t x1Temp, y1Temp;
            uint16_t w1Temp, h1Temp;

            matrix.clearDisplay();

            matrix.setTextSize(1);

            const GFXfont *font;
            //font = &FiveBySeven5pt7b;
            font = &F14LED7pt8b;
            matrix.setFont(font);

            char buf[15];
            const char *ptr = PSTR("iqbaL");

            strcpy_P(buf, ptr);

            matrix.getTextBounds(buf, 0, 0, &x1Temp, &y1Temp, &w1Temp, &h1Temp);

            uint16_t x_start = (matrix.width() - w1Temp) / 2;
            uint16_t y_start = (matrix.height() - h1Temp) / 2 + h1Temp - 2;

            matrix.getTextBounds(buf, x_start, y_start, &x1Temp, &y1Temp, &w1Temp, &h1Temp);

            //    Serial.print(x1Temp);
            //    Serial.print(",");
            //    Serial.print(y1Temp);
            //    Serial.print(",");
            //    Serial.print(w1Temp);
            //    Serial.print(",");
            //    Serial.print(h1Temp);
            //    Serial.println();

            matrix.setCursor(x_start, y_start);
            matrix.print(buf);

            boolean effect0 = 1;

            if (effect0 == 1)
            {

                int16_t x0 = x1Temp;
                int16_t y0 = y1Temp;
                int16_t x1 = x1Temp;

                int16_t y1 = y1Temp;
                uint16_t w = w1Temp;
                uint16_t h = h1Temp;

                //      x0 = 0;
                //      x1 = 0;
                //      w = 48;

                static uint8_t animation = 0;

                uint16_t buf[w * h];

                matrix.copyBuffer(x0, y0, w, h, buf);

                //laser-in-left
                if (animation == 0)
                {

                    static int16_t offset = 0;
                    static uint16_t offsetMax;
                    static int16_t xTemp = 0;
                    uint16_t index = 0;

                    matrix.fillRect(x0 + xTemp, y0, w, h, 0);

                    offsetMax = w - xTemp;
                    uint16_t sumBuf = 0;

                    for (uint16_t yTemp = 0; yTemp < h; yTemp++)
                    {
                        matrix.drawPixel(x1 + w - offset, y1 + yTemp, buf[xTemp + index]);
                        //detect empty spaces
                        sumBuf = sumBuf + buf[xTemp + index];
                        index = index + w;
                    }

                    //skip when found empty spaces
                    if (sumBuf == 0)
                    {
                        xTemp++;
                    }

                    offset++;

                    //reset offset and change animation
                    if (offset > offsetMax)
                    {
                        offset = 0;
                        xTemp++;
                        if (xTemp == w)
                        {
                            xTemp = 0;
                            if (animation == 0)
                            {
                                animation = 1;
                            }
                        }
                    }
                }

                //laser-out-left
                else if (animation == 1)
                {

                    static int16_t offset = 0;
                    static uint16_t offsetMax;
                    static int16_t xTemp = 0;
                    uint16_t index = 0;

                    matrix.fillRect(x0, y0, xTemp + 1, h, 0);

                    offsetMax = xTemp;
                    uint16_t sumBuf = 0;

                    for (uint16_t yTemp = 0; yTemp < h; yTemp++)
                    {

                        matrix.drawPixel(x1 + xTemp - offset, y1 + yTemp, buf[xTemp + index]);
                        sumBuf = sumBuf + buf[xTemp + index];
                        index = index + w;
                    }

                    if (sumBuf == 0)
                    {
                        xTemp++;
                    }

                    offset++;

                    //reset offset and change animation
                    if (offset > offsetMax)
                    {
                        offset = 0;
                        xTemp++;
                        if (xTemp == w)
                        {
                            xTemp = 0;
                            if (animation == 1)
                            {
                                animation = 0;
                            }
                        }
                    }
                }
            }
        }

        // -------------------------------------------------------------------
        // MODE 0, PAGE 8, JAM / MENIT MENUJU SHOLAT TYPE 2
        // -------------------------------------------------------------------
        else if (currentPageMode0 == 8)
        {

            //if (!matrix.isReversed()) {
            //  matrix.reverse();
            //}
            matrix.setTextWrap(0);

            matrix.clearDisplay();

            matrix.setTextSize(1);

            const GFXfont *font;
            //font = &FiveBySeven5pt7b;
            font = &UbuntuMono_B10pt8b;
            //font = &F14LED7pt8b;
            matrix.setFont(font);

            int16_t xTemp, yTemp;
            uint16_t wTemp, hTemp;

            char contents[2] = "8";

            matrix.getTextBounds(contents, 0, 0, &xTemp, &yTemp, &wTemp, &hTemp);

            //prepare array for time left sholat
            char bufTimeLeftSholat[6];

            //format (add leading '0' if  less than 10) and store in the array
            sprintf(bufTimeLeftSholat, "%02d%02d%02d", HOUR, MINUTE, SECOND);

            int16_t x0 = 2;
            int16_t y0 = 13;

            if (bufTimeLeftSholat[1] != '0')
            {
                x0 = 2;
            }
            else if (bufTimeLeftSholat[1] == '0' && bufTimeLeftSholat[2] != '0')
            {
                x0 = 8;
            }
            else if (bufTimeLeftSholat[1] == '0' && bufTimeLeftSholat[2] == '0')
            {
                x0 = 13;
            }

            // print dash sign
            int dashLength = 7;
            if (state500ms)
            {
                matrix.fillRect(x0, 7, dashLength, 2, 1);
            }

            x0 = x0 + dashLength + 1;

            // print hour digit2
            if (bufTimeLeftSholat[1] != '0')
            {
                matrix.setCursor(x0, y0);
                matrix.print(bufTimeLeftSholat[1]);

                //print Colon
                x0 = x0 + wTemp + 2;
                matrix.fillRect(x0, y0 - 3, 2, 2, 1);
                matrix.fillRect(x0, y0 - 9, 2, 2, 1);
                x0 = x0 + 2;
            }

            //print minute digit 1
            if (bufTimeLeftSholat[2] != '0' || (bufTimeLeftSholat[2] == '0' && bufTimeLeftSholat[1] != '0'))
            {
                matrix.setCursor(x0, y0);
                matrix.print(bufTimeLeftSholat[2]);
                x0 = x0 + wTemp + 1;
            }

            // print minute digit 2
            matrix.setCursor(x0, y0);
            matrix.print(bufTimeLeftSholat[3]);

            //print Colon
            x0 = x0 + wTemp + 2;
            matrix.fillRect(x0, y0 - 3, 2, 2, 1);
            matrix.fillRect(x0, y0 - 9, 2, 2, 1);

            // print second digit 1
            x0 = x0 + 2;
            matrix.setCursor(x0, y0);
            matrix.print(bufTimeLeftSholat[4]);

            // print second digit 2
            x0 = x0 + wTemp + 1;
            matrix.setCursor(x0, y0);
            matrix.print(bufTimeLeftSholat[5]);
        }

        // -------------------------------------------------------------------
        // MODE 0, PAGE 9, JAM SAJA TYPE 2
        // -------------------------------------------------------------------
        else if (currentPageMode0 == 9)
        {
            byte typePage9 = 1;

            if (typePage9 == 0)
            {
                matrix.setTextWrap(0);

                matrix.clearDisplay();

                matrix.setTextSize(1);

                const GFXfont *font;
                //font = &FiveBySeven5pt7b;
                font = &UbuntuMono_B10pt8b;
                //font = &F14LED7pt8b;
                matrix.setFont(font);

                // calculate font width
                int16_t xTemp, yTemp;
                uint16_t wTemp, hTemp;
                char contents[2] = "8";
                matrix.getTextBounds(contents, 0, 0, &xTemp, &yTemp, &wTemp, &hTemp);

                //array for time Now
                char bufNow[6];

                uint8_t hour12;
                if (hourLocal > 12)
                {
                    hour12 = hourLocal - 12;
                }
                else
                {
                    hour12 = hourLocal;
                }
                //format (add leading '0' if  less than 10) and store in the array
                sprintf(bufNow, "%02d%02d%02d", hour12, minLocal, secLocal);

                int16_t x0;
                int16_t y0;

                // set starting row row based on font type
                if (font == &UbuntuMono_B10pt8b)
                {
                    y0 = 13;
                }

                //determine time Format 12-hour or 24-hour
                // 0 = 24-hour
                // 1 = 12-hour
                byte timeFormat = 1;

                // determine starting column
                if (bufNow[0] != '0')
                {
                    if (timeFormat == 0)
                    {
                        x0 = 0;
                    }
                    else if (timeFormat == 1)
                    {
                        x0 = -1;
                    }
                }
                else if (bufNow[0] == '0')
                {
                    x0 = 4;
                }

                // print hour digit 1
                if (bufNow[0] != '0')
                {
                    matrix.setCursor(x0, y0);
                    matrix.print(bufNow[0]);
                    x0 = x0 + wTemp + 1;
                }

                // print hour digit2
                matrix.setCursor(x0, y0);
                matrix.print(bufNow[1]);

                //print Colon
                x0 = x0 + wTemp + 2;
                matrix.fillRect(x0, y0 - 2, 4, 2, 1);
                matrix.fillRect(x0 + 1, y0 - 3, 2, 4, 1);
                matrix.fillRect(x0, y0 - 8, 4, 2, 1);
                matrix.fillRect(x0 + 1, y0 - 9, 2, 4, 1);

                //print minute digit 1
                x0 = x0 + 4;
                matrix.setCursor(x0, y0);
                matrix.print(bufNow[2]);
                //matrix.print(":");
                x0 = x0 + wTemp + 1;

                // print minute digit 2
                matrix.setCursor(x0, y0);
                matrix.print(bufNow[3]);

                //print Colon
                x0 = x0 + wTemp + 2;
                matrix.fillRect(x0, y0 - 2, 4, 2, 1);
                matrix.fillRect(x0 + 1, y0 - 3, 2, 4, 1);
                matrix.fillRect(x0, y0 - 8, 4, 2, 1);
                matrix.fillRect(x0 + 1, y0 - 9, 2, 4, 1);

                // print second digit 1
                x0 = x0 + 4;
                matrix.setCursor(x0, y0);
                matrix.print(bufNow[4]);

                // print second digit 2
                x0 = x0 + wTemp + 1;
                matrix.setCursor(x0, y0);
                matrix.print(bufNow[5]);
            }

            else if (typePage9 == 1)
            {
                matrix.setTextWrap(0);

                matrix.clearDisplay();

                matrix.setTextSize(1);

                const GFXfont *font;
                //font = &FiveBySeven5pt7b;
                font = &UbuntuMono_B10pt8b;
                //font = &F14LED7pt8b;
                matrix.setFont(font);

                // calculate font width
                int16_t xTemp, yTemp;
                uint16_t wTemp, hTemp;
                char contents[2] = "8";
                matrix.getTextBounds(contents, 0, 0, &xTemp, &yTemp, &wTemp, &hTemp);

                //array for time Now
                char bufNow[6];

                uint8_t hour12;
                if (hourLocal > 12)
                {
                    hour12 = hourLocal - 12;
                }
                else
                {
                    hour12 = hourLocal;
                }
                //format (add leading '0' if  less than 10) and store in the array
                sprintf(bufNow, "%02d%02d%02d", hour12, minLocal, secLocal);

                int16_t x0;
                int16_t y0;

                // set starting row row based on font type
                if (font == &UbuntuMono_B10pt8b)
                {
                    y0 = 13;
                }

                //determine time Format 12-hour or 24-hour
                // 0 = 24-hour
                // 1 = 12-hour
                byte timeFormat = 1;

                // determine starting column
                if (bufNow[0] != '0')
                {
                    if (timeFormat == 0)
                    {
                        x0 = 0;
                    }
                    else if (timeFormat == 1)
                    {
                        x0 = -1;
                    }
                }
                else if (bufNow[0] == '0')
                {
                    x0 = 4;
                }

                int x0_hour_digit_1 = x0;

                // print hour digit 1
                if (bufNow[0] != '0')
                {
                    matrix.setCursor(x0, y0);
                    matrix.print(bufNow[0]);
                    x0 = x0 + wTemp + 1;
                }

                int x0_hour_digit_2 = x0;

                // print hour digit2
                matrix.setCursor(x0, y0);
                matrix.print(bufNow[1]);

                //print Colon
                x0 = x0 + wTemp + 2;
                matrix.fillRect(x0, y0 - 2, 4, 2, 1);
                matrix.fillRect(x0 + 1, y0 - 3, 2, 4, 1);
                matrix.fillRect(x0, y0 - 8, 4, 2, 1);
                matrix.fillRect(x0 + 1, y0 - 9, 2, 4, 1);

                //print minute digit 1
                x0 = x0 + 4;
                int x0_minute_digit_1 = x0;
                matrix.setCursor(x0, y0);
                matrix.print(bufNow[2]);
                //matrix.print(":");
                x0 = x0 + wTemp + 1;
                int x0_minute_digit_2 = x0;

                // print minute digit 2
                matrix.setCursor(x0, y0);
                matrix.print(bufNow[3]);

                //print Colon
                x0 = x0 + wTemp + 2;
                matrix.fillRect(x0, y0 - 2, 4, 2, 1);
                matrix.fillRect(x0 + 1, y0 - 3, 2, 4, 1);
                matrix.fillRect(x0, y0 - 8, 4, 2, 1);
                matrix.fillRect(x0 + 1, y0 - 9, 2, 4, 1);

                // print second digit 1
                x0 = x0 + 4;
                int x0_second_digit_1 = x0;
                matrix.setCursor(x0, y0);
                matrix.print(bufNow[4]);

                // print second digit 2
                x0 = x0 + wTemp + 1;
                int x0_second_digit_2 = x0;
                matrix.setCursor(x0, y0);
                matrix.print(bufNow[5]);

                //array for 1 second ahead
                static char bufTimeFuture[6];

                //counter to determine when to trigger the animation
                int sec;
                static int second_old = 0;
                sec = bufNow[5];
                // static unsigned long currMillis;
                static uint16_t count = 0;
                if (sec != second_old)
                {
                    second_old = sec;
                    // currMillis = millis();
                    count = 0;
                }

                static boolean animateSecDigit1;
                static boolean animateSecDigit2;
                static boolean animateMinDigit1;
                static boolean animateMinDigit2;
                static boolean animateHourDigit1;
                static boolean animateHourDigit2;

                if (count == 1000 / _ledMatrixSettings.scrollspeed - 25)
                {
                    //break time element
                    uint32_t t_future = localTime + 1;
                    RtcDateTime dt;
                    dt.InitWithEpoch32Time(t_future);

                    //format (add leading '0' if less than 10) and store in the array
                    sprintf(bufTimeFuture, "%02d%02d%02d", dt.Hour(), dt.Minute(), dt.Second());

                    //animateSecDigit2 = true;

                    if (bufNow[5] == '9')
                    {
                        //animateSecDigit1 = true;
                        if (bufNow[4] == '5')
                        {
                            animateMinDigit2 = true;
                            if (bufNow[3] == '9')
                            {
                                animateMinDigit1 = true;
                                if (bufNow[2] == '5')
                                {
                                    animateHourDigit2 = true;
                                    if (bufNow[1] == '3' || bufNow[1] == '9')
                                    {
                                        animateHourDigit1 = true;
                                    }
                                }
                            }
                        }
                    }
                }

                if (count == 1000 / _ledMatrixSettings.scrollspeed - 25)
                {

                    animateSecDigit2 = true;

                    if (bufNow[5] == '9')
                    {
                        animateSecDigit1 = true;
                    }
                }

                count++;

                uint16_t w = 9;
                uint16_t h = 12;

                if (animateSecDigit2)
                {
                    static int animation = 0;

                    //slide-out-down
                    if (animation == 0)
                    {
                        //boolean slide_in_down (char chr, const GFXfont *font, int16_t x0, int16_t y0, uint16_t w, uint16_t h);
                        boolean slideOutDown = slide_out_down_sec_digit_2(bufNow[5], font, x0_second_digit_2, y0, w, h);
                        if (slideOutDown == true)
                        {
                            animation = 1;
                        }
                    }

                    //slide-in-down
                    if (animation == 1)
                    {
                        //boolean slide_in_down (char chr, const GFXfont *font, int16_t x0, int16_t y0, uint16_t w, uint16_t h);
                        boolean slideInDown = slide_in_down_sec_digit_2(bufTimeFuture[5], font, x0_second_digit_2, y0, w, h);
                        if (slideInDown == true)
                        {
                            animateSecDigit2 = false;
                            animation = 0;
                        }
                    }
                }

                if (animateSecDigit1)
                {
                    static int animation = 0;

                    //slide-out-down
                    if (animation == 0)
                    {
                        //boolean slide_in_down (char chr, const GFXfont *font, int16_t x0, int16_t y0, uint16_t w, uint16_t h);
                        boolean slideOutDown = slide_out_down_sec_digit_1(bufNow[4], font, x0_second_digit_1, y0, w, h);
                        if (slideOutDown == true)
                        {
                            animation = 1;
                        }
                    }

                    //slide-in-down
                    if (animation == 1)
                    {
                        //boolean slide_in_down (char chr, const GFXfont *font, int16_t x0, int16_t y0, uint16_t w, uint16_t h);
                        boolean slideInDown = slide_in_down_sec_digit_1(bufTimeFuture[4], font, x0_second_digit_1, y0, w, h);
                        if (slideInDown == true)
                        {
                            animateSecDigit1 = false;
                            animation = 0;
                        }
                    }
                }

                if (animateMinDigit2)
                {
                    static int animation = 0;

                    //slide-out-down
                    if (animation == 0)
                    {
                        //boolean slide_in_down (char chr, const GFXfont *font, int16_t x0, int16_t y0, uint16_t w, uint16_t h);
                        boolean slideOutDown = slide_out_down_min_digit_2(bufNow[3], font, x0_minute_digit_2, y0, w, h);
                        if (slideOutDown == true)
                        {
                            animation = 1;
                        }
                    }

                    //slide-in-down
                    if (animation == 1)
                    {
                        //boolean slide_in_down (char chr, const GFXfont *font, int16_t x0, int16_t y0, uint16_t w, uint16_t h);
                        boolean slideInDown = slide_in_down_min_digit_2(bufTimeFuture[3], font, x0_minute_digit_2, y0, w, h);
                        if (slideInDown == true)
                        {
                            animateMinDigit2 = false;
                            animation = 0;
                        }
                    }
                }

                if (animateMinDigit1)
                {
                    static int animation = 0;

                    //slide-out-down
                    if (animation == 0)
                    {
                        //boolean slide_in_down (char chr, const GFXfont *font, int16_t x0, int16_t y0, uint16_t w, uint16_t h);
                        boolean slideOutDown = slide_out_down_min_digit_1(bufNow[2], font, x0_minute_digit_1, y0, w, h);
                        if (slideOutDown == true)
                        {
                            animation = 1;
                        }
                    }

                    //slide-in-down
                    if (animation == 1)
                    {
                        //boolean slide_in_down (char chr, const GFXfont *font, int16_t x0, int16_t y0, uint16_t w, uint16_t h);
                        boolean slideInDown = slide_in_down_min_digit_1(bufTimeFuture[2], font, x0_minute_digit_1, y0, w, h);
                        if (slideInDown == true)
                        {
                            animateMinDigit1 = false;
                            animation = 0;
                        }
                    }
                }

                if (animateHourDigit2)
                {
                    static int animation = 0;

                    //slide-out-down
                    if (animation == 0)
                    {
                        //boolean slide_in_down (char chr, const GFXfont *font, int16_t x0, int16_t y0, uint16_t w, uint16_t h);
                        boolean slideOutDown = slide_out_down_hour_digit_2(bufNow[1], font, x0_hour_digit_2, y0, w, h);
                        if (slideOutDown == true)
                        {
                            animation = 1;
                        }
                    }

                    //slide-in-down
                    if (animation == 1)
                    {
                        //boolean slide_in_down (char chr, const GFXfont *font, int16_t x0, int16_t y0, uint16_t w, uint16_t h);
                        boolean slideInDown = slide_in_down_hour_digit_2(bufTimeFuture[1], font, x0_hour_digit_2, y0, w, h);
                        if (slideInDown == true)
                        {
                            animateHourDigit2 = false;
                            animation = 0;
                        }
                    }
                }

                if (animateHourDigit1)
                {
                    static int animation = 0;

                    //slide-out-down
                    if (animation == 0)
                    {
                        //boolean slide_in_down (char chr, const GFXfont *font, int16_t x0, int16_t y0, uint16_t w, uint16_t h);
                        boolean slideOutDown = slide_out_down_hour_digit_1(bufNow[0], font, x0_hour_digit_1, y0, w, h);
                        if (slideOutDown == true)
                        {
                            animation = 1;
                        }
                    }

                    //slide-in-down
                    if (animation == 1)
                    {
                        //boolean slide_in_down (char chr, const GFXfont *font, int16_t x0, int16_t y0, uint16_t w, uint16_t h);
                        boolean slideInDown = slide_in_down_hour_digit_1(bufTimeFuture[0], font, x0_hour_digit_1, y0, w, h);
                        if (slideInDown == true)
                        {
                            animateHourDigit1 = false;
                            animation = 0;
                        }
                    }
                }
            }
        }

        // -------------------------------------------------------------------
        // MODE 0, PAGE 10, SANDBOX 2
        // -------------------------------------------------------------------
        else if (currentPageMode0 == 10)
        {

            static int c;

            matrix.clearDisplay();

            matrix.setTextSize(1);

            const GFXfont *font = &F14LED7pt8b;
            matrix.setFont(font);

            if (font == &TomThumb)
            {
                matrix.setCursor(c, 14);
            }
            else if (font == &bold_led_board_7_regular4pt7b)
            {
                matrix.setCursor(c, 12);
            }
            else if (font == &ChessType9pt7b)
            {
                matrix.setCursor(c, 12);
            }
            else if (font == &FiveBySeven5pt7b)
            {
                matrix.setCursor(c, 14);
            }
            else if (font == &F14LED7pt8b)
            {
                matrix.setCursor(c, 12);
            }

            //ESPHTTPServer.load_running_text();
            matrix.print("A");

            static int count;

            if (count == 80)
            {
                count = 0;
                c++;
            }
            count++;

            if (c > 63)
            {
                c = 0;
            }
        }

        // -------------------------------------------------------------------
        // MODE 0, PAGE 11, IQAMAH TIME
        // -------------------------------------------------------------------
        else if (currentPageMode0 == 11)
        {
            //calculate time to iqamah
            uint32_t adzanEndTime = sholat.timestampSholatTimesToday[CURRENTTIMEID] + (60 * _ledMatrixSettings.adzanwaittime);
            uint32_t iqamahTime = adzanEndTime + (60 * _ledMatrixSettings.iqamahwaittime);

            // time_t iqamahTime = currentSholatTime + (60 * 7);

            uint32_t timeToIqamah = iqamahTime - now;

            // uint16_t days;
            uint8_t hours;
            uint8_t minutes;
            uint8_t seconds;

            // days = elapsedDays(iqamahTime);
            hours = numberOfHours(timeToIqamah);
            minutes = numberOfMinutes(timeToIqamah);
            seconds = numberOfSeconds(timeToIqamah);

            matrix.setTextWrap(0);

            matrix.clearDisplay();

            matrix.setTextSize(1);

            const GFXfont *font;
            //font = &FiveBySeven5pt7b;
            font = &UbuntuMono_B10pt8b;
            //font = &F14LED7pt8b;
            matrix.setFont(font);

            int16_t xTemp, yTemp;
            uint16_t wTemp, hTemp;

            char contents[2] = "8";

            matrix.getTextBounds(contents, 0, 0, &xTemp, &yTemp, &wTemp, &hTemp);

            //prepare array for time left sholat
            char bufTimeToIqamah[7];

            //format (add leading '0' if  less than 10) and store in the array
            sprintf(bufTimeToIqamah, "%02d%02d%02d", hours, minutes, seconds);

            int16_t x0 = 2;
            int16_t y0 = 13;

            if (bufTimeToIqamah[1] != '0')
            {
                x0 = 2;
            }
            else if (bufTimeToIqamah[1] == '0' && bufTimeToIqamah[2] != '0')
            {
                x0 = 8;
            }
            else if (bufTimeToIqamah[1] == '0' && bufTimeToIqamah[2] == '0')
            {
                x0 = 13;
            }

            // print dash sign
            int dashLength = 7;
            if (state500ms)
            {
                matrix.fillRect(x0, 7, dashLength, 2, 1);
            }

            x0 = x0 + dashLength + 1;

            // print hour digit2
            if (bufTimeToIqamah[1] != '0')
            {
                matrix.setCursor(x0, y0);
                matrix.print(bufTimeToIqamah[1]);

                //print Colon
                x0 = x0 + wTemp + 2;
                matrix.fillRect(x0, y0 - 3, 2, 2, 1);
                matrix.fillRect(x0, y0 - 9, 2, 2, 1);
                x0 = x0 + 2;
            }

            //print minute digit 1
            if (bufTimeToIqamah[2] != '0' || (bufTimeToIqamah[2] == '0' && bufTimeToIqamah[1] != '0'))
            {
                matrix.setCursor(x0, y0);
                matrix.print(bufTimeToIqamah[2]);
                x0 = x0 + wTemp + 1;
            }

            // print minute digit 2
            matrix.setCursor(x0, y0);
            matrix.print(bufTimeToIqamah[3]);

            //print Colon
            x0 = x0 + wTemp + 2;
            matrix.fillRect(x0, y0 - 3, 2, 2, 1);
            matrix.fillRect(x0, y0 - 9, 2, 2, 1);

            // print second digit 1
            x0 = x0 + 2;
            matrix.setCursor(x0, y0);
            matrix.print(bufTimeToIqamah[4]);

            // print second digit 2
            x0 = x0 + wTemp + 1;
            matrix.setCursor(x0, y0);
            matrix.print(bufTimeToIqamah[5]);
        }

        // -------------------------------------------------------------------
        // MODE 0, PAGE 12, JAM DAN TANGGAL TYPE 2
        // -------------------------------------------------------------------
        else if (currentPageMode0 == 12)
        {

            matrix.clearDisplay();

            matrix.setTextWrap(0);

            int c;
            int row0 = 6;
            // int row1 = 15;
            int startCol = 1;
            int x0 = X;
            // int x1 = X;

            if (_ledMatrixSettings.scrollrow_0 == false)
            {
                x0 = 0;
            }

            if (_ledMatrixSettings.scrollrow_1 == false)
            {
                // x1 = 0;
            }

            // -------------------------------------------------------------------
            // ROW 0
            // -------------------------------------------------------------------

            startCol = 15; //override start colum for Row0

            const GFXfont *fontRow0 = &Wide11x8;
            matrix.setFont(fontRow0);

            if (fontRow0 == &Wide11x8)
            {
                startCol = 9;
                row0 = 7;
            }
            else if (fontRow0 == &bold_led_board_7_regular4pt7b)
            {
                startCol = 15;
                row0 = 6;
            }

            //hour
            c = DISP_REFF + startCol + x0;
            matrix.setCursor(c, row0);

            char buf[2];
            sprintf(buf, "%02d", hourLocal);
            matrix.print(buf);

            //colon
            c = matrix.getCursorX() + 1;

            if (secLocal < 30)
            {
                matrix.fillRect(c, row0 - 1, 2, 2, 1);

                // if (blinkColon) {
                if (state1000ms)
                {
                    matrix.fillRect(c, row0 - 4, 2, 2, 1);
                }
                // }
            }
            else
            {
                matrix.fillRect(c, row0 - 4, 2, 2, 1);

                // if (blinkColon) {
                if (state1000ms)
                {
                    matrix.fillRect(c, row0 - 1, 2, 2, 1);
                }
                // }
            }

            //minute
            c = matrix.getCursorX() + 5;
            matrix.setCursor(c, row0);

            sprintf(buf, "%02d", minLocal);
            matrix.print(buf);

            // -------------------------------------------------------------------
            // ROW 1
            // -------------------------------------------------------------------
            matrix.setTextWrap(0);

            // int16_t x1Temp, y1Temp;
            // uint16_t wTemp, hTemp;

            // int c;
            // int row0 = 6;
            // int row1 = 14;
            startCol = 1;

            if (_ledMatrixSettings.scrollrow_0 == false)
            {
                c = DISP_REFF + startCol;
            }

            if (_ledMatrixSettings.scrollrow_1 == false)
            {
                c = DISP_REFF + startCol;
            }

            c = DISP_REFF + startCol + X;

            // matrix.clearDisplay();

            matrix.setTextSize(1);

            const GFXfont *font = &FiveBySeven5pt7b;
            matrix.setFont(font);

            if (font == &TomThumb)
            {
                matrix.setCursor(c, 14);
            }
            else if (font == &bold_led_board_7_regular4pt7b)
            {
                matrix.setCursor(c, 12);
            }
            else if (font == &ChessType9pt7b)
            {
                matrix.setCursor(c, 12);
            }
            else if (font == &RepetitionScrolling5pt8b)
            {
                matrix.setCursor(c, 14);
            }
            else if (font == &FiveBySeven5pt7b)
            {
                matrix.setCursor(c, 15);
            }
            else if (font == &F14LED7pt8b)
            {
                matrix.setCursor(c, 12);
            }

            load_running_text_2();

            // -------------------------------------------------------------------
            // Animation
            // -------------------------------------------------------------------
            boolean effect0 = 0;
            // boolean effect1 = 0;

            if (effect0 == 1)
            {
                static uint16_t scan = 0;
                //uint16_t scanMax = _ledMatrixSettings.scrollspeed / 7;
                uint16_t scanMax = _ledMatrixSettings.scrollspeed;
                if (scan < scanMax)
                {
                    scan++;
                }
                else if (scan >= scanMax)
                {
                    scan = 0;
                }

                //    int16_t x0 = 2;
                //    int16_t y0 = 3;
                //    int16_t x1 = 2;
                //    int16_t y1 = 3;
                //    uint16_t w = 14;
                //    uint16_t h = 10;
            }

            if (effect0 == 1)
            {
                int16_t x0 = 0;
                int16_t y0 = 0;
                int16_t x1 = 0;
                int16_t y1 = 0;
                uint16_t w = 64;
                uint16_t h = 16;

                static int16_t offset;
                static uint16_t offsetMax;

                static uint8_t animation = 6;

                uint16_t index;
                uint16_t buf[1024];

                matrix.copyBuffer(x0, y0, w, h, buf);
                matrix.fillRect(x0, y0, w, h, 0);

                for (uint16_t yTemp = 0; yTemp < h; yTemp++)
                {
                    for (uint16_t xTemp = 0; xTemp < w; xTemp++)
                    {

                        //slide-out-up
                        if (animation == 0)
                        {
                            offsetMax = h;
                            if (yTemp >= offset)
                            {
                                matrix.drawPixel(x1 + xTemp, y1 + yTemp - offset, buf[index]);
                            }
                            else
                            {
                                matrix.drawPixel(x1 + xTemp, y1 + yTemp - offset, 0);
                            }
                        }

                        //slide-out-down
                        else if (animation == 1)
                        {
                            offsetMax = h;
                            if (yTemp < h - offset)
                            {
                                matrix.drawPixel(x1 + xTemp, y1 + yTemp + offset, buf[index]);
                            }
                            else
                            {
                                matrix.drawPixel(x1 + xTemp, y1 + yTemp + offset, 0);
                            }
                        }

                        //slide-out-right
                        else if (animation == 2)
                        {
                            offsetMax = w;
                            if (xTemp < w - offset)
                            {
                                matrix.drawPixel(x1 + xTemp + offset, y1 + yTemp, buf[index]);
                            }
                            else
                            {
                                matrix.drawPixel(x1 + xTemp + offset, y1 + yTemp, 0);
                            }
                        }

                        //slide-out-to-left
                        else if (animation == 3)
                        {
                            offsetMax = w;
                            if (xTemp >= offset)
                            {
                                matrix.drawPixel(x1 + xTemp - offset, y1 + yTemp, buf[index]);
                            }
                            else
                            {
                                matrix.drawPixel(x1 + xTemp - offset, y1 + yTemp, 0);
                            }
                        }

                        //slide-in-right
                        else if (animation == 4)
                        {
                            offsetMax = w;
                            if (xTemp >= w - offset)
                            {
                                matrix.drawPixel(x1 + xTemp - w + offset, y1 + yTemp, buf[index]);
                            }
                            else
                            {
                                matrix.drawPixel(x1 + xTemp - w + offset, y1 + yTemp, 0);
                            }
                        }

                        //slide-in-up
                        else if (animation == 5)
                        {
                            offsetMax = h;
                            if (yTemp < offset)
                            {
                                matrix.drawPixel(x1 + xTemp, y1 + yTemp + h - offset, buf[index]);
                            }
                            else
                            {
                                matrix.drawPixel(x1 + xTemp, y1 + yTemp + h - offset, 0);
                            }
                        }

                        //slide-in-down
                        else if (animation == 6)
                        {
                            offsetMax = h;
                            if (yTemp >= h - offset)
                            {
                                matrix.drawPixel(x1 + xTemp, y1 + yTemp - h + offset, buf[index]);
                            }
                            else
                            {
                                matrix.drawPixel(x1 + xTemp, y1 + yTemp - h + offset, 0);
                            }
                        }

                        //slide-in-left
                        else if (animation == 7)
                        {
                            offsetMax = w;
                            if (xTemp < offset)
                            {
                                matrix.drawPixel(x1 + xTemp + w - offset, y1 + yTemp, buf[index]);
                            }
                            else
                            {
                                matrix.drawPixel(x1 + xTemp + w - offset, y1 + yTemp, 0);
                            }
                        }

                        index++;
                    }
                }

                offset++;

                //reset offset and change animation
                if (offset > offsetMax)
                {
                    offset = 0;
                    if (animation == 6)
                    {
                        animation = 3;
                    }
                    else if (animation == 3)
                    {
                        animation = 4;
                    }
                    else if (animation == 4)
                    {
                        animation = 1;
                    }
                    else if (animation == 1)
                    {
                        animation = 5;
                    }
                    else if (animation == 5)
                    {
                        animation = 2;
                    }
                    else if (animation == 2)
                    {
                        animation = 7;
                    }
                    else if (animation == 7)
                    {
                        animation = 0;
                    }
                    else if (animation == 0)
                    {
                        animation = 6;
                    }
                }
            }
        }
    }

    // ------------
    // MODE 1
    // ------------
    else if (MODE == 1)
    {
        if (MODE != MODE_old)
        {
            MODE_old = MODE;
            Serial.println(F("MODE 1 [Config Mode]"));
        }

        // static bool refresh;
        if (currentPageMode1_old != currentPageMode1)
        {
            currentPageMode1_old = currentPageMode1;
            Serial.print(F("CONFIG MODE, PAGE "));
            Serial.println(currentPageMode1);

            // refresh = true;
            matrix.clearDisplay();
        }

        // -------------------------------------------------------------------
        // MODE 1, PAGE 0
        // DEVICE NAME
        // -------------------------------------------------------------------
        if (currentPageMode1 == 0)
        {

            //clear Screen
            matrix.clearDisplay();

            // ------------------------
            // String "Name" at row 0
            // ------------------------

            char str[64];

            const char *ptr = PSTR("Host Name");

            strcpy_P(str, ptr);

            int16_t x0, y0;
            uint16_t w, h;

            matrix.setFont(&TomThumb);

            matrix.getTextBounds(str, 0, 0, &x0, &y0, &w, &h);

            uint16_t x1 = (matrix.width() - w) / 2 + 4;
            uint16_t y1 = (matrix.height() / 2 - h) / 2 + h;

            //update x1 and y1 value
            matrix.getTextBounds(str, x1, y1, &x0, &y0, &w, &h);

            //Print to led matrix
            matrix.setCursor(x1, y1);
            matrix.print(str);

            // ------------------------
            // Device name at row 1
            // ------------------------

            strlcpy(str, WiFi.hostname().c_str(), sizeof(str));

            matrix.setFont(&TomThumb);

            matrix.getTextBounds(str, 0, 0, &x0, &y0, &w, &h);

            x1 = (matrix.width() - w) / 2 + 4;
            y1 = (matrix.height() / 2 - h) / 2 + h + matrix.height() / 2;

            //update x1 and y1 value
            matrix.getTextBounds(str, x1, y1, &x0, &y0, &w, &h);

            //Print to led matrix
            matrix.setCursor(x1, y1);
            matrix.print(str);
        }

        // -------------------------------------------------------------------
        // MODE 1, PAGE 1
        // SSID
        // -------------------------------------------------------------------
        if (currentPageMode1 == 1)
        {

            //clear Screen
            matrix.clearDisplay();

            // ------------------------
            // String "SSID" at row 0
            // ------------------------

            char str[64];

            const char *ptr = PSTR("SSID");

            strcpy_P(str, ptr);

            int16_t x0, y0;
            uint16_t w, h;

            matrix.setFont(&TomThumb);

            matrix.getTextBounds(str, 0, 0, &x0, &y0, &w, &h);

            uint16_t x1 = (matrix.width() - w) / 2 + 4;
            uint16_t y1 = (matrix.height() / 2 - h) / 2 + h;

            //update x1 and y1 value
            matrix.getTextBounds(str, x1, y1, &x0, &y0, &w, &h);

            //Print to led matrix
            matrix.setCursor(x1, y1);
            matrix.print(str);

            // ------------------------
            // SSID name at row 1
            // ------------------------

            strlcpy(str, WiFi.SSID().c_str(), sizeof(str));

            matrix.setFont(&TomThumb);

            matrix.getTextBounds(str, 0, 0, &x0, &y0, &w, &h);

            x1 = (matrix.width() - w) / 2 + 4;
            y1 = (matrix.height() / 2 - h) / 2 + h + matrix.height() / 2 - 1;

            //update x1 and y1 value
            matrix.getTextBounds(str, x1, y1, &x0, &y0, &w, &h);

            //Print to led matrix
            matrix.setCursor(x1, y1);
            matrix.print(str);
        }

        // -------------------------------------------------------------------
        // MODE 1, PAGE 2
        // IP ADDRESS
        // -------------------------------------------------------------------
        if (currentPageMode1 == 2)
        {

            //clear Screen
            matrix.clearDisplay();

            // ------------------------
            // String "IP Address" at row 0
            // ------------------------

            char str[64];
            const char *ptr = PSTR("IP Address");
            strcpy_P(str, ptr);

            int16_t x0, y0;
            uint16_t w, h;

            matrix.setFont(&TomThumb);

            matrix.getTextBounds(str, 0, 0, &x0, &y0, &w, &h);

            uint16_t x1 = (matrix.width() - w) / 2 + 4;
            uint16_t y1 = (matrix.height() / 2 - h) / 2 + h;

            //update x1 and y1 value
            matrix.getTextBounds(str, x1, y1, &x0, &y0, &w, &h);

            //Print to led matrix
            matrix.setCursor(x1, y1);
            matrix.print(str);

            // ------------------------
            // Local IP Address at row 1
            // ------------------------

            strlcpy(str, WiFi.localIP().toString().c_str(), sizeof(str));

            matrix.setFont(&TomThumb);

            matrix.getTextBounds(str, 0, 0, &x0, &y0, &w, &h);

            x1 = (matrix.width() - w) / 2 + 4;
            y1 = (matrix.height() / 2 - h) / 2 + h + matrix.height() / 2;

            //update x1 and y1 value
            matrix.getTextBounds(str, x1, y1, &x0, &y0, &w, &h);

            //Print to led matrix
            matrix.setCursor(x1, y1);
            matrix.print(str);
        }

        // -------------------------------------------------------------------
        // MODE 1, PAGE 3
        // GATEWAY
        // -------------------------------------------------------------------
        if (currentPageMode1 == 3)
        {

            //clear Screen
            matrix.clearDisplay();

            // ------------------------
            // String "Gateway" at row 0
            // ------------------------

            char str[64];
            const char *ptr = PSTR("Gateway");
            strcpy_P(str, ptr);

            int16_t x0, y0;
            uint16_t w, h;

            matrix.setFont(&TomThumb);

            matrix.getTextBounds(str, 0, 0, &x0, &y0, &w, &h);

            uint16_t x1 = (matrix.width() - w) / 2 + 4;
            uint16_t y1 = (matrix.height() / 2 - h) / 2 + h - 1;

            //update x1 and y1 value
            matrix.getTextBounds(str, 0, 0, &x0, &y0, &w, &h);

            //Print to led matrix
            matrix.setCursor(x1, y1);
            matrix.print(str);

            // ------------------------
            // Gateway IP Address at row 1
            // ------------------------

            strlcpy(str, WiFi.gatewayIP().toString().c_str(), sizeof(str));

            matrix.setFont(&TomThumb);

            matrix.getTextBounds(str, 0, 0, &x0, &y0, &w, &h);

            x1 = (matrix.width() - w) / 2 + 4;
            y1 = (matrix.height() / 2 - h) / 2 + h + matrix.height() / 2;

            //update x1 and y1 value
            matrix.getTextBounds(str, 0, 0, &x0, &y0, &w, &h);

            //Print to led matrix
            matrix.setCursor(x1, y1);
            matrix.print(str);
        }

        // -------------------------------------------------------------------
        // MODE 1, PAGE 4
        // NETMASK
        // -------------------------------------------------------------------
        if (currentPageMode1 == 4)
        {

            //clear Screen
            matrix.clearDisplay();

            // ------------------------
            // String "Netmask" at row 0
            // ------------------------

            char str[64];
            const char *ptr = PSTR("Netmask");
            strcpy_P(str, ptr);

            int16_t x0, y0;
            uint16_t w, h;

            matrix.setFont(&TomThumb);

            matrix.getTextBounds(str, 0, 0, &x0, &y0, &w, &h);

            uint16_t x1 = (matrix.width() - w) / 2 + 4;
            uint16_t y1 = (matrix.height() / 2 - h) / 2 + h;

            //update x1 and y1 value
            matrix.getTextBounds(str, x1, y1, &x0, &y0, &w, &h);

            //Print to led matrix
            matrix.setCursor(x1, y1);
            matrix.print(str);

            // ------------------------
            // Netmask Address at row 1
            // ------------------------

            strlcpy(str, WiFi.subnetMask().toString().c_str(), sizeof(str));

            matrix.setFont(&TomThumb);

            matrix.getTextBounds(str, 0, 0, &x0, &y0, &w, &h);

            x1 = (matrix.width() - w) / 2 + 4;
            y1 = (matrix.height() / 2 - h) / 2 + h + matrix.height() / 2;

            //update x1 and y1 value
            matrix.getTextBounds(str, x1, y1, &x0, &y0, &w, &h);

            //Print to led matrix
            matrix.setCursor(x1, y1);
            matrix.print(str);
        }

        // -------------------------------------------------------------------
        // MODE 1, PAGE 5
        // MAC ADDRESS
        // -------------------------------------------------------------------
        if (currentPageMode1 == 5)
        {

            //clear Screen
            matrix.clearDisplay();

            // ------------------------
            // String "MAC Address" at row 0
            // ------------------------

            char str[64];
            const char *ptr = PSTR("MAC Address");
            strcpy_P(str, ptr);

            int16_t x0, y0;
            uint16_t w, h;

            matrix.setFont(&TomThumb);

            matrix.getTextBounds(str, 0, 0, &x0, &y0, &w, &h);

            uint16_t x1 = (matrix.width() - w) / 2 + 4;
            uint16_t y1 = (matrix.height() / 2 - h) / 2 + h;

            //update x1 and y1 value
            matrix.getTextBounds(str, x1, y1, &x0, &y0, &w, &h);

            //Print to led matrix
            matrix.setCursor(x1, y1);
            matrix.print(str);

            // ------------------------
            // MAC Address at row 1
            // ------------------------

            strlcpy(str, WiFi.macAddress().c_str(), sizeof(str));

            matrix.setFont(&TomThumb);

            matrix.getTextBounds(str, 0, 0, &x0, &y0, &w, &h);

            x1 = (matrix.width() - w) / 2 + 4;
            y1 = (matrix.height() / 2 - h) / 2 + h + matrix.height() / 2;

            //update x1 and y1 value
            matrix.getTextBounds(str, x1, y1, &x0, &y0, &w, &h);

            //Print to led matrix
            matrix.setCursor(x1, y1);
            matrix.print(str);
        }

        // -------------------------------------------------------------------
        // MODE 1, PAGE 6
        // DNS ADDRESS
        // -------------------------------------------------------------------
        if (currentPageMode1 == 6)
        {

            //clear Screen
            matrix.clearDisplay();

            // ------------------------
            // String "DNS" at row 0
            // ------------------------

            char str[64];
            const char *ptr = PSTR("DNS");
            strcpy_P(str, ptr);

            int16_t x0, y0;
            uint16_t w, h;

            matrix.setFont(&TomThumb);

            matrix.getTextBounds(str, 0, 0, &x0, &y0, &w, &h);

            uint16_t x1 = (matrix.width() - w) / 2 + 4;
            uint16_t y1 = (matrix.height() / 2 - h) / 2 + h;

            //update x1 and y1 value
            matrix.getTextBounds(str, x1, y1, &x0, &y0, &w, &h);

            //Print to led matrix
            matrix.setCursor(x1, y1);
            matrix.print(str);

            // ------------------------
            // DNS Address at row 1
            // ------------------------

            strlcpy(str, WiFi.dnsIP().toString().c_str(), sizeof(str));

            matrix.setFont(&TomThumb);

            matrix.getTextBounds(str, 0, 0, &x0, &y0, &w, &h);

            x1 = (matrix.width() - w) / 2 + 4;
            y1 = (matrix.height() / 2 - h) / 2 + h + matrix.height() / 2;

            //update x1 and y1 value
            matrix.getTextBounds(str, x1, y1, &x0, &y0, &w, &h);

            //Print to led matrix
            matrix.setCursor(x1, y1);
            matrix.print(str);
        }

        // -------------------------------------------------------------------
        // MODE 1, PAGE 7
        // LAST SYNC
        // -------------------------------------------------------------------
        if (currentPageMode1 == 7)
        {

            //clear Screen
            matrix.clearDisplay();

            // ------------------------
            // String "Last sync" at row 0
            // ------------------------

            char str[64];
            const char *ptr = PSTR("Last sync");
            strcpy_P(str, ptr);

            int16_t x0, y0;
            uint16_t w, h;

            matrix.setFont(&TomThumb);

            matrix.getTextBounds(str, 0, 0, &x0, &y0, &w, &h);

            uint16_t x1 = (matrix.width() - w) / 2 + 4;
            uint16_t y1 = (matrix.height() / 2 - h) / 2 + h - 1;

            //update x1 and y1 value
            matrix.getTextBounds(str, x1, y1, &x0, &y0, &w, &h);

            //Print to led matrix
            matrix.setCursor(x1, y1);
            matrix.print(str);

            // ------------------------
            // Last sync time at row 1
            // ------------------------

            strcpy(str, getDateTimeStr(lastSync));

            matrix.setFont(&TomThumb);

            matrix.getTextBounds(str, 0, 0, &x0, &y0, &w, &h);

            x1 = (matrix.width() - w) / 2 + 4;
            y1 = (matrix.height() / 2 - h) / 2 + h + matrix.height() / 2;

            //update x1 and y1 value
            matrix.getTextBounds(str, x1, y1, &x0, &y0, &w, &h);

            //Print to led matrix
            matrix.setCursor(x1, y1);
            matrix.print(str);
        }

        // -------------------------------------------------------------------
        // MODE 1, PAGE 8
        // TIME
        // -------------------------------------------------------------------
        if (currentPageMode1 == 8)
        {

            //clear Screen
            matrix.clearDisplay();

            // ------------------------
            // String "Time" at row 0
            // ------------------------

            char str[64];
            const char *ptr = PSTR("Time");
            strcpy_P(str, ptr);

            int16_t x0, y0;
            uint16_t w, h;

            matrix.setFont(&TomThumb);

            matrix.getTextBounds(str, 0, 0, &x0, &y0, &w, &h);

            uint16_t x1 = (matrix.width() - w) / 2 + 4;
            uint16_t y1 = (matrix.height() / 2 - h) / 2 + h;

            //update x1 and y1 value
            matrix.getTextBounds(str, x1, y1, &x0, &y0, &w, &h);

            //Print to led matrix
            matrix.setCursor(x1, y1);
            matrix.print(str);

            // ------------------------
            // Local time at row 1
            // ------------------------

            strlcpy(str, getTimeStr(localTime), sizeof(str));

            matrix.setFont(&bold_led_board_7_regular4pt7b);
            matrix.setFont(&FiveBySeven5pt7b);
            matrix.setFont(&TomThumb);

            matrix.getTextBounds(str, 0, 0, &x0, &y0, &w, &h);

            x1 = (matrix.width() - w) / 2 + 4;
            y1 = (matrix.height() / 2 - h) / 2 + h + matrix.height() / 2;

            //update x1 and y1 value
            matrix.getTextBounds(str, x1, y1, &x0, &y0, &w, &h);

            //Print to led matrix
            matrix.setCursor(x1, y1);
            matrix.print(str);
        }

        // -------------------------------------------------------------------
        // MODE 1, PAGE 9
        // DATE
        // -------------------------------------------------------------------
        if (currentPageMode1 == 9)
        {

            //clear Screen
            matrix.clearDisplay();

            // ------------------------
            // String "Date" at row 0
            // ------------------------

            char str[64];
            const char *ptr = PSTR("Date");
            strcpy_P(str, ptr);

            int16_t x0, y0;
            uint16_t w, h;

            matrix.setFont(&TomThumb);

            matrix.getTextBounds(str, 0, 0, &x0, &y0, &w, &h);

            uint16_t x1 = (matrix.width() - w) / 2 + 4;
            uint16_t y1 = (matrix.height() / 2 - h) / 2 + h;

            //update x1 and y1 value
            matrix.getTextBounds(str, x1, y1, &x0, &y0, &w, &h);

            //Print to led matrix
            matrix.setCursor(x1, y1);
            matrix.print(str);

            // ------------------------
            // Local date at row 1
            // ------------------------

            strlcpy(str, getDateStr(localTime), sizeof(str));

            matrix.setFont(&TomThumb);

            matrix.getTextBounds(str, 0, 0, &x0, &y0, &w, &h);

            x1 = (matrix.width() - w) / 2 + 4;
            y1 = (matrix.height() / 2 - h) / 2 + h + matrix.height() / 2;

            //update x1 and y1 value
            matrix.getTextBounds(str, x1, y1, &x0, &y0, &w, &h);

            //Print to led matrix
            matrix.setCursor(x1, y1);
            matrix.print(str);
        }

        // -------------------------------------------------------------------
        // MODE 1, PAGE 10
        // UPTIME
        // -------------------------------------------------------------------
        if (currentPageMode1 == 10)
        {

            //clear Screen
            matrix.clearDisplay();

            // ------------------------
            // String "Uptime" at row 0
            // ------------------------

            char str[64];
            const char *ptr = PSTR("Uptime");
            strcpy_P(str, ptr);

            int16_t x0, y0;
            uint16_t w, h;

            matrix.setFont(&TomThumb);

            matrix.getTextBounds(str, 0, 0, &x0, &y0, &w, &h);

            uint16_t x1 = (matrix.width() - w) / 2 + 4;
            uint16_t y1 = (matrix.height() / 2 - h) / 2 + h - 1;

            //update x1 and y1 value
            matrix.getTextBounds(str, x1, y1, &x0, &y0, &w, &h);

            //Print to led matrix
            matrix.setCursor(x1, y1);
            matrix.print(str);

            // ------------------------
            // Uptime at row 1
            // ------------------------

            strlcpy(str, getUptimeStr(), sizeof(str));

            matrix.setFont(&TomThumb);

            matrix.getTextBounds(str, 0, 0, &x0, &y0, &w, &h);

            x1 = (matrix.width() - w) / 2 + 4;
            y1 = (matrix.height() / 2 - h) / 2 + h + matrix.height() / 2;

            //update x1 and y1 value
            matrix.getTextBounds(str, x1, y1, &x0, &y0, &w, &h);

            //Print to led matrix
            matrix.setCursor(x1, y1);
            matrix.print(str);
        }

        // -------------------------------------------------------------------
        // MODE 1, PAGE 11
        // LAST BOOT
        // -------------------------------------------------------------------
        if (currentPageMode1 == 11)
        {

            //clear Screen
            matrix.clearDisplay();

            // ------------------------
            // String "Last boot" at row 0
            // ------------------------

            char str[64];
            const char *ptr = PSTR("Last boot");
            strcpy_P(str, ptr);

            int16_t x0, y0;
            uint16_t w, h;

            matrix.setFont(&TomThumb);

            matrix.getTextBounds(str, 0, 0, &x0, &y0, &w, &h);

            uint16_t x1 = (matrix.width() - w) / 2 + 4;
            uint16_t y1 = (matrix.height() / 2 - h) / 2 + h;

            //update x1 and y1 value
            matrix.getTextBounds(str, x1, y1, &x0, &y0, &w, &h);

            //Print to led matrix
            matrix.setCursor(x1, y1);
            matrix.print(str);

            // ------------------------
            // Last boot time at row 1
            // ------------------------

            // strlcpy(str, getDateTimeStr(localTime(_lastBoot)), sizeof(str));
            strlcpy(str, getLastBootStr(), sizeof(str));

            matrix.setFont(&TomThumb);

            matrix.getTextBounds(str, 0, 0, &x0, &y0, &w, &h);

            x1 = (matrix.width() - w) / 2 + 4;
            y1 = (matrix.height() / 2 - h) / 2 + h + matrix.height() / 2;

            //update x1 and y1 value
            matrix.getTextBounds(str, x1, y1, &x0, &y0, &w, &h);

            //Print to led matrix
            matrix.setCursor(x1, y1);
            matrix.print(str);
        }

        // -------------------------------------------------------------------
        // MODE 1, PAGE 12
        // FREE HEAP
        // -------------------------------------------------------------------
        if (currentPageMode1 == 12)
        {

            //clear Screen
            matrix.clearDisplay();

            // ------------------------
            // String "Free heap" at row 0
            // ------------------------

            char str[64];
            const char *ptr = PSTR("Free heap");
            strcpy_P(str, ptr);

            int16_t x0, y0;
            uint16_t w, h;

            matrix.setFont(&TomThumb);

            matrix.getTextBounds(str, 0, 0, &x0, &y0, &w, &h);

            uint16_t x1 = (matrix.width() - w) / 2 + 4;
            uint16_t y1 = (matrix.height() / 2 - h) / 2 + h - 1;

            //update x1 and y1 value
            matrix.getTextBounds(str, x1, y1, &x0, &y0, &w, &h);

            //Print to led matrix
            matrix.setCursor(x1, y1);
            matrix.print(str);

            // ------------------------
            // Free heap at row 1
            // ------------------------

            itoa(ESP.getFreeHeap(), str, 10);

            matrix.setFont(&TomThumb);

            matrix.getTextBounds(str, 0, 0, &x0, &y0, &w, &h);

            x1 = (matrix.width() - w) / 2 + 4;
            y1 = (matrix.height() / 2 - h) / 2 + h + matrix.height() / 2;

            //update x1 and y1 value
            matrix.getTextBounds(str, x1, y1, &x0, &y0, &w, &h);

            //Print to led matrix
            matrix.setCursor(x1, y1);
            matrix.print(str);
        }
    }

    // -------------------------------------------------------------------
    // MODE 2
    // -------------------------------------------------------------------

    else if (MODE == 2)
    {
        static bool stateEdit = false;
        static uint32_t startMillis = 0;
        static bool stateSwitch_old = false;
        static int16_t count = 0;
        static int16_t pos = -1;
        static int16_t encoder0Pos_old = encoder0Pos;

        if (MODE != MODE_old)
        {
            MODE_old = MODE;
            Serial.println(F("MODE 2 [Edit Mode]"));

            // reset static variables
            stateEdit = false;
            startMillis = millis();
            stateSwitch_old = false;
            count = 0;
            pos = -1;
            encoder0Pos_old = encoder0Pos;
        }

        // static uint32_t timerSwitch;

        // if (stateSwitch != stateSwitch_old)
        // {

        //     stateSwitch_old = stateSwitch;

        //     if (stateSwitch == HIGH)
        //     {
        //         // startMillis = millis();

        //         // stateEdit = !stateEdit;

        //         // timerSwitch = millis();
        //         // alarmState = HIGH;
        //         // tone0 = HIGH;

        //         // if (stateEdit == false)
        //         // {
        //         //   _config.timezone = tempTimezone;
        //         //   save_config();
        //         // }
        //     }
        //     else if (stateSwitch == LOW)
        //     {
        //         // stateEdit = false;
        //     }
        // }

        bool encoder0PosIncreasing = false;
        bool encoder0PosDecreasing = false;

        if (encoder0Pos > encoder0Pos_old)
        {
            encoder0PosIncreasing = true;
            encoder0Pos_old = encoder0Pos;
        }
        else if (encoder0Pos < encoder0Pos_old)
        {
            encoder0PosDecreasing = true;
            encoder0Pos_old = encoder0Pos;
        }

        if (encoder0PosIncreasing && stateEdit == false)
        {
            currentPageMode2++;
            if (currentPageMode2 >= PageTitleMode2Count)
            {
                currentPageMode2 = 0;
            }
        }
        else if (encoder0PosDecreasing && stateEdit == false)
        {
            currentPageMode2--;
            if (currentPageMode2 <= 0 || currentPageMode2 >= PageTitleMode2Count)
            {
                currentPageMode2 = PageTitleMode2Count - 1;
            }
        }

        // -------------------------------------------------------------------
        // MODE 2, PAGE 0
        // SETTING TIMEZONE
        // -------------------------------------------------------------------
        if (currentPageMode2 == 0)
        {
            // static int8_t tempTimezone = 0;

            if (currentPageMode2_old != currentPageMode2)
            {
                currentPageMode2_old = currentPageMode2;
                Serial.print(F("EDIT MODE, PAGE "));
                Serial.println(currentPageMode2);

                // reset static variables used in this page
                stateEdit = false;
                stateSwitch_old = false;
                count = 0;
                // tempTimezone = _config.timezone;
            }

            //clear Screen
            matrix.clearDisplay();

            // -------
            // Row 0
            // -------

            char str[16];
            const char *ptr = PSTR("Set Timezone");
            strcpy_P(str, ptr);

            int16_t x0, y0;
            uint16_t w, h;
            matrix.setFont(&TomThumb);
            matrix.getTextBounds(str, 0, 0, &x0, &y0, &w, &h);
            uint16_t x1 = (matrix.width() - (w - 4)) / 2;
            uint16_t y1 = (matrix.height() / 2 - h) / 2 + h - 0;
            //update x1 and y1 value
            matrix.getTextBounds(str, x1, y1, &x0, &y0, &w, &h);

            //Print to led matrix
            matrix.setCursor(x1, y1);
            matrix.print(str);

            // -------
            // Row 1
            // -------

            // matrix.setFont(&TomThumb);
            // matrix.setFont(&bold_led_board_7_regular4pt7b);
            // matrix.setFont(&RepetitionScrolling5pt8b);
            matrix.setFont(&FiveBySeven5pt7b);
            // matrix.setFont(&Org_01);

            if (stateEdit)
            {
                if (encoder0PosIncreasing)
                {
                    count++;
                    if (count == timezoneCount)
                    {
                        count = 0;
                    }
                }
                else if (encoder0PosDecreasing)
                {
                    count--;
                    if (count < 0)
                    {
                        count = timezoneCount - 1;
                    }
                }

                if (count == WIB)
                {
                    ptr = PSTR("WIB (+7)");
                    // tempTimezone = 70;
                }
                if (count == WITA)
                {
                    ptr = PSTR("WITA (+8)");
                    // tempTimezone = 80;
                }
                if (count == WIT)
                {
                    ptr = PSTR("WIT (+9)");
                    // tempTimezone = 90;
                }

                strcpy_P(str, ptr);

                matrix.getTextBounds(str, 0, 0, &x0, &y0, &w, &h);
                x1 = (matrix.width() - (w - 4)) / 2;
                y1 = (matrix.height() / 2 - h) / 2 + h - 1;

                if (state500ms)
                {
                    // matrix.fillRect(x1, y1, w - 5, h, 0);
                    ptr = PSTR("");
                }
            }
            else
            {
                if (_configLocation.timezone == 70)
                {
                    ptr = PSTR("WIB (+7)");
                    count = 0;
                }
                else if (_configLocation.timezone == 80)
                {
                    ptr = PSTR("WITA (+8)");
                    count = 1;
                }
                else if (_configLocation.timezone == 90)
                {
                    ptr = PSTR("WIT (+9)");
                    count = 2;
                }
                else
                {
                    ptr = PSTR("Unknown");
                }
            }

            strcpy_P(str, ptr);

            matrix.getTextBounds(str, 0, 0, &x0, &y0, &w, &h);
            x1 = (matrix.width() - (w - 4)) / 2;
            y1 = (matrix.height() / 2 - h) / 2 + h - 1;
            //update x1 and y1 value
            // matrix.getTextBounds(str, x1, y1, &x0, &y0, &w, &h);

            matrix.setCursor(x1, y1 + 8);
            matrix.print(str);
        }

        // -------------------------------------------------------------------
        // MODE 2, PAGE 1
        // SETTING LOCATION
        // -------------------------------------------------------------------
        else if (currentPageMode2 == 1)
        {
            //clear Screen
            matrix.clearDisplay();

            matrix.setFont(&bold_led_board_7_regular4pt7b);
            matrix.setFont(&FiveBySeven5pt7b);
            matrix.setFont(&F14LED7pt8b);
            matrix.setFont(&RepetitionScrolling5pt8b);
            matrix.setFont(&TomThumb);

            if (currentPageMode2_old != currentPageMode2)
            {
                currentPageMode2_old = currentPageMode2;
                Serial.print(F("EDIT MODE, PAGE "));
                Serial.println(currentPageMode2);

                // reset static variables used in this page
                stateEdit = false;
                stateSwitch_old = false;
                count = 0;
                // tempTimezone = _config.timezone;

                load_regencies_file(11);
            }

            static int cityIndex = 1;

            static bool stateSwitch_old = 0;
            if (stateSwitch != stateSwitch_old)
            {

                stateSwitch_old = stateSwitch;

                if (stateSwitch == HIGH)
                {
                    cityIndex++;
                }
            }

            matrix.setCursor(0, 6);

            load_provinces_file(cityIndex);

            matrix.setCursor(0, 14);

            load_regencies_file(cityIndex);
        }

        // -------------------------------------------------------------------
        // MODE 2, PAGE 2
        // SETTING DATE
        // -------------------------------------------------------------------
        if (currentPageMode2 == 2)
        {
            //clear Screen
            matrix.clearDisplay();

            static bool yesNo = false;

            static uint16_t yr = yearLocal;
            static int8_t mo = monthLocal;
            static int8_t d = mdayLocal;

            static int16_t xYr = 0;
            static int16_t xMo = 0;
            static int16_t xD = 0;
            static int16_t xYesNo = 0;

            if (currentPageMode2_old != currentPageMode2)
            {
                currentPageMode2_old = currentPageMode2;
                Serial.print(F("EDIT MODE, PAGE "));
                Serial.println(currentPageMode2);

                // store current page
                _ledMatrixSettings.pagemode2 = currentPageMode2;

                // reset static variables
                stateEdit = false;
                stateSwitch_old = false;
                yesNo = false;
                pos = -1;
                yr = yearLocal;
                mo = monthLocal;
                d = mdayLocal;
                // hr = hourLocal;
                // min = minLocal;
                // sec = secLocal;

                xYr = 0;
                xMo = 0;
                xD = 0;
                xYesNo = 0;
            }

            static bool stateSwitch_old = false;
            if (stateSwitch != stateSwitch_old)
            {
                stateSwitch_old = stateSwitch;

                if (stateSwitch == true)
                {
                    startMillis = millis();
                }
                else if (stateSwitch == false)
                {
                    if (enterEditModeFromShortcut)
                    {
                        enterEditModeFromShortcut = false;
                    }
                    else if (millis() <= startMillis + 500)
                    {
                        stateEdit = true;
                        pos++;
                        if (pos > 3)
                        {
                            // reset
                            pos = -1;
                            stateEdit = false;

                            if (yesNo)
                            {
                                // calculate utcTime
                                uint16_t _yr = yr;
                                uint8_t _mo = mo;
                                uint8_t _d = d;
                                uint8_t _hr = hourLocal;
                                uint8_t _min = minLocal;
                                uint8_t _sec = secLocal;

                                RtcDateTime dt(_yr, _mo, _d, _hr, _min, _sec);

                                // convert t to utc
                                dt -= TimezoneSeconds();
                                PRINT("\r\nTimestamp to set: %u\r\n", dt.Epoch32Time());

                                // finally, set rtc time;
                                Rtc.SetDateTime(dt);
                                PRINT("RTC has been updated!\r\n");

                                // sync system time
                                timeval tv = {dt.Epoch32Time(), 0};
                                timezone tz = {0, 0};
                                settimeofday(&tv, &tz);
                                PRINT("System time has been updated!\r\n\r\n");

                                // update last sync
                                lastSync = dt.Epoch32Time();

                                // reset to no
                                yesNo = false;

                                tone1 = HIGH;
                            }
                        }
                    }
                }
            }

            if (stateSwitch == false && millis() <= startMillis + 500)
            {
            }

            // if (stateSwitch != stateSwitch_old)
            // {

            //     stateSwitch_old = stateSwitch;

            //     if (stateSwitch == HIGH)
            //     {
            //         // startMillis = millis();

            //         pos++;

            //         if (pos < 4)
            //         {
            //             stateEdit = true;
            //         }
            //         else
            //         {
            //             // reset pos & stateedit
            //             pos = -1;
            //             stateEdit = false;

            //             if (yesNo)
            //             {
            //                 // calculate utcTime
            //                 uint16_t _yr = yr;
            //                 uint8_t _mo = mo;
            //                 uint8_t _d = d;
            //                 uint8_t _hr = hourLocal;
            //                 uint8_t _min = minLocal;
            //                 uint8_t _sec = secLocal;

            //                 RtcDateTime dt(_yr, _mo, _d, _hr, _min, _sec);

            //                 // convert t to utc
            //                 dt -= TimezoneSeconds();
            //                 PRINT("\r\nTimestamp to set: %u\r\n", dt.Epoch32Time());

            //                 // finally, set rtc time;
            //                 Rtc.SetDateTime(dt);
            //                 PRINT("RTC has been updated!\r\n");

            //                 // sync system time
            //                 timeval tv = {dt.Epoch32Time(), 0};
            //                 timezone tz = {0, 0};
            //                 settimeofday(&tv, &tz);
            //                 PRINT("System time has been updated!\r\n\r\n");

            //                 // update last sync
            //                 lastSync = dt.Epoch32Time();

            //                 // reset to no
            //                 yesNo = false;

            //                 tone1 = HIGH;
            //             }
            //         }

            //         // timerSwitch = millis();
            //         // alarmState = HIGH;
            //         // tone0 = HIGH;

            //         // if (stateEdit == false)
            //         // {
            //         //   _config.timezone = tempTimezone;
            //         //   save_config();
            //         // }
            //     }
            //     else if (stateSwitch == LOW)
            //     {
            //         // stateEdit = false;
            //     }
            // }

            if (stateEdit)
            {
                static uint8_t pos_old = 0;
                if (pos != pos_old)
                {
                    pos_old = pos;
                    PRINT("pos: %d\r\n", pos);
                }
                if (pos == 0)
                {
                    if (encoder0PosIncreasing)
                    {
                        yr++;
                        if (yr > 2100)
                        {
                            yr = 2018;
                        }
                    }
                    else if (encoder0PosDecreasing)
                    {
                        yr--;
                        if (yr < 2018)
                        {
                            yr = 2100;
                        }
                    }
                }
                else if (pos == 1)
                {
                    if (encoder0PosIncreasing)
                    {
                        mo++;
                        if (mo > 12)
                        {
                            mo = 1;
                        }
                    }
                    else if (encoder0PosDecreasing)
                    {
                        mo--;
                        if (mo < 1)
                        {
                            mo = 12;
                        }
                    }
                }
                else if (pos == 2)
                {
                    // 1. January - 31 days
                    // 2. February - 28 days in a common year and 29 days in leap years
                    // 3. March - 31 days
                    // 4. April - 30 days
                    // 5. May - 31 days
                    // 6. June - 30 days
                    // 7. July - 31 days
                    // 8. August - 31 days
                    // 9. September - 30 days
                    // 10. October - 31 days
                    // 11. November - 30 days
                    // 12. December - 31 days

                    int8_t maxDay = 31;

                    if (mo == 1 || mo == 3 || mo == 5 || mo == 7 || mo == 8 || mo == 10 || mo == 12)
                    {
                        maxDay = 31;
                    }
                    else if (mo == 4 || mo == 6 || mo == 9 || mo == 11)
                    {
                        maxDay = 30;
                    }
                    else if (mo == 2)
                    {
                        if (yr % 4 == 0)
                        {
                            maxDay = 29;
                        }
                        else
                        {
                            maxDay = 28;
                        }
                    }

                    if (encoder0PosIncreasing)
                    {
                        d++;
                        if (d > maxDay)
                        {
                            d = 1;
                        }
                    }
                    else if (encoder0PosDecreasing)
                    {
                        d--;
                        if (d < 1)
                        {
                            d = maxDay;
                        }
                    }
                }
                else if (pos == 3)
                {
                    if (encoder0PosIncreasing || encoder0PosDecreasing)
                    {
                        yesNo = !yesNo;
                    }
                }
            }

            char str[16];
            const GFXfont *font;
            int16_t x, y;
            uint16_t w, h;

            // -------
            // Row 0
            // -------
            bool ROW_0 = true;

            if (ROW_0)
            {
                if (stateEdit == false)
                {
                    const char *ptr = PSTR("DATE");
                    strcpy_P(str, ptr);

                    font = &TomThumb;
                    matrix.setFont(font);

                    matrix.getTextBounds(str, 0, 0, &x, &y, &w, &h);

                    // set starting row row based on font type
                    if (font == &TomThumb)
                    {
                        x = (matrix.width() - w) / 2 + 4;
                        y = (matrix.height() / 2 - h) / 2 + h;
                    }
                    else
                    {
                        x = (matrix.width() - w) / 2;
                        y = (matrix.height() / 2 - h) / 2 + h;
                    }

                    x = 1;

                    //Print to led matrix
                    matrix.setCursor(x, y);
                    matrix.print(str);
                }
                else if (stateEdit)
                {
                    const char *ptr = PSTR("SAVE: ");
                    strcpy_P(str, ptr);

                    // font = &Org_01;
                    font = &TomThumb;
                    matrix.setFont(font);

                    x = 1;
                    y = 6;

                    // Print to led matrix
                    matrix.setCursor(x, y);
                    matrix.print(str);

                    // Print yesNo
                    xYesNo = matrix.getCursorX();

                    if (yesNo)
                    {
                        ptr = PSTR("YES");
                    }
                    else
                    {
                        ptr = PSTR("NO");
                    }
                    strcpy_P(str, ptr);
                    matrix.print(str);
                }
            }

            // -------
            // Row 1
            // -------
            bool ROW_1 = true;

            if (ROW_1)
            {
                // matrix.setFont(&TomThumb);
                // matrix.setFont(&bold_led_board_7_regular4pt7b);
                // matrix.setFont(&RepetitionScrolling5pt8b);
                matrix.setFont(&FiveBySeven5pt7b);
                // matrix.setFont(&Org_01);

                int16_t x = 0;
                int16_t y = 14;

                if (stateEdit == false)
                {
                    //Print Year
                    yr = yearLocal;
                    sprintf(str, "%02d", yr);
                    xYr = x;
                    matrix.setCursor(xYr, y);
                    matrix.print(str);

                    // //Print separator
                    // x0 = x0 + 22;
                    // matrix.setCursor(x0, y);
                    // matrix.print(" ");

                    //Print month
                    mo = monthLocal;
                    sprintf(str, "%s", monthShortStr(mo));
                    x = x + 27;
                    xMo = x;
                    matrix.setCursor(xMo, y);
                    matrix.print(str);

                    // //Print separator
                    // x0 = x0 + 11;
                    // matrix.setCursor(x0, y);
                    // matrix.print(FPSTR(" "));

                    //Print day
                    d = mdayLocal;
                    sprintf(str, "%02d", d);
                    x = x + 21;
                    xD = x;
                    matrix.setCursor(xD, y);
                    matrix.print(str);
                }
                else if (stateEdit)
                {
                    //Print hour
                    sprintf(str, "%02d", yr);
                    matrix.setCursor(xYr, y);
                    matrix.print(str);

                    // //Print separator
                    // matrix.setCursor(xYr + 11, y);
                    // matrix.print("-");

                    //Print min
                    sprintf(str, "%s", monthShortStr(mo));
                    matrix.setCursor(xMo, y);
                    matrix.print(str);

                    // //Print separator
                    // matrix.setCursor(xMo + 11, y);
                    // matrix.print(FPSTR("-"));

                    //Print sec
                    sprintf(str, "%02d", d);
                    matrix.setCursor(xD, y);
                    matrix.print(str);
                }
            }

            // blink
            if (state250ms && stateEdit)
            {
                int16_t x = 0;
                int16_t y = 0;
                uint16_t w = 0;
                uint16_t h = 0;

                if (pos == 0) // year
                {
                    x = xYr;
                    y = 8;
                    w = 24;
                    h = 7;
                }
                else if (pos == 1) // month
                {
                    x = xMo;
                    y = 8;
                    w = 18;
                    h = 7;
                }
                else if (pos == 2) // day
                {
                    x = xD;
                    y = 8;
                    w = 12;
                    h = 7;
                }
                if (pos == 3)
                {
                    x = xYesNo;
                    y = 1;
                    w = 17;
                    h = 5;
                }
                matrix.fillRect(x, y, w, h, false);
                // matrix.drawRect(x, y-1, w+1, h+2, 1);
            }
        }

        // -------------------------------------------------------------------
        // MODE 2, PAGE 3
        // SETTING TIME
        // -------------------------------------------------------------------
        if (currentPageMode2 == 3)
        {
            //clear Screen
            matrix.clearDisplay();

            static bool yesNo = false;
            static int8_t hr = hourLocal;
            static int8_t min = minLocal;
            static int8_t sec = secLocal;
            static int16_t xHr = 0;
            static int16_t xMin = 0;
            static int16_t xSec = 0;
            static int16_t xYesNo = 0;

            if (currentPageMode2_old != currentPageMode2)
            {
                currentPageMode2_old = currentPageMode2;
                Serial.print(F("EDIT MODE, PAGE "));
                Serial.println(currentPageMode2);

                // store current page
                _ledMatrixSettings.pagemode2 = currentPageMode2;

                // reset static variables
                stateEdit = false;
                stateSwitch_old = false;
                yesNo = false;
                pos = -1;
                hr = hourLocal;
                min = minLocal;
                sec = secLocal;
                // d = mdayLocal;
                // mo = monthLocal;
                // yr = yearLocal;

                xHr = 0;
                xMin = 0;
                xSec = 0;
                xYesNo = 0;
            }

            static bool stateSwitch_old = false;
            if (stateSwitch != stateSwitch_old)
            {
                stateSwitch_old = stateSwitch;
                if (stateSwitch == true)
                {
                    startMillis = millis();
                }
                else if (stateSwitch == false)
                {
                    if (enterEditModeFromShortcut)
                    {
                        enterEditModeFromShortcut = false;
                    }
                    else if (millis() <= startMillis + 500)
                    {
                        stateEdit = true;
                        pos++;
                        if (pos > 3)
                        {
                            // reset
                            pos = -1;
                            stateEdit = false;

                            if (yesNo)
                            {
                                // calculate utcTime
                                uint16_t y = yearLocal;
                                uint8_t mo = monthLocal;
                                uint8_t d = mdayLocal;
                                uint8_t h = hr;
                                uint8_t mi = min;
                                uint8_t s = sec;

                                RtcDateTime dt(y, mo, d, h, mi, s);

                                // convert t to utc
                                dt -= TimezoneSeconds();
                                PRINT("\r\nTimestamp to set: %u\r\n", dt.Epoch32Time());

                                // finally, set rtc time;
                                Rtc.SetDateTime(dt);
                                PRINT("RTC has been updated!\r\n");

                                // sync system time
                                timeval tv = {dt.Epoch32Time(), 0};
                                timezone tz = {0, 0};
                                settimeofday(&tv, &tz);
                                PRINT("System time has been updated!\r\n\r\n");

                                // update last sync
                                lastSync = dt.Epoch32Time();

                                // reset to no
                                yesNo = false;

                                tone1 = HIGH;
                            }
                        }
                    }
                }
            }

            if (stateEdit)
            {
                if (pos == 0)
                {
                    if (encoder0PosIncreasing)
                    {
                        hr++;
                        if (hr > 23)
                        {
                            hr = 0;
                        }
                    }
                    else if (encoder0PosDecreasing)
                    {
                        hr--;
                        if (hr < 0)
                        {
                            hr = 23;
                        }
                    }
                }
                else if (pos == 1)
                {
                    if (encoder0PosIncreasing)
                    {
                        min++;
                        if (min > 59)
                        {
                            min = 0;
                        }
                    }
                    else if (encoder0PosDecreasing)
                    {
                        min--;
                        if (min < 0)
                        {
                            min = 59;
                        }
                    }
                }
                else if (pos == 2)
                {
                    if (encoder0PosIncreasing)
                    {
                        sec++;
                        if (sec > 59)
                        {
                            sec = 0;
                        }
                    }
                    else if (encoder0PosDecreasing)
                    {
                        sec--;
                        if (sec < 0)
                        {
                            sec = 59;
                        }
                    }
                }
                else if (pos == 3)
                {
                    if (encoder0PosIncreasing || encoder0PosDecreasing)
                    {
                        yesNo = !yesNo;
                    }
                }
            }

            // -------
            // Row 0
            // -------

            char str[16];

            if (stateEdit == false)
            {
                const char *ptr = PSTR("TIME");
                strcpy_P(str, ptr);

                int16_t x0, y0;
                uint16_t w, h;
                matrix.setFont(&TomThumb);
                matrix.getTextBounds(str, 0, 0, &x0, &y0, &w, &h);
                uint16_t x1 = (matrix.width() - (w - 4)) / 2;
                uint16_t y1 = (matrix.height() / 2 - h) / 2 + h - 0;
                //update x1 and y1 value
                matrix.getTextBounds(str, x1, y1, &x0, &y0, &w, &h);

                //Print to led matrix
                matrix.setCursor(1, y1);
                matrix.print(str);
            }
            else if (stateEdit)
            {
                const char *ptr = PSTR("SAVE: ");
                strcpy_P(str, ptr);

                matrix.setFont(&TomThumb);

                // Print to led matrix
                matrix.setCursor(1, 6);
                matrix.print(str);

                // Print yesNo
                xYesNo = matrix.getCursorX();

                if (yesNo)
                {
                    ptr = PSTR("YES");
                    strcpy_P(str, ptr);
                    matrix.print(str);
                }
                else
                {
                    ptr = PSTR("NO");
                    strcpy_P(str, ptr);
                    matrix.print(str);
                }
            }

            // -------
            // Row 1
            // -------

            // matrix.setFont(&TomThumb);
            // matrix.setFont(&bold_led_board_7_regular4pt7b);
            matrix.setFont(&RepetitionScrolling5pt8b);
            // matrix.setFont(&FiveBySeven5pt7b);
            // matrix.setFont(&Org_01);

            if (stateEdit == false)
            {
                int16_t x0 = 1;
                int16_t y0 = 6;

                // //Print day
                // sprintf(str, "%02d", d);
                // matrix.setCursor(x0, y0);
                // matrix.print(str);

                // //Print month
                // x0 = x0 + 9;
                // sprintf(str, "%s", monthShortStr(mo));
                // matrix.setCursor(x0, y0);
                // matrix.print(str);

                // //Print year
                // x0 = x0 + 13;
                // sprintf(str, "%04d", yr);
                // matrix.setCursor(x0, y0);
                // matrix.print(str);

                //Print hour
                hr = hourLocal;
                sprintf(str, "%02d", hr);
                x0 = 1;
                matrix.setCursor(x0, y0 + 8);
                matrix.print(str);
                xHr = x0;

                //Print ":"
                x0 = x0 + 11;
                matrix.setCursor(x0, y0 + 8);
                matrix.print(":");

                //Print min
                min = minLocal;
                sprintf(str, "%02d", min);
                x0 = x0 + 6;
                matrix.setCursor(x0, y0 + 8);
                matrix.print(str);
                xMin = x0;

                //Print ":"
                x0 = x0 + 11;
                matrix.setCursor(x0, y0 + 8);
                matrix.print(FPSTR(":"));

                //Print sec
                sec = 0;
                sprintf(str, "%02d", secLocal);
                x0 = x0 + 6;
                matrix.setCursor(x0, y0 + 8);
                matrix.print(str);
                xSec = x0;
            }
            else if (stateEdit)
            {
                int16_t y = 14;

                //Print hour
                sprintf(str, "%02d", hr);
                matrix.setCursor(xHr, y);
                matrix.print(str);

                //Print ":"
                matrix.setCursor(xHr + 11, y);
                matrix.print(":");

                //Print min
                sprintf(str, "%02d", min);
                matrix.setCursor(xMin, y);
                matrix.print(str);

                //Print ":"
                matrix.setCursor(xMin + 11, y);
                matrix.print(FPSTR(":"));

                //Print sec
                sprintf(str, "%02d", sec);
                matrix.setCursor(xSec, y);
                matrix.print(str);
            }

            // blink
            if (state250ms && stateEdit)
            {
                int16_t x = 0;
                int16_t y = 8;
                uint16_t w = 11;
                uint16_t h = 7;

                if (pos == 0)
                {
                    x = xHr;
                }
                else if (pos == 1)
                {
                    x = xMin;
                }
                else if (pos == 2)
                {
                    x = xSec;
                }
                if (pos == 3)
                {
                    x = xYesNo;
                    y = 1;
                    w = 11;
                    h = 5;
                }
                matrix.fillRect(x, y, w, h, 0);
            }
        }
    }
}