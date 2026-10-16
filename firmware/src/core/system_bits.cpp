#include "core/system_bits.h"


// ============================================================
// GLOBAL EVENT GROUP
// ============================================================

EventGroupHandle_t systemEvents = NULL;


// ============================================================
// INITIALIZE SYSTEM EVENTS
// ============================================================

bool initSystemEvents()
{
    systemEvents = xEventGroupCreate();

    if (systemEvents == NULL)
    {
        Serial.println("[ERROR] Cannot create system EventGroup");

        return false;
    }

    Serial.println("[OK] System EventGroup created");

    return true;
}