#include <pebble.h>

/* =========================================================================
 * AURORA - Weather watchface
 * Gestapelte Zeit (Stunden weiss / Minuten gruen), rechts Datumsspalte,
 * Wetter-Icon + Min/Max, unten zentriert Schritte.
 * Zielplattformen: Gabbro (Pebble Round 2, 260x260) und Chalk (180x180).
 * ========================================================================= */

/* ---------- DEBUG ----------
 * Auskommentiert = Release. Aktiviert:
 *  - Wetter-Icons laufen im Sekundentakt durch (Groessen-Check)
 *  - Schrittzahl fest auf 20000
 *  - Hoechsttemperatur fest auf -23
 */
#define DEBUG_MODE 

/* ---------- COLORS ---------- */
#define COLOR_BG        GColorBlack
#define COLOR_DATE      GColorWhite
#define COLOR_ICON      GColorWhite
#define COLOR_TEMP      GColorWhite
#define COLOR_STEPS_LBL GColorWhite
#define COLOR_STEPS_VAL GColorWhite

/* Default-Farben (per Clay konfigurierbar) */
#define DEFAULT_HOUR_HEX 0xFFFFFF        /* weiss */
#define DEFAULT_MIN_HEX  0x00FFAA        /* GColorMediumSpringGreen */

/* ---------- FONTS + LAYOUT pro Plattform ---------- */
#if defined(PBL_PLATFORM_CHALK)
  #define FONT_DATE  FONT_KEY_GOTHIC_18_BOLD
  #define FONT_TEMP  FONT_KEY_GOTHIC_18_BOLD
  #define FONT_SLBL  FONT_KEY_GOTHIC_14
  #define FONT_SVAL  FONT_KEY_GOTHIC_18_BOLD

  #define HOUR_BOX   GRect(20,  22, 104, 48)
  #define MIN_BOX    GRect(20,  74, 104, 48)

  #define DOW_BOX    GRect(125, 15, 68, 22)
  #define DAY_BOX    GRect(135, 35, 68, 22)
  #define MON_BOX    GRect(135, 55, 68, 22)

  #define ICON_CX    135
  #define ICON_CY    100
  #define ICON_SCALE 1

  #define LOW_BOX    GRect(90, 110, 70, 22)
  #define HIGH_BOX   GRect(90, 130, 70, 22)
  #define TEMP_ALIGN GTextAlignmentRight

  #define SLBL_BOX   GRect(0, 142, 180, 18)
  #define SVAL_BOX   GRect(0, 152, 180, 22)
#else
  /* Gabbro (Pebble Round 2) - Default */
  #define FONT_DATE  FONT_KEY_GOTHIC_28_BOLD
  #define FONT_TEMP  FONT_KEY_GOTHIC_24_BOLD
  #define FONT_SLBL  FONT_KEY_GOTHIC_24
  #define FONT_SVAL  FONT_KEY_GOTHIC_28_BOLD

  #define HOUR_BOX   GRect(24,  35, 150, 66)
  #define MIN_BOX    GRect(24, 109, 150, 66)

  #define DOW_BOX    GRect(180, 20, 96, 32)
  #define DAY_BOX    GRect(200, 50, 96, 32)
  #define MON_BOX    GRect(200, 80, 96, 32)

  #define ICON_CX    192
  #define ICON_CY    140
  #define ICON_SCALE 1

  #define LOW_BOX    GRect(150, 156, 78, 30)
  #define HIGH_BOX   GRect(150, 184, 78, 30)
  #define TEMP_ALIGN GTextAlignmentRight

  #define SLBL_BOX   GRect(0, 204, 260, 22)
  #define SVAL_BOX   GRect(0, 222, 260, 30)
#endif

#if defined(PBL_PLATFORM_CHALK)
  #define RES_TIME RESOURCE_ID_EUROSTILE_DEMI_64
#else
  #define RES_TIME RESOURCE_ID_EUROSTILE_DEMI_92
#endif

/* ---------- WEATHER ICON TYPES ---------- */
#define WX_CLEAR   0
#define WX_PARTLY  1
#define WX_CLOUD   2
#define WX_RAIN    3
#define WX_SNOW    4
#define WX_THUNDER 5
#define WX_FOG     6
#define WX_COUNT   7

/* ---------- LANGUAGES ---------- */
#define LANG_EN    0
#define LANG_DE    1
#define LANG_FR    2
#define LANG_ES    3
#define LANG_IT    4
#define LANG_NL    5
#define LANG_COUNT 6

