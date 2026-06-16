#include "src/resource_ids.auto.h"
#include <pebble.h>
#include <stdio.h>
#define LETTERLEN 13
#define BATTERYLEN 9
#define RENDERLETTERS 6
#define WEATHERLEN 6
#define CTOF *1.8+32
#define SETTINGS_KEY 1
#define DEFAULT_DAY 1
#define DEFAULT_NIGHT 0
typedef struct ClaySettings {
  bool Celsius; // false = Fahrenheit, true = Celsius
  bool weatherScreens; // false = hidden, true = shown
  int forceWeather; // -1 = disabled, 0>= enabled
} ClaySettings;

static ClaySettings settings;
static void prv_default_settings() {
  settings.Celsius = false;
  settings.weatherScreens = false;
  settings.forceWeather = -1;
}
static void prv_save_settings() {
  persist_write_data(SETTINGS_KEY, &settings, sizeof(settings));
}

static void prv_load_settings() {
  prv_default_settings();
  persist_read_data(SETTINGS_KEY, &settings, sizeof(settings));
}


static Window *s_main_window;
static GBitmap *s_bitmap_day;
static int lastWeather = 0;
static BitmapLayer *s_battery_layer;
static BitmapLayer *s_weather_icon_layer;
static bool day = true;
static BitmapLayer *s_backdrop_layer;
static TextLayer *s_time_text_layer;
static TextLayer *s_batt_text_layer;
static TextLayer *s_step_layer;
static TextLayer *s_weather_text_layer;
static TextLayer *s_date_layer;

const int weather_codes[WEATHERLEN] = {
  RESOURCE_ID_SUN,
  RESOURCE_ID_PARTYCLOUD,
  RESOURCE_ID_CLOUDY,
  RESOURCE_ID_RAIN,
  RESOURCE_ID_LIGHTNING,
  RESOURCE_ID_SNOW
};
const int batt_codes[BATTERYLEN] = {
  RESOURCE_ID_BATT_CHARGE,
  RESOURCE_ID_BATT_FULL,
    RESOURCE_ID_BATT_90,
    RESOURCE_ID_BATT_75,
    RESOURCE_ID_BATT_50,
    RESOURCE_ID_BATT_40,
    RESOURCE_ID_BATT_25,
    RESOURCE_ID_BATT_15,
    RESOURCE_ID_BATT_5
};
const int day_codes[WEATHERLEN] = {
  RESOURCE_ID_DAY_CLEAR,
    RESOURCE_ID_DAY_PARTYCLOUD,
  RESOURCE_ID_DAY_CLOUDY,
  RESOURCE_ID_DAY_RAIN,
  RESOURCE_ID_DAY_LIGHTNING,
  RESOURCE_ID_DAY_SNOW,
};
const int night_codes[WEATHERLEN] = {
  RESOURCE_ID_NIGHT_CLEAR,
    RESOURCE_ID_NIGHT_PARTYCLOUD,
  RESOURCE_ID_NIGHT_CLOUDY,
  RESOURCE_ID_NIGHT_RAIN,
  RESOURCE_ID_NIGHT_LIGHTNING,
  RESOURCE_ID_NIGHT_SNOW,
};

static GBitmap* s_bitmap_battery[BATTERYLEN];
static GBitmap* s_bitmap_weather[WEATHERLEN];
static int s_steps = 0;
static int s_battery_level;
static bool s_battery_charge;
static void update_weather() {
  DictionaryIterator *iter;
  app_message_outbox_begin(&iter);
  dict_write_uint8(iter, MESSAGE_KEY_REQUEST_WEATHER, 1);
  app_message_outbox_send();

}
static void prv_update_display() {
  update_weather();  
}
void grab_clock(int i) {
  gbitmap_destroy(s_bitmap_day);
  int imageindex = day?DEFAULT_DAY:DEFAULT_NIGHT;
  if(settings.weatherScreens) {
    imageindex = settings.weatherScreens;
  }
  else if(settings.forceWeather > -1) {
    imageindex = settings.forceWeather;
  }
  if(day) {
    s_bitmap_day = gbitmap_create_with_resource(day_codes[imageindex]);
  }
  else {
    s_bitmap_day = gbitmap_create_with_resource(night_codes[imageindex]);

  }
}

