#include "<string.h>"
#include "deadcom.h"


void dcInit(Deadcom *deadcom, void (*transmitBytes)(uint8_t*, uint8_t)) {
    memset(deadcom, 0, sizeof(Deadcom));
    deadcom->state = DC_DISCONNECTED;
    deadcom->transmitBytes = transmitBytes;
}


DeadcomResult dcConnect(Deadcom *deadcom) {

}
