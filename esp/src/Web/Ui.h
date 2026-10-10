#pragma once

#ifdef ARDUINO
#include <Arduino.h>
#endif

namespace revov {

// Starts the HTTP listener that serves the sim console page
// on WEB_UI_PORT and the WebSocket command/event channel on
// SIM_WS_PORT.
void webui_init();

// Services pending HTTP requests and WebSocket frames,
// including the EventBus fan-out to connected console
// clients. Must run once per loop iteration; it never
// blocks waiting for a client.
void webui_service();

// True once the listeners are running.
bool webui_isServing();

}  // namespace revov