static void update_battery() {
  static char buffer[5];
  snprintf(buffer, 5, "%d%%",s_battery_level);
  text_layer_set_text(s_batt_text_layer, buffer);

  if(s_battery_charge) {
   bitmap_layer_set_bitmap(s_battery_layer, s_bitmap_battery[0] );

  }
  else {
    if(s_battery_level >= 90) {
      bitmap_layer_set_bitmap(s_battery_layer, s_bitmap_battery[1] );
    }
    else if(s_battery_level > 75) {
      bitmap_layer_set_bitmap(s_battery_layer, s_bitmap_battery[2] );

    }
    else if(s_battery_level > 50) {
      bitmap_layer_set_bitmap(s_battery_layer, s_bitmap_battery[3] );
    }
    else if(s_battery_level > 40) {
      bitmap_layer_set_bitmap(s_battery_layer, s_bitmap_battery[4] );
    }
    else if(s_battery_level > 25) {
      bitmap_layer_set_bitmap(s_battery_layer, s_bitmap_battery[5] );
    }
    else if(s_battery_level > 15) {
      bitmap_layer_set_bitmap(s_battery_layer, s_bitmap_battery[6] );
    }

    else if(s_battery_level > 5) {
      bitmap_layer_set_bitmap(s_battery_layer, s_bitmap_battery[7] );
    }
    else {
      bitmap_layer_set_bitmap(s_battery_layer, s_bitmap_battery[8] );

    }
  }

}
static void battery_callback(BatteryChargeState state) {
  // Record the new battery level
  s_battery_level = state.charge_percent;
  s_battery_charge = state.is_charging;
  // Update the meter
  update_battery();
}
static void update_steps() {
  HealthMetric metric = HealthMetricStepCount;
  time_t start = time_start_of_today();
  time_t end = time(NULL);

  // Check the metric has data available for today
  HealthServiceAccessibilityMask mask = health_service_metric_accessible(metric,
    start, end);

  if(mask & HealthServiceAccessibilityMaskAvailable) {
    // Data is available!
    s_steps = (int)health_service_sum_today(metric);
  }
  static char buffer[7]; // 6 chars for steps is ridiculous but thats what we're going with here
  snprintf(buffer, 7, "%d",s_steps);

  text_layer_set_text(s_step_layer, buffer);
}
static void health_callback(HealthEventType event, void *context) {
  if(event == HealthEventMovementUpdate) {
    update_steps();
  }
}
static void update_time() {
  // Get a tm structure
  time_t temp = time(NULL);
  struct tm *tick_time = localtime(&temp);

  // Write the current hours and minutes into a buffer
  static char s_date_buffer[32];
  strftime(s_date_buffer, sizeof(s_date_buffer), "%a %b %d %Y", tick_time);

  // Display the date
  text_layer_set_text(s_date_layer, s_date_buffer);
  static char s_time_buffer[8];
  strftime(s_time_buffer, sizeof(s_time_buffer), "%I:%M", tick_time);
  text_layer_set_text(s_time_text_layer, s_time_buffer);
  if(tick_time->tm_hour > 19 || tick_time->tm_hour < 6) {
    if(day) {
      day = false;
      grab_clock(lastWeather);
      text_layer_set_text_color(s_time_text_layer, GColorWhite);
    }
  }
  else {
    if(!day) {
      day = true;
      grab_clock(lastWeather);
      text_layer_set_text_color(s_time_text_layer, GColorBlack);

    }

  }
  int subhour = tick_time->tm_hour;

  // if(((tick_time->tm_hour-subhour)/10)%10) {
  //     layer_set_hidden(bitmap_layer_get_layer(s_letter_layer[0]), false);
  //     bitmap_layer_set_bitmap(s_letter_layer[0], s_bitmap_letters[((tick_time->tm_hour-subhour)/10)%10]);
  // }
  // else {
  //     layer_set_hidden(bitmap_layer_get_layer(s_letter_layer[0]), true);

  // }
  // bitmap_layer_set_bitmap(s_letter_layer[1], s_bitmap_letters[(tick_time->tm_hour-subhour)%10]);
  // bitmap_layer_set_bitmap(s_letter_layer[2], s_bitmap_letters[(tick_time->tm_min/10)%10]);
  // bitmap_layer_set_bitmap(s_letter_layer[3], s_bitmap_letters[(tick_time->tm_min)%10]);
  // if(!clock_is_24h_style()) {
  //     layer_set_hidden(bitmap_layer_get_layer(s_letter_layer[4]), false);
  //     layer_set_hidden(bitmap_layer_get_layer(s_letter_layer[5]), false);

  //     bitmap_layer_set_bitmap(s_letter_layer[4], subhour?s_bitmap_letters[10]:s_bitmap_letters[11]);
  //     bitmap_layer_set_bitmap(s_letter_layer[5], s_bitmap_letters[12]);
  // }
  // else {
  //   layer_set_hidden(bitmap_layer_get_layer(s_letter_layer[4]), true);
  //   layer_set_hidden(bitmap_layer_get_layer(s_letter_layer[5]), true);

  // }
}
static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  update_time();
  // Get weather update every 30 minutes
  if (tick_time->tm_min % 30 == 0) {
    update_weather();
  }
}
static void main_window_load(Window *window) {
  s_backdrop_layer = bitmap_layer_create(GRect(0, 0, 200, 228));
  bitmap_layer_set_compositing_mode(s_backdrop_layer, GCompOpSet);
  bitmap_layer_set_bitmap(s_backdrop_layer, s_bitmap_day);
  // Show the Window on the watch, with animated=true
  layer_add_child(window_get_root_layer(window), 
                                      bitmap_layer_get_layer(s_backdrop_layer));
  // for(int i = 0; i < RENDERLETTERS; i++) {
  //   s_letter_layer[i] = bitmap_layer_create(letterpos[i]);
  //   bitmap_layer_set_compositing_mode(s_letter_layer[i], GCompOpSet);
  //   bitmap_layer_set_bitmap(s_letter_layer[i], s_bitmap_letters[0]);
  //   layer_add_child(window_get_root_layer(window), 
  //   bitmap_layer_get_layer(s_letter_layer[i]));

  // }
  s_battery_layer = bitmap_layer_create(GRect(0,228-9-2,16,9));
  bitmap_layer_set_compositing_mode(s_battery_layer, GCompOpSet);
  bitmap_layer_set_bitmap(s_battery_layer, s_bitmap_battery[0]);
  layer_add_child(window_get_root_layer(window), 
  bitmap_layer_get_layer(s_battery_layer));
  s_batt_text_layer = text_layer_create(
    GRect(16, 228-16, 9*4, 16)); 
  text_layer_set_background_color(s_batt_text_layer, GColorClear);
  text_layer_set_text_color(s_batt_text_layer, GColorWhite);
  text_layer_set_font(s_batt_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));
  text_layer_set_text_alignment(s_batt_text_layer, GTextAlignmentLeft);
  layer_add_child(window_get_root_layer(window), text_layer_get_layer(s_batt_text_layer));

  //place steps at x=68
  s_step_layer = text_layer_create(
  GRect(68, 228-16, 9*8, 16)); 
  text_layer_set_background_color(s_step_layer, GColorClear);
  text_layer_set_text_color(s_step_layer, GColorWhite);
  text_layer_set_font(s_step_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));
  text_layer_set_text_alignment(s_step_layer, GTextAlignmentLeft);
  layer_add_child(window_get_root_layer(window), text_layer_get_layer(s_step_layer));
  text_layer_set_text(s_step_layer, "888888");
  // place weather at x=98+16
  s_weather_icon_layer = bitmap_layer_create(GRect(98,228-9-2,16,9));
  bitmap_layer_set_compositing_mode(s_weather_icon_layer, GCompOpSet);
  bitmap_layer_set_bitmap(s_weather_icon_layer, s_bitmap_weather[0]);
  layer_add_child(window_get_root_layer(window), 
  bitmap_layer_get_layer(s_weather_icon_layer));

  s_weather_text_layer = text_layer_create(
  GRect(98+16, 228-16, 9*8, 16)); 
  text_layer_set_background_color(s_weather_text_layer, GColorClear);
  text_layer_set_text_color(s_weather_text_layer, GColorWhite);
  text_layer_set_font(s_weather_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));
  text_layer_set_text_alignment(s_weather_text_layer, GTextAlignmentLeft);
  layer_add_child(window_get_root_layer(window), text_layer_get_layer(s_weather_text_layer));
  text_layer_set_text(s_weather_text_layer, "Loading...");

  s_date_layer = text_layer_create(
  GRect(0, 155+40, 200, 20)); 
  text_layer_set_background_color(s_date_layer, GColorClear);
  text_layer_set_text_color(s_date_layer, GColorWhite);
  text_layer_set_font(s_date_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18));
  text_layer_set_text_alignment(s_date_layer, GTextAlignmentCenter);
  layer_add_child(window_get_root_layer(window), text_layer_get_layer(s_date_layer));
  text_layer_set_text(s_date_layer, "Mon Jan 01");
  s_time_text_layer = text_layer_create(
    GRect(0, 32, 200, 48)); 
  text_layer_set_background_color(s_time_text_layer, GColorClear);
  text_layer_set_text_color(s_time_text_layer, GColorBlack);
  text_layer_set_font(s_time_text_layer, fonts_get_system_font(FONT_KEY_ROBOTO_BOLD_SUBSET_49));
  text_layer_set_text_alignment(s_time_text_layer, GTextAlignmentCenter);
  layer_add_child(window_get_root_layer(window), text_layer_get_layer(s_time_text_layer));
  text_layer_set_text(s_time_text_layer, "5:00");

  
}

