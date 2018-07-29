#ifndef ioexpanderhelper_h
#define ioexpanderhelper_h

#include <Wire.h>
#include "pinouthelper.h"
#include "encoderhelper.h"

extern boolean stateSwitch;

extern volatile bool PCFInterruptFlag;

void ICACHE_RAM_ATTR PCFInterrupt();

bool IoExpanderSetup();
void IoExpanderLoop();



#endif 