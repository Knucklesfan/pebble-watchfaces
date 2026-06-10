#include "src/resource_ids.auto.h"
#include <pebble.h>
#include <stdbool.h>
// #define NULL 0
#define SETTINGS_KEY 1
#define FACEOFFSET_X 36
#define FACEOFFSET_Y 24
typedef struct ClaySettings {
  bool Celsius; // false = Fahrenheit, true = Celsius
  bool ShowTime; // false = hidden, true = shown
  bool ShowDate; // ''
  bool ShowSteps; // ''
  bool ShowBattery; // ''
  bool ShowWeather; // ''
} ClaySettings;

static ClaySettings settings;
static void prv_default_settings() {
  settings.Celsius = false;
  settings.ShowTime = true;
  settings.ShowDate = true;
  settings.ShowSteps = true;
  settings.ShowBattery = true;
  settings.ShowWeather = true;

}

static void prv_save_settings() {
  persist_write_data(SETTINGS_KEY, &settings, sizeof(settings));
}

static void prv_load_settings() {
  prv_default_settings();
  persist_read_data(SETTINGS_KEY, &settings, sizeof(settings));
}

#define CLOCKFACES 64
#define BATTERYLEN 9
#define WEATHERLEN 6
// static uint8_t currentFrame = 0;
#if PBL_DISPLAY_WIDTH == 260
  uint8_t offset_x = 34;
#else
  uint8_t offset_x = 4;
#endif
static Window *s_main_window;
static GBitmap *s_bitmap_backdrop;
static GBitmap* s_bitmap_clock;

static GBitmap *s_bitmap_foot;

static BitmapLayer *s_battery_layer;
static BitmapLayer *s_weather_icon_layer;

static BitmapLayer *s_backdrop_layer;
static BitmapLayer *s_clock_layer;
static BitmapLayer *s_foot_layer;


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
static int stuff[CLOCKFACES];
static GBitmap* s_bitmap_battery[BATTERYLEN];
static GBitmap* s_bitmap_weather[WEATHERLEN];
static int s_steps = 0;
static int s_battery_level;
static bool s_battery_charge;
void grab_clock(int i) {
  gbitmap_destroy(s_bitmap_clock);
  s_bitmap_clock = gbitmap_create_with_resource(RESOURCE_ID_CLOCK_0+(i%CLOCKFACES));
}