/* ---------- PERSIST KEYS ---------- */
#define PK_ICON 1
#define PK_HIGH 2
#define PK_LOW  3
#define PK_HCOL 4
#define PK_MCOL 5
#define PK_LANG 6

/* ---------- STATE ---------- */
static Window  *s_window;
static Layer   *s_canvas;

static char s_hour[4];
static char s_min[4];
static char s_dow[6];
static char s_day[4];
static char s_mon[6];
static char s_steps[8];
static char s_low[8];
static char s_high[8];

static int  s_weather_icon = WX_PARTLY;
static int  s_weather_high = 0;
static int  s_weather_low  = 0;
static bool s_have_weather = false;

static int  s_hour_hex = DEFAULT_HOUR_HEX;
static int  s_min_hex  = DEFAULT_MIN_HEX;

static int  s_lang = LANG_EN;

static GFont s_font_time;

/* Wochentage 2-stellig, Index = tm_wday (0 = Sonntag) */
static const char *DOW_TBL[LANG_COUNT][7] = {
  {"Su","Mo","Tu","We","Th","Fr","Sa"},       /* EN */
  {"So","Mo","Di","Mi","Do","Fr","Sa"},       /* DE */
  {"Di","Lu","Ma","Me","Je","Ve","Sa"},       /* FR */
  {"Do","Lu","Ma","Mi","Ju","Vi","S\u00E1"},  /* ES (Sa) */
  {"Do","Lu","Ma","Me","Gi","Ve","Sa"},       /* IT */
  {"Zo","Ma","Di","Wo","Do","Vr","Za"},       /* NL */
};

/* Monate 3-stellig, Index = tm_mon (0 = Januar) */
static const char *MON_TBL[LANG_COUNT][12] = {
  {"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"},                /* EN */
  {"Jan","Feb","M\u00E4r","Apr","Mai","Jun","Jul","Aug","Sep","Okt","Nov","Dez"},           /* DE (Mar) */
  {"Jan","F\u00E9v","Mar","Avr","Mai","Jun","Jul","Ao\u00FB","Sep","Oct","Nov","D\u00E9c"}, /* FR (Fev, Aou, Dec) */
  {"Ene","Feb","Mar","Abr","May","Jun","Jul","Ago","Sep","Oct","Nov","Dic"},                /* ES */
  {"Gen","Feb","Mar","Apr","Mag","Giu","Lug","Ago","Set","Ott","Nov","Dic"},                /* IT */
  {"Jan","Feb","Mrt","Apr","Mei","Jun","Jul","Aug","Sep","Okt","Nov","Dec"},                /* NL */
};

/* Schritte-Label je Sprache */
static const char *STEPS_TBL[LANG_COUNT] = {
  "Steps",     /* EN */
  "Schritte",  /* DE */
  "Pas",       /* FR */
  "Pasos",     /* ES */
  "Passi",     /* IT */
  "Stappen",   /* NL */
};

/* ========================================================================= */
/* Wetter-Icons (rein gezeichnet, keine Bitmap-Ressourcen)                   */
/* ========================================================================= */

static void draw_sun(GContext *ctx, GPoint c, int r, GColor color) {
  graphics_context_set_fill_color(ctx, color);
  graphics_context_set_stroke_color(ctx, color);
  graphics_context_set_stroke_width(ctx, 2);
  graphics_fill_circle(ctx, c, r);
  for (int i = 0; i < 8; i++) {
    int32_t a = TRIG_MAX_ANGLE * i / 8;
    int x1 = c.x + (r + 2) * sin_lookup(a) / TRIG_MAX_RATIO;
    int y1 = c.y - (r + 2) * cos_lookup(a) / TRIG_MAX_RATIO;
    int x2 = c.x + (r + 6) * sin_lookup(a) / TRIG_MAX_RATIO;
    int y2 = c.y - (r + 6) * cos_lookup(a) / TRIG_MAX_RATIO;
    graphics_draw_line(ctx, GPoint(x1, y1), GPoint(x2, y2));
  }
}

static void draw_cloud(GContext *ctx, GPoint c, GColor color) {
  graphics_context_set_fill_color(ctx, color);
  graphics_fill_circle(ctx, GPoint(c.x - 8, c.y + 1), 7);
  graphics_fill_circle(ctx, GPoint(c.x + 1, c.y - 5), 9);
  graphics_fill_circle(ctx, GPoint(c.x + 10, c.y + 1), 7);
  graphics_fill_rect(ctx, GRect(c.x - 14, c.y, 28, 9), 0, GCornerNone);
}

static void draw_drops(GContext *ctx, GPoint c, GColor color) {
  graphics_context_set_stroke_color(ctx, color);
  graphics_context_set_stroke_width(ctx, 2);
  for (int i = -1; i <= 1; i++) {
    int x = c.x + i * 8;
    graphics_draw_line(ctx, GPoint(x, c.y + 11), GPoint(x - 2, c.y + 17));
  }
}

static void draw_flakes(GContext *ctx, GPoint c, GColor color) {
  graphics_context_set_fill_color(ctx, color);
  for (int i = -1; i <= 1; i++) {
    graphics_fill_circle(ctx, GPoint(c.x + i * 8, c.y + 14), 2);
  }
}

static void draw_bolt(GContext *ctx, GPoint c, GColor color) {
  graphics_context_set_stroke_color(ctx, color);
  graphics_context_set_stroke_width(ctx, 3);
  graphics_draw_line(ctx, GPoint(c.x + 2, c.y + 9),  GPoint(c.x - 3, c.y + 15));
  graphics_draw_line(ctx, GPoint(c.x - 3, c.y + 15), GPoint(c.x + 1, c.y + 15));
  graphics_draw_line(ctx, GPoint(c.x + 1, c.y + 15), GPoint(c.x - 4, c.y + 21));
}

static void draw_fog(GContext *ctx, GPoint c, GColor color) {
  graphics_context_set_stroke_color(ctx, color);
  graphics_context_set_stroke_width(ctx, 2);
  for (int i = 0; i < 3; i++) {
    int y = c.y + 9 + i * 5;
    graphics_draw_line(ctx, GPoint(c.x - 12, y), GPoint(c.x + 12, y));
  }
}

static void draw_weather_icon(GContext *ctx, int type, GPoint c) {
  switch (type) {
    case WX_CLEAR:
      draw_sun(ctx, c, 9, COLOR_ICON);
      break;
    case WX_PARTLY:
      draw_sun(ctx, GPoint(c.x - 7, c.y - 7), 7, COLOR_ICON);
      draw_cloud(ctx, GPoint(c.x + 3, c.y + 5), COLOR_ICON);
      break;
    case WX_CLOUD:
      draw_cloud(ctx, c, COLOR_ICON);
      break;
    case WX_RAIN:
      draw_cloud(ctx, GPoint(c.x, c.y - 4), COLOR_ICON);
      draw_drops(ctx, GPoint(c.x, c.y - 4), COLOR_ICON);
      break;
    case WX_SNOW:
      draw_cloud(ctx, GPoint(c.x, c.y - 4), COLOR_ICON);
      draw_flakes(ctx, GPoint(c.x, c.y - 4), COLOR_ICON);
      break;
    case WX_THUNDER:
      draw_cloud(ctx, GPoint(c.x, c.y - 4), COLOR_ICON);
      draw_bolt(ctx, GPoint(c.x, c.y - 4), GColorYellow);
      break;
    case WX_FOG:
      draw_cloud(ctx, GPoint(c.x, c.y - 6), COLOR_ICON);
      draw_fog(ctx, GPoint(c.x, c.y - 6), COLOR_ICON);
      break;
    default:
      draw_cloud(ctx, c, COLOR_ICON);
      break;
  }
}

/* ========================================================================= */
/* Zeichnen                                                                  */
/* ========================================================================= */

