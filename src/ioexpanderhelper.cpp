#include "ioexpanderhelper.h"

volatile bool PCFInterruptFlag = false;

boolean stateSwitch;

void ICACHE_RAM_ATTR PCFInterrupt()
{
    PCFInterruptFlag = true;
}

bool IoExpanderSetup()
{
    // https://github.com/RalphBacon/PCF8574-Pin-Extender-I2C/blob/master/PCF8574_InputOutputInterrupt.ino
    // PCF8574N is 'reverse' logic inasmuch it SINKS current
    // so HIGH is OFF and LOW is ON (will we remember this?)

    //IoExpanderWire.begin(SDA, SCL);
    //Specsheets say PCF8574 is officially rated only for 100KHz I2C-bus
    //IoExpanderWire.setClock(100000L);

    // Turn OFF all pins by sending a high byte (1 bit per byte)
    //  Wire.beginTransmission(IO_EXPANDER_ADDRESS);
    //  Wire.write(0xF);
    //  Wire.endTransmission();

    //Setup interrupt
    pinMode(MCU_INTERRUPT_PIN, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(MCU_INTERRUPT_PIN), PCFInterrupt, FALLING);

    return true;
}

void IoExpanderLoop()
{
    // static bool encoder0PosIncreasing;
    // static bool encoder0PosDecreasing;
    // static int encoder0Pos = 0;
    static bool ledState;
    static unsigned long startLed;

    // i2C Interrup
    //PCFInterruptFlag = false;
    if (PCFInterruptFlag)
    {

        Wire.requestFrom(IO_EXPANDER_ADDRESS, 1);

        bool wireAvailable = Wire.available();

        byte readIOexpander = 0;
        if (wireAvailable)
        {
            readIOexpander = Wire.read();
        }
        else
        {
            PCFInterruptFlag = false;
            return;
        }

        //    // print Binary reading of IO expander
        //    static byte count;
        //    Serial.print(readIOexpander, BIN);
        //    if (readIOexpander != 0b11111111) {
        //      count++;
        //      Serial.print(" - ");
        //      Serial.println(count);
        //    } else {
        //      count = 0;
        //      Serial.print(" - ");
        //      Serial.println(count);
        //      Serial.println();
        //    }

        if (wireAvailable)
        {
            boolean rtcSQWState = bitRead(readIOexpander, RTC_SQW_PIN);
            if (rtcSQWState == 0)
            {
                //  Serial.print(rtcSQWState);
                //  Serial.print(", ");
                //  Serial.print(millis());
                //  Serial.print(", ");
                //  RtcDateTime dt = Rtc.GetDateTime();
                //  time_t timeRtc = dt.Epoch32Time();
                //  Serial.println(timeRtc);

                ledState = HIGH;
                digitalWrite(LED_1, LOW);
            }

            stateSwitch = !bitRead(readIOexpander, ENCODER_SW_PIN);

            static boolean stateA_old;
            boolean stateA = !bitRead(readIOexpander, ENCODER_CLK_PIN);
            if (stateA != stateA_old)
            {
                //Serial.print("A: ");
                //Serial.println (stateA);
                stateA_old = stateA;
                if (stateA == HIGH)
                {
                    if (!bitRead(readIOexpander, ENCODER_DT_PIN) == LOW)
                    {
                        encoder0Pos++;
                        encoder0PosIncreasing = true;
                    }
                    else
                    {
                        encoder0Pos--;
                        encoder0PosDecreasing = true;
                    }
                    // Serial.print("Position: ");
                    // Serial.println (encoder0Pos);
                }
            }
            else
            {
                encoder0PosIncreasing = false;
                encoder0PosIncreasing = false;
            }
        }

        // DO NOTE: When you write LOW to a pin on a PCF8574 it becomes an OUTPUT.
        // It wouldn't generate an interrupt if you were to connect a button to it that pulls it HIGH when you press the button.
        // Any pin you wish to use as input must be written HIGH and be pulled LOW to generate an interrupt.
        //pcf8574.write(6, LOW);
        //pcf8574.toggle(6);
        PCFInterruptFlag = false;
    }
}