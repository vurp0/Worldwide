#include <pebble.h>

#define TIME_OFFSET_KEY 1
#define DEFAULT_TIME_OFFSET 0

static Window *s_window;
static GBitmap *s_background_bitmap;
static BitmapLayer *s_background_layer;
static GBitmap *s_cursor_bitmap;
static BitmapLayer *s_cursor_layer;

static int cursor_x_position = 0;

/* Warning: the watchface doesn't work properly in the CloudPebble emulator.
   This is because the emulated Pebble uses the computer's local time, but
   the JS part of the app doesn't report the correct timezone in the CloudPebble emulator. */
static int time_offset = DEFAULT_TIME_OFFSET; // The offset from UTC in minutes. Comes directly from JS: Date.prototype.getTimezoneOffset()

static void check_time() {
  // The following three lines are maybe the worst I've ever written.
  time_t current_time = time(NULL) + (time_offset*60) + 43200; // basically, take the current UTC time and shift it by 12 hours
  current_time = current_time % 86400; // mod length of a day
  cursor_x_position = 142-((current_time/608+2)%142); // current time of day in seconds divided by 608 gives a value between 0 and approx. 168.
                                                      // The +2 is there because my world map is about two pixels off
  layer_set_frame(bitmap_layer_get_layer(s_cursor_layer), GRect(cursor_x_position, 0, 3, 168));
  layer_mark_dirty(bitmap_layer_get_layer(s_cursor_layer));
}

static void inbox_received_callback(DictionaryIterator *iterator, void *context) {
  //completely typical inbox received callback
  Tuple *t = dict_read_first(iterator);

  while (t != NULL) {
    switch (t->key) {
      case TIME_OFFSET_KEY:
        //APP_LOG(APP_LOG_LEVEL_INFO, "Received time zone offset: %d", (int)(t->value->int32)); // debug message
        time_offset = (int)(t->value->int32);
        check_time();
        break;
    }
    t = dict_read_next(iterator);
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

static void tick_handler(struct tm *tick_time, TimeUnits units_changes) {
  check_time();
}

static void window_load(Window *window) {
  s_background_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_WORLD_BACKGROUND);
  s_cursor_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_CURSOR);
  s_background_layer = bitmap_layer_create(GRect(0,0,144,168));
  s_cursor_layer = bitmap_layer_create(GRect(cursor_x_position,0,3,168));
  bitmap_layer_set_bitmap(s_background_layer, s_background_bitmap);
  bitmap_layer_set_bitmap(s_cursor_layer, s_cursor_bitmap);
  layer_add_child(window_get_root_layer(window), bitmap_layer_get_layer(s_background_layer));
  layer_add_child(window_get_root_layer(window), bitmap_layer_get_layer(s_cursor_layer));
}

static void window_unload(Window *window) {
  gbitmap_destroy(s_background_bitmap);
  gbitmap_destroy(s_cursor_bitmap);
  bitmap_layer_destroy(s_background_layer);
  bitmap_layer_destroy(s_cursor_layer);
}

static void init() {
  s_window = window_create();
  
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload
  });
  
  window_stack_push(s_window, true);
  
  app_message_register_inbox_received(inbox_received_callback);
  app_message_register_inbox_dropped(inbox_dropped_callback);
  app_message_register_outbox_failed(outbox_failed_callback);
  app_message_register_outbox_sent(outbox_sent_callback);
  
  app_message_open(app_message_inbox_size_maximum(), app_message_outbox_size_maximum());
  
  tick_timer_service_subscribe(MINUTE_UNIT, (TickHandler)tick_handler);
  
  // see if the timezone has been cached
  if (persist_exists(TIME_OFFSET_KEY)) {
    time_offset = persist_read_int(TIME_OFFSET_KEY);
  }
  
  check_time();
}

static void deinit() {
  // write the timezone to persistent storage as a cache
  // time_offset could have been modified through an AppMessage while the app was running
  persist_write_int(TIME_OFFSET_KEY, time_offset);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}