static void canvas_update(Layer *layer, GContext *ctx) {
  GColor hour_col = GColorFromHEX(s_hour_hex);
  GColor min_col  = GColorFromHEX(s_min_hex);

  /* Zeit */
  graphics_context_set_text_color(ctx, hour_col);
  graphics_draw_text(ctx, s_hour, s_font_time,
                     HOUR_BOX, GTextOverflowModeFill, GTextAlignmentCenter, NULL);
  graphics_context_set_text_color(ctx, min_col);
  graphics_draw_text(ctx, s_min, s_font_time,
                     MIN_BOX, GTextOverflowModeFill, GTextAlignmentCenter, NULL);

  /* Datum */
  graphics_context_set_text_color(ctx, COLOR_DATE);
  graphics_draw_text(ctx, s_dow, fonts_get_system_font(FONT_DATE),
                     DOW_BOX, GTextOverflowModeFill, GTextAlignmentLeft, NULL);
  graphics_draw_text(ctx, s_day, fonts_get_system_font(FONT_DATE),
                     DAY_BOX, GTextOverflowModeFill, GTextAlignmentLeft, NULL);
  graphics_draw_text(ctx, s_mon, fonts_get_system_font(FONT_DATE),
                     MON_BOX, GTextOverflowModeFill, GTextAlignmentLeft, NULL);

  /* Wetter-Icon + Temperaturen */
  draw_weather_icon(ctx, s_weather_icon, GPoint(ICON_CX, ICON_CY));

  if (s_have_weather) {
    graphics_context_set_text_color(ctx, COLOR_TEMP);
    graphics_draw_text(ctx, s_low, fonts_get_system_font(FONT_TEMP),
                       LOW_BOX, GTextOverflowModeFill, TEMP_ALIGN, NULL);
    graphics_draw_text(ctx, s_high, fonts_get_system_font(FONT_TEMP),
                       HIGH_BOX, GTextOverflowModeFill, TEMP_ALIGN, NULL);
  }

  /* Schritte */
  graphics_context_set_text_color(ctx, COLOR_STEPS_LBL);
  graphics_draw_text(ctx, STEPS_TBL[s_lang], fonts_get_system_font(FONT_SLBL),
                     SLBL_BOX, GTextOverflowModeFill, GTextAlignmentCenter, NULL);
  graphics_context_set_text_color(ctx, COLOR_STEPS_VAL);
  graphics_draw_text(ctx, s_steps, fonts_get_system_font(FONT_SVAL),
                     SVAL_BOX, GTextOverflowModeFill, GTextAlignmentCenter, NULL);
}

/* ========================================================================= */
/* Daten aktualisieren                                                       */
/* ========================================================================= */

static int read_steps(void) {
#if defined(PBL_HEALTH)
  HealthServiceAccessibilityMask m =
      health_service_metric_accessible(HealthMetricStepCount,
                                        time_start_of_today(), time(NULL));
  if (m & HealthServiceAccessibilityMaskAvailable) {
    return (int)health_service_sum_today(HealthMetricStepCount);
  }
#endif
  return 0;
}

static void update_steps(void) {
  int steps = read_steps();
#ifdef DEBUG_MODE
  steps = 20000;
#endif
  snprintf(s_steps, sizeof(s_steps), "%d", steps);
}

static void update_clock(void) {
  time_t now = time(NULL);
  struct tm *t = localtime(&now);

  if (clock_is_24h_style()) {
    strftime(s_hour, sizeof(s_hour), "%H", t);
  } else {
    strftime(s_hour, sizeof(s_hour), "%I", t);
  }
  strftime(s_min, sizeof(s_min), "%M", t);

  snprintf(s_dow, sizeof(s_dow), "%s", DOW_TBL[s_lang][t->tm_wday]);
  snprintf(s_day, sizeof(s_day), "%02d", t->tm_mday);
  snprintf(s_mon, sizeof(s_mon), "%s", MON_TBL[s_lang][t->tm_mon]);
}

static void format_temps(void) {
  snprintf(s_low,  sizeof(s_low),  "%d\u00B0", s_weather_low);
  snprintf(s_high, sizeof(s_high), "%d\u00B0", s_weather_high);
}

/* ========================================================================= */
/* AppMessage                                                                */
/* ========================================================================= */

static void request_weather(void) {
  DictionaryIterator *iter;
  if (app_message_outbox_begin(&iter) == APP_MSG_OK) {
    dict_write_uint8(iter, MESSAGE_KEY_REQUEST_UPDATE, 1);
    app_message_outbox_send();
  }
}

