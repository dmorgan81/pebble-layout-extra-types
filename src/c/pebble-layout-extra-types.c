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