static void main_window_unload(Window *window) {

}
static void inbox_received_callback(DictionaryIterator *iterator, void *context) {
  Tuple *temp_tuple = dict_find(iterator, MESSAGE_KEY_TEMPERATURE);
  Tuple *conditions_tuple = dict_find(iterator, MESSAGE_KEY_CONDITIONS);
  Tuple *condint_tuple = dict_find(iterator, MESSAGE_KEY_CONDINT);

  if (temp_tuple && conditions_tuple) {
    static char temperature_buffer[8];
    static char conditions_buffer[32];
    static char weather_layer_buffer[42];
    static int weathercode = 0;
    weathercode = (int)condint_tuple->value->int32;
    if(settings.Celsius) {
      snprintf(temperature_buffer, sizeof(temperature_buffer), "%d°C", (int)(temp_tuple->value->int32));
    }
    else {
      snprintf(temperature_buffer, sizeof(temperature_buffer), "%d°F", (int)(temp_tuple->value->int32 * 1.8 + 32));
    }
    snprintf(conditions_buffer, sizeof(conditions_buffer), "%s", conditions_tuple->value->cstring);
    snprintf(weather_layer_buffer, sizeof(weather_layer_buffer), "%s %s", temperature_buffer, conditions_buffer);
    text_layer_set_text(s_weather_text_layer, weather_layer_buffer);
    if (weathercode == 0) {bitmap_layer_set_bitmap(s_weather_icon_layer, s_bitmap_weather[0]); grab_clock(0); lastWeather=0;}
    else if (weathercode <= 48) {bitmap_layer_set_bitmap(s_weather_icon_layer, s_bitmap_weather[2]);grab_clock(2); lastWeather=2;}
    else if (weathercode <= 67) {bitmap_layer_set_bitmap(s_weather_icon_layer, s_bitmap_weather[3]);grab_clock(3); lastWeather=3;}
    else if (weathercode <= 77) {bitmap_layer_set_bitmap(s_weather_icon_layer, s_bitmap_weather[5]);grab_clock(5); lastWeather=5;}
    else if (weathercode <= 82) {bitmap_layer_set_bitmap(s_weather_icon_layer, s_bitmap_weather[3]);grab_clock(3); lastWeather=3;}
    else if (weathercode <= 86) {bitmap_layer_set_bitmap(s_weather_icon_layer, s_bitmap_weather[5]);grab_clock(5); lastWeather=5;}
    else if (weathercode == 95) {bitmap_layer_set_bitmap(s_weather_icon_layer, s_bitmap_weather[4]);grab_clock(4); lastWeather=4;}
    else if (weathercode <= 99) {bitmap_layer_set_bitmap(s_weather_icon_layer, s_bitmap_weather[4]);grab_clock(4); lastWeather=4;}
  }
  Tuple *showcelsius_t = dict_find(iterator, MESSAGE_KEY_CELSIUS);
  if (showcelsius_t) {
    settings.Celsius = showcelsius_t->value->int32 == 1;
  }

  Tuple *dynamicimage_t = dict_find(iterator, MESSAGE_KEY_DYNAMICIMAGE);
  if (dynamicimage_t) {
    settings.weatherScreens = dynamicimage_t->value->int32 == 1;
  }
  Tuple *forceseason_t = dict_find(iterator, MESSAGE_KEY_FORCESEASON);
  if (forceseason_t) { //yeah i know this is gross, but im lazy and this was easy enough
    if(forceseason_t->value->cstring[0] == '-') settings.forceWeather = -1;
    if(forceseason_t->value->cstring[0] == '0') settings.forceWeather = 0;
    if(forceseason_t->value->cstring[0] == '1') settings.forceWeather = 1;
    if(forceseason_t->value->cstring[0] == '2') settings.forceWeather = 2;
    if(forceseason_t->value->cstring[0] == '3') settings.forceWeather = 3;
    if(forceseason_t->value->cstring[0] == '4') settings.forceWeather = 4;
    if(forceseason_t->value->cstring[0] == '5') settings.forceWeather = 5;
  }
  // Save and apply if any settings were changed
  if (showcelsius_t || dynamicimage_t || forceseason_t) {
    prv_save_settings();
    prv_update_display();
}
}