static void inbox_received(DictionaryIterator *iter, void *context) {
  bool dirty = false;
  bool relang = false;
  Tuple *t;

  t = dict_find(iter, MESSAGE_KEY_WEATHER_ICON);
  if (t) {
    s_weather_icon = (int)t->value->int32;
    persist_write_int(PK_ICON, s_weather_icon);
    dirty = true;
  }
  t = dict_find(iter, MESSAGE_KEY_WEATHER_HIGH);
  if (t) {
    s_weather_high = (int)t->value->int32;
    persist_write_int(PK_HIGH, s_weather_high);
    s_have_weather = true;
    dirty = true;
  }
  t = dict_find(iter, MESSAGE_KEY_WEATHER_LOW);
  if (t) {
    s_weather_low = (int)t->value->int32;
    persist_write_int(PK_LOW, s_weather_low);
    s_have_weather = true;
    dirty = true;
  }
  t = dict_find(iter, MESSAGE_KEY_HOUR_COLOR);
  if (t) {
    s_hour_hex = (int)t->value->int32 & 0xFFFFFF;
    persist_write_int(PK_HCOL, s_hour_hex);
    dirty = true;
  }
  t = dict_find(iter, MESSAGE_KEY_MIN_COLOR);
  if (t) {
    s_min_hex = (int)t->value->int32 & 0xFFFFFF;
    persist_write_int(PK_MCOL, s_min_hex);
    dirty = true;
  }
  t = dict_find(iter, MESSAGE_KEY_LANG);
  if (t) {
    int lang;
    if (t->type == TUPLE_CSTRING) {
      lang = atoi(t->value->cstring);
    } else {
      lang = (int)t->value->int32;
    }
    if (lang < 0 || lang >= LANG_COUNT) {
      lang = LANG_EN;
    }
    s_lang = lang;
    persist_write_int(PK_LANG, s_lang);
    relang = true;
    dirty = true;
  }

  if (relang) {
    update_clock();
  }
  if (dirty) {
    format_temps();
    layer_mark_dirty(s_canvas);
  }
}

/* ========================================================================= */
/* Services                                                                  */
/* ========================================================================= */

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  update_clock();
  update_steps();

#ifdef DEBUG_MODE
  s_weather_icon = (s_weather_icon + 1) % WX_COUNT;
  s_weather_high = -23;
  s_have_weather = true;
  format_temps();
#endif

  layer_mark_dirty(s_canvas);

  if ((units_changed & MINUTE_UNIT) && (tick_time->tm_min % 30 == 0)) {
    request_weather();
  }
}

#if defined(PBL_HEALTH)
static void health_handler(HealthEventType event, void *context) {
  if (event == HealthEventMovementUpdate || event == HealthEventSignificantUpdate) {
    update_steps();
    layer_mark_dirty(s_canvas);
  }
}
#endif

static void bt_handler(bool connected) {
  if (connected) {
    request_weather();
  }
}

/* ========================================================================= */
/* Window                                                                    */
/* ========================================================================= */

static void window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);
  s_canvas = layer_create(bounds);
  layer_set_update_proc(s_canvas, canvas_update);
  layer_add_child(root, s_canvas);
}

static void window_unload(Window *window) {
  layer_destroy(s_canvas);
}

static void init(void) {
  /* Persistenz laden */
  s_weather_icon = persist_exists(PK_ICON) ? persist_read_int(PK_ICON) : WX_PARTLY;
  s_weather_high = persist_exists(PK_HIGH) ? persist_read_int(PK_HIGH) : 0;
  s_weather_low  = persist_exists(PK_LOW)  ? persist_read_int(PK_LOW)  : 0;
  s_have_weather = persist_exists(PK_HIGH);
  s_hour_hex     = persist_exists(PK_HCOL) ? persist_read_int(PK_HCOL) : DEFAULT_HOUR_HEX;
  s_min_hex      = persist_exists(PK_MCOL) ? persist_read_int(PK_MCOL) : DEFAULT_MIN_HEX;
  s_lang         = persist_exists(PK_LANG) ? persist_read_int(PK_LANG) : LANG_EN;
  if (s_lang < 0 || s_lang >= LANG_COUNT) {
    s_lang = LANG_EN;
  }

  s_font_time = fonts_load_custom_font(resource_get_handle(RES_TIME));

  update_clock();
  update_steps();
  format_temps();

  s_window = window_create();
  window_set_background_color(s_window, COLOR_BG);
  window_set_window_handlers(s_window, (WindowHandlers){
    .load = window_load,
    .unload = window_unload,
  });
  window_stack_push(s_window, true);

  app_message_register_inbox_received(inbox_received);
  app_message_open(128, 64);

#if defined(PBL_HEALTH)
  health_service_events_subscribe(health_handler, NULL);
#endif

  connection_service_subscribe((ConnectionHandlers){
    .pebble_app_connection_handler = bt_handler
  });

#ifdef DEBUG_MODE
  tick_timer_service_subscribe(SECOND_UNIT, tick_handler);
#else
  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
#endif
}

static void deinit(void) {
  fonts_unload_custom_font(s_font_time);
  tick_timer_service_unsubscribe();
  connection_service_unsubscribe();
#if defined(PBL_HEALTH)
  health_service_events_unsubscribe();
#endif
  window_destroy(s_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}