static void update_weather() {
  DictionaryIterator *iter;
  app_message_outbox_begin(&iter);
  dict_write_uint8(iter, MESSAGE_KEY_REQUEST_WEATHER, 1);
  app_message_outbox_send();

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
static void prv_update_display() {


  // Show/hide date based on setting
  layer_set_hidden(text_layer_get_layer(s_date_layer), !settings.ShowDate);

  layer_set_hidden(text_layer_get_layer(s_batt_text_layer), !settings.ShowBattery);
  layer_set_hidden(bitmap_layer_get_layer(s_battery_layer), !settings.ShowBattery);

  layer_set_hidden(text_layer_get_layer(s_time_text_layer), !settings.ShowTime);

  layer_set_hidden(text_layer_get_layer(s_weather_text_layer), !settings.ShowWeather);
  layer_set_hidden(bitmap_layer_get_layer(s_weather_icon_layer), !settings.ShowWeather);

  layer_set_hidden(text_layer_get_layer(s_step_layer), !settings.ShowSteps);
  layer_set_hidden(bitmap_layer_get_layer(s_foot_layer), !settings.ShowSteps);
  layer_mark_dirty(text_layer_get_layer(s_weather_text_layer));
  
}
static void battery_callback(BatteryChargeState state) {
  if(settings.ShowBattery) {
  // Record the new battery level
  s_battery_level = state.charge_percent;
  s_battery_charge = state.is_charging;
  // Update the meter
  update_battery();
  }
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
    if(settings.ShowSteps) update_steps();
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
  float clock_percentage = ((tick_time->tm_hour) + (tick_time->tm_min/60.0f))/24.0f;

  int value = 64 * clock_percentage;
  printf("%d\n",value);

  printf("%d %d\n",tick_time->tm_hour,tick_time->tm_min);
  grab_clock(value);
  // if(tick_time->tm_hour > 19 || tick_time->tm_hour < 6) {
  //   if(day) {
  //     bitmap_layer_set_bitmap(s_backdrop_layer, s_bitmap_night);
  //     text_layer_set_text_color(s_time_text_layer, GColorBlack);
  //     day = false;
  //   }
  // }
  // else {
  //   if(!day) {
  //     bitmap_layer_set_bitmap(s_backdrop_layer, s_bitmap_day);
  //     text_layer_set_text_color(s_time_text_layer, GColorBlack);

  //     day = true;
  //   }

  // }
  // int subhour = tick_time->tm_hour;

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
  
  bitmap_layer_set_bitmap(s_clock_layer, s_bitmap_clock);
  // Get weather update every 30 minutes
  if (tick_time->tm_min % 30 == 0 && settings.ShowWeather) {
    update_weather();
  }
}
static void main_window_load(Window *window) {
  s_backdrop_layer = bitmap_layer_create(GRect(offset_x, offset_x, 192, 192));
  bitmap_layer_set_compositing_mode(s_backdrop_layer, GCompOpSet);
  bitmap_layer_set_bitmap(s_backdrop_layer, s_bitmap_backdrop);
  layer_add_child(window_get_root_layer(window), 
                                      bitmap_layer_get_layer(s_backdrop_layer));

  s_clock_layer = bitmap_layer_create(GRect(offset_x+FACEOFFSET_X, offset_x+FACEOFFSET_Y, 120, 84));
  bitmap_layer_set_compositing_mode(s_clock_layer, GCompOpSet);
  bitmap_layer_set_bitmap(s_clock_layer, s_bitmap_clock);
  layer_add_child(window_get_root_layer(window), 
  bitmap_layer_get_layer(s_clock_layer));
  // for(int i = 0; i < RENDERLETTERS; i++) {
  //   s_letter_layer[i] = bitmap_layer_create(letterpos[i]);
  //   bitmap_layer_set_compositing_mode(s_letter_layer[i], GCompOpSet);
  //   bitmap_layer_set_bitmap(s_letter_layer[i], s_bitmap_letters[0]);
  //   layer_add_child(window_get_root_layer(window), 
  //   bitmap_layer_get_layer(s_letter_layer[i]));

  // }
  // place weather at x=98+16
  #if PBL_DISPLAY_WIDTH == 260
    s_battery_layer = bitmap_layer_create(GRect(90,228-9-2-1,16,9));
  bitmap_layer_set_compositing_mode(s_battery_layer, GCompOpSet);
  bitmap_layer_set_bitmap(s_battery_layer, s_bitmap_battery[0]);
  layer_add_child(window_get_root_layer(window), 
  bitmap_layer_get_layer(s_battery_layer));
  s_batt_text_layer = text_layer_create(
    GRect(16+90, 228-16-1, 9*4, 16)); 
  text_layer_set_background_color(s_batt_text_layer, GColorClear);
  text_layer_set_text_color(s_batt_text_layer, GColorBlack);
  text_layer_set_font(s_batt_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));
  text_layer_set_text_alignment(s_batt_text_layer, GTextAlignmentLeft);
  layer_add_child(window_get_root_layer(window), text_layer_get_layer(s_batt_text_layer));

  s_foot_layer = bitmap_layer_create(GRect(68+88-16, 228-12, 16, 10));
  bitmap_layer_set_compositing_mode(s_foot_layer, GCompOpSet);
  bitmap_layer_set_bitmap(s_foot_layer, s_bitmap_foot);
  layer_add_child(window_get_root_layer(window), 
  bitmap_layer_get_layer(s_foot_layer));

  //place steps at x=68
  s_step_layer = text_layer_create(
  GRect(68+88, 228-16-1, 9*8, 16)); 
  text_layer_set_background_color(s_step_layer, GColorClear);
  text_layer_set_text_color(s_step_layer, GColorBlack);
  text_layer_set_font(s_step_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));
  text_layer_set_text_alignment(s_step_layer, GTextAlignmentLeft);
  layer_add_child(window_get_root_layer(window), text_layer_get_layer(s_step_layer));
  text_layer_set_text(s_step_layer, "888888");

    s_weather_icon_layer = bitmap_layer_create(GRect(90,228,9,9));
    bitmap_layer_set_compositing_mode(s_weather_icon_layer, GCompOpSet);
    bitmap_layer_set_bitmap(s_weather_icon_layer, s_bitmap_weather[0]);
    layer_add_child(window_get_root_layer(window), 
    bitmap_layer_get_layer(s_weather_icon_layer));

    s_weather_text_layer = text_layer_create(
    GRect(90+16, 228-5, 9*8, 16)); 
    text_layer_set_background_color(s_weather_text_layer, GColorClear);
    text_layer_set_text_color(s_weather_text_layer, GColorBlack);
    text_layer_set_font(s_weather_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));
    text_layer_set_text_alignment(s_weather_text_layer, GTextAlignmentLeft);
    layer_add_child(window_get_root_layer(window), text_layer_get_layer(s_weather_text_layer));
    text_layer_set_text(s_weather_text_layer, "Loading...");
  
  s_time_text_layer = text_layer_create(
    GRect(0, 146, PBL_DISPLAY_WIDTH, 48)); 
  text_layer_set_background_color(s_time_text_layer, GColorClear);
  text_layer_set_text_color(s_time_text_layer, GColorLimerick);
  text_layer_set_font(s_time_text_layer, fonts_get_system_font(FONT_KEY_LECO_32_BOLD_NUMBERS));
  text_layer_set_text_alignment(s_time_text_layer, GTextAlignmentCenter);
  layer_add_child(window_get_root_layer(window), text_layer_get_layer(s_time_text_layer));
  text_layer_set_text(s_time_text_layer, "5:00");


  #else
  s_battery_layer = bitmap_layer_create(GRect(offset_x,228-9-2,16,9));
  bitmap_layer_set_compositing_mode(s_battery_layer, GCompOpSet);
  bitmap_layer_set_bitmap(s_battery_layer, s_bitmap_battery[0]);
  layer_add_child(window_get_root_layer(window), 
  bitmap_layer_get_layer(s_battery_layer));
  s_batt_text_layer = text_layer_create(
    GRect(16+offset_x, 228-16, 9*4, 16)); 
  text_layer_set_background_color(s_batt_text_layer, GColorClear);
  text_layer_set_text_color(s_batt_text_layer, GColorBlack);
  text_layer_set_font(s_batt_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));
  text_layer_set_text_alignment(s_batt_text_layer, GTextAlignmentLeft);
  layer_add_child(window_get_root_layer(window), text_layer_get_layer(s_batt_text_layer));

  s_foot_layer = bitmap_layer_create(GRect(28, 228-14, 9*8, 16));
  bitmap_layer_set_compositing_mode(s_foot_layer, GCompOpSet);
  bitmap_layer_set_bitmap(s_foot_layer, s_bitmap_foot);
  layer_add_child(window_get_root_layer(window), 
  bitmap_layer_get_layer(s_foot_layer));

  //place steps at x=68
  s_step_layer = text_layer_create(
  GRect(68+offset_x, 228-16, 9*8, 16)); 
  text_layer_set_background_color(s_step_layer, GColorClear);
  text_layer_set_text_color(s_step_layer, GColorBlack);
  text_layer_set_font(s_step_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));
  text_layer_set_text_alignment(s_step_layer, GTextAlignmentLeft);
  layer_add_child(window_get_root_layer(window), text_layer_get_layer(s_step_layer));
  text_layer_set_text(s_step_layer, "888888");

  s_weather_icon_layer = bitmap_layer_create(GRect(98+offset_x,228-9-2,16,9));
  bitmap_layer_set_compositing_mode(s_weather_icon_layer, GCompOpSet);
  bitmap_layer_set_bitmap(s_weather_icon_layer, s_bitmap_weather[0]);
  layer_add_child(window_get_root_layer(window), 
  bitmap_layer_get_layer(s_weather_icon_layer));

  s_weather_text_layer = text_layer_create(
  GRect(98+16+offset_x, 228-16, 9*8, 16)); 
  text_layer_set_background_color(s_weather_text_layer, GColorClear);
  text_layer_set_text_color(s_weather_text_layer, GColorBlack);
  text_layer_set_font(s_weather_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));
  text_layer_set_text_alignment(s_weather_text_layer, GTextAlignmentLeft);
  layer_add_child(window_get_root_layer(window), text_layer_get_layer(s_weather_text_layer));
  text_layer_set_text(s_weather_text_layer, "Loading...");

  s_time_text_layer = text_layer_create(
    GRect(0, 116, PBL_DISPLAY_WIDTH, 48)); 
  text_layer_set_background_color(s_time_text_layer, GColorClear);
  text_layer_set_text_color(s_time_text_layer, GColorLimerick);
  text_layer_set_font(s_time_text_layer, fonts_get_system_font(FONT_KEY_LECO_32_BOLD_NUMBERS));
  text_layer_set_text_alignment(s_time_text_layer, GTextAlignmentCenter);
  layer_add_child(window_get_root_layer(window), text_layer_get_layer(s_time_text_layer));
  text_layer_set_text(s_time_text_layer, "5:00");

  #endif


  s_date_layer = text_layer_create(
  GRect(0, 155+40, PBL_DISPLAY_WIDTH, 20)); 
  text_layer_set_background_color(s_date_layer, GColorClear);
  text_layer_set_text_color(s_date_layer, GColorBlack);
  text_layer_set_font(s_date_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18));
  text_layer_set_text_alignment(s_date_layer, GTextAlignmentCenter);
  layer_add_child(window_get_root_layer(window), text_layer_get_layer(s_date_layer));
  text_layer_set_text(s_date_layer, "Mon Jan 01");

  prv_update_display();
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
    if (weathercode == 0) bitmap_layer_set_bitmap(s_weather_icon_layer, s_bitmap_weather[0]);
    else if (weathercode <= 48) bitmap_layer_set_bitmap(s_weather_icon_layer, s_bitmap_weather[2]);
    else if (weathercode <= 67) bitmap_layer_set_bitmap(s_weather_icon_layer, s_bitmap_weather[3]);
    else if (weathercode <= 77) bitmap_layer_set_bitmap(s_weather_icon_layer, s_bitmap_weather[5]);
    else if (weathercode <= 82) bitmap_layer_set_bitmap(s_weather_icon_layer, s_bitmap_weather[3]);
    else if (weathercode <= 86) bitmap_layer_set_bitmap(s_weather_icon_layer, s_bitmap_weather[5]);
    else if (weathercode == 95) bitmap_layer_set_bitmap(s_weather_icon_layer, s_bitmap_weather[4]);
    else if (weathercode <= 99) bitmap_layer_set_bitmap(s_weather_icon_layer, s_bitmap_weather[4]);
  }
  Tuple *showclock_t = dict_find(iterator, MESSAGE_KEY_SHOWCLOCK);
  if (showclock_t) {
    settings.ShowTime = showclock_t->value->int32 == 1;
  }

  Tuple *showdate_t = dict_find(iterator, MESSAGE_KEY_SHOWDATE);
  if (showdate_t) {
    settings.ShowDate = showdate_t->value->int32 == 1;
  }
  Tuple *showsteps_t = dict_find(iterator, MESSAGE_KEY_SHOWSTEPS);
  if (showsteps_t) {
    settings.ShowSteps = showsteps_t->value->int32 == 1;
  }
  Tuple *showweather_t = dict_find(iterator, MESSAGE_KEY_SHOWWEATHER);
  if (showweather_t) {
    settings.ShowWeather = showweather_t->value->int32 == 1;
  }
  Tuple *showbattery_t = dict_find(iterator, MESSAGE_KEY_SHOWBATTERY);
  if (showbattery_t) {
    settings.ShowBattery = showbattery_t->value->int32 == 1;
  }

  // Save and apply if any settings were changed
  if (showclock_t || showdate_t || showsteps_t || showweather_t || showbattery_t) {
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
  window_set_background_color(s_main_window,GColorBabyBlueEyes);
  // Set handlers to manage the elements inside the Window
  window_set_window_handlers(s_main_window, (WindowHandlers) {
    .load = main_window_load,
    .unload = main_window_unload
  });
  s_bitmap_backdrop = gbitmap_create_with_resource(RESOURCE_ID_CLOCK);
  s_bitmap_foot = gbitmap_create_with_resource(RESOURCE_ID_RUNFOOT);
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
  grab_clock(0);

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
