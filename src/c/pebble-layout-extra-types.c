#include <pebble.h>
#include <pebble-layout/pebble-layout.h>
#include <pebble-layout/pebble-json.h>
#include <pebble-events/pebble-events.h>
#include "pebble-layout-extra-types.h"

#define eq(s, t) (strncmp(s, t, strlen(s)) == 0)

struct DateTimeLayerData {
    TextLayer *layer;
    char buf[64];
    char format[32];
    EventHandle event_handle;
};

static void *prv_date_time_layer_create(GRect frame) {
    struct DateTimeLayerData *data = malloc(sizeof(struct DateTimeLayerData));
    data->layer = text_layer_create(frame);
    return data;
}

static void prv_date_time_layer_destroy(void *object) {
    struct DateTimeLayerData *data = (struct DateTimeLayerData *) object;

    if (data->event_handle) events_tick_timer_service_unsubscribe(data->event_handle);
    data->event_handle = NULL;

    text_layer_destroy(data->layer);
    data->layer = NULL;

    free(data);
}

static void prv_date_time_layer_tick_handler(struct tm *tick_time, TimeUnits units_changed, void *context) {
    struct DateTimeLayerData *data = (struct DateTimeLayerData *) context;
    strftime(data->buf, sizeof(data->buf), data->format, tick_time);
    text_layer_set_text(data->layer, data->buf);
}

static void prv_date_time_layer_parse(Layout *layout, Json *json, void *object) {
    struct DateTimeLayerData *data = (struct DateTimeLayerData *) object;

    size_t size = json_get_size(json);
    for (size_t i = 0; i < size; i++) {
        char *key = json_next_string(json);
        if (eq(key, "format")) {
            char *value = json_next_string(json);
            strncpy(data->format, value, sizeof(data->format));
            free(value);
        } else if (eq(key, "granularity")) {
            TimeUnits unit = YEAR_UNIT;
            char *value = json_next_string(json);
            if (eq(value, "month")) unit = MONTH_UNIT;
            else if (eq(value, "day")) unit = DAY_UNIT;
            else if (eq(value, "hour")) unit = HOUR_UNIT;
            else if (eq(value, "minute")) unit = MINUTE_UNIT;
            else if (eq(value, "second")) unit = SECOND_UNIT;
            free(value);
            data->event_handle = events_tick_timer_service_subscribe_context(unit, prv_date_time_layer_tick_handler, data);
        } else {
            json_skip_tree(json);
        }
        free(key);
    }

    if (data->event_handle) {
        time_t now = time(NULL);
        prv_date_time_layer_tick_handler(localtime(&now), SECOND_UNIT, data);
    }
}

static Layer *prv_date_time_layer_get_layer(void *object) {
    struct DateTimeLayerData *data = (struct DateTimeLayerData *) object;
    return text_layer_get_layer(data->layer);
}

static void *prv_date_time_layer_cast(void *object) {
    struct DateTimeLayerData *data = (struct DateTimeLayerData *) object;
    return data->layer;
}

void layout_add_date_time_type(Layout *layout) {
    layout_add_type(layout, "DateTimeLayer", (TypeFuncs) {
        .create = prv_date_time_layer_create,
        .destroy = prv_date_time_layer_destroy,
        .parse = prv_date_time_layer_parse,
        .get_layer = prv_date_time_layer_get_layer,
        .cast = prv_date_time_layer_cast
    }, "TextLayer");
}

struct BatteryLayerData {
    TextLayer *layer;
    char buf[8];
    EventHandle *event_handle;
};

static void prv_battery_layer_event_handler(BatteryChargeState charge_state, void *context) {
    struct BatteryLayerData *data = (struct BatteryLayerData *) context;
    snprintf(data->buf, sizeof(data->buf), "%d%%", charge_state.charge_percent);
    text_layer_set_text(data->layer, data->buf);
}

static void *prv_battery_layer_create(GRect frame) {
    struct BatteryLayerData *data = malloc(sizeof(struct BatteryLayerData));
    data->layer = text_layer_create(frame);

    prv_battery_layer_event_handler(battery_state_service_peek(), data);
    data->event_handle = events_battery_state_service_subscribe_context(prv_battery_layer_event_handler, data);

    return data;
}

static void prv_battery_layer_destroy(void *object) {
    struct BatteryLayerData *data = (struct BatteryLayerData *) object;

    events_battery_state_service_unsubscribe(data->event_handle);
    data->event_handle = NULL;

    text_layer_destroy(data->layer);
    data->layer = NULL;

    free(data);
}

static Layer *prv_battery_layer_get_layer(void *object) {
    struct BatteryLayerData *data = (struct BatteryLayerData *) object;
    return text_layer_get_layer(data->layer);
}

static void *prv_battery_layer_cast(void *object) {
    struct BatteryLayerData *data = (struct BatteryLayerData *) object;
    return data->layer;
}

void layout_add_battery_type(Layout *layout) {
    layout_add_type(layout, "BatteryLayer", (TypeFuncs) {
        .create = prv_battery_layer_create,
        .destroy = prv_battery_layer_destroy,
        .get_layer = prv_battery_layer_get_layer,
        .cast = prv_battery_layer_cast
    }, "TextLayer");
}

struct ConnectionToggleData {
    bool show_on_connected;
    EventHandle event_handle;
};

static void *prv_connection_toggle_create(GRect frame) {
    return layer_create_with_data(frame, sizeof(struct ConnectionToggleData));
}

static void prv_connection_toggle_destroy(void *object) {
    Layer *layer = (Layer *) object;
    struct ConnectionToggleData *data = layer_get_data(layer);

    events_connection_service_unsubscribe(data->event_handle);
    data->event_handle = NULL;

    layer_destroy(layer);
}

static void prv_connection_toggle_connection_handler(bool connected, void *context) {
    Layer *layer = (Layer *) context;
    struct ConnectionToggleData *data = layer_get_data(layer);
    layer_set_hidden(layer, data->show_on_connected ? !connected : connected);
}

static void prv_connection_toggle_parse(Layout *layout, Json *json, void *object) {
    Layer *layer = (Layer *) object;
    struct ConnectionToggleData *data = layer_get_data(layer);

    EventConnectionHandlers handlers;
    handlers.pebble_app_connection_handler = prv_connection_toggle_connection_handler;
    handlers.pebblekit_connection_handler = NULL;
    bool connected = connection_service_peek_pebble_app_connection();
    size_t size = json_get_size(json);
    for (size_t i = 0; i < size; i++) {
        char *key = json_next_string(json);
        if (eq(key, "state")) {
            char *value = json_next_string(json);
            if (eq(value, "show")) data->show_on_connected = true;
            else data->show_on_connected = false;
            free(value);
        } else if (eq(key, "source")) {
            char *value = json_next_string(json);
            if (eq(value, "pebblekit")) {
                handlers.pebble_app_connection_handler = NULL;
                handlers.pebblekit_connection_handler = prv_connection_toggle_connection_handler;
                connected = connection_service_peek_pebblekit_connection();
            }
            free(value);
        } else {
            json_skip_tree(json);
        }
        free(key);
    }

    prv_connection_toggle_connection_handler(connected, layer);
    data->event_handle = events_connection_service_subscribe_context(handlers, layer);
}

void layout_add_connection_toggle_type(Layout *layout) {
    layout_add_type(layout, "ConnectionToggle", (TypeFuncs) {
        .create = prv_connection_toggle_create,
        .destroy = prv_connection_toggle_destroy,
        .parse = prv_connection_toggle_parse
    }, NULL);
}
