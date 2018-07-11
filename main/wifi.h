#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

extern EventGroupHandle_t wifi_event_group;
extern const int WIFI_CONNECTED_BIT;

void wifi_init();