static void inbox_dropped_callback(AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Message dropped!");
}

static void outbox_failed_callback(DictionaryIterator *iterator, AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Outbox send failed!");
}

static void outbox_sent_callback(DictionaryIterator *iterator, void *context) {
  APP_LOG(APP_LOG_LEVEL_INFO, "Outbox send success!");
}

static void init() {
  prv_load_settings();
  s_main_window = window_create();
  window_set_background_color(s_main_window, GColorBlack);

  // Set handlers to manage the elements inside the Window
  window_set_window_handlers(s_main_window, (WindowHandlers) {
    .load = main_window_load,
    .unload = main_window_unload
  });
  s_bitmap_day = gbitmap_create_with_resource(DEFAULT_DAY);

    //initialize backdrop

  // for(int i = 0; i < LETTERLEN; i++) {
  //   s_bitmap_letters[i] = gbitmap_create_with_resource(letter_codes[i]);
  // }
  for(int i = 0; i < BATTERYLEN; i++) {
    s_bitmap_battery[i] = gbitmap_create_with_resource(batt_codes[i]);
  }
  for(int i = 0; i < WEATHERLEN; i++) {
    s_bitmap_weather[i] = gbitmap_create_with_resource(weather_codes[i]);

  }
  app_message_register_inbox_received(inbox_received_callback);
  app_message_register_inbox_dropped(inbox_dropped_callback);
  app_message_register_outbox_failed(outbox_failed_callback);
  app_message_register_outbox_sent(outbox_sent_callback);

  // Open AppMessage
  const int inbox_size = 256;
  const int outbox_size = 256;
  app_message_open(inbox_size, outbox_size);
  // Show the Window on the watch, with animated=true
  window_stack_push(s_main_window, true);
  
  update_time();

  // Register with TickTimerService
  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
    // Register for battery level updates
  battery_state_service_subscribe(battery_callback);
  health_service_events_subscribe(health_callback, NULL);
  // Ensure battery level is displayed from the start
  battery_callback(battery_state_service_peek());
  update_steps();
  update_weather();

}

static void deinit() {
  window_destroy(s_main_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
