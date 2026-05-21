#include "silent_serial.h"

#ifdef Serial
#undef Serial
#endif

SilentSerialWrapper SilentSerial(::Serial);

#define Serial SilentSerial

