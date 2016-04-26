#include <pebble.h>

static Window *window;
static TextLayer *help_layer; //TITLE on each screen
static TextLayer *summary_layer;
static TextLayer *lowbtn_layer;
static TextLayer *highbtn_layer;
static TextLayer *hourmin_layer; //used by checkin to select HOUR : MIN

#define SUMMARYSCREEN 0
#define CHECKINSCREEN 1 //Start TIME
#define WORKHOURSSCREEN 2 //How many hours you work
#define BREAKSCREEN 3 //Break Duration Minutes

#define NSTAGE 4
static int stage = SUMMARYSCREEN;
static int startHours = -1;
static int startMinutes = -1;
static int workHours = 0;
static int workMinutes = 0;
static int breakMinutes = 0;
static int breakHours = 0;
static bool checkInSet = false;
static bool workHourSet = false;
static bool breakSet = false;
static int timerId = 0;
#define s_timer 0
#define s_startH 1
#define s_startM 2
#define s_workH 3
#define s_workM 4 
#define s_breakH 5
#define s_breakM 6

static char TIME[10];
static char NOWTIME[20];
static const char* const Titles[] = { 
  "Estimated Exit","Check In","Work Time","Break Dur.",
};

static const char* const Numbers[] = { 
  "00","01","02","03","04","05","06","07","08","09",
  "10","11","12","13","14","15","16","17","18","19",
  "20","21","22","23","24","25","26","27","28","29",
  "30","31","32","33","34","35","36","37","38","39",
  "40","41","42","43","44","45","46","47","48","49",
  "50","51","52","53","54","55","56","57","58","59"
};

//effective seconds to count
static int calcTimer(){
  int hh = workHours + breakHours;
  int mm = workMinutes + breakMinutes;
  
  return (mm + (hh * 60)) * 60; 
}

static void saveAndSet() { 

  int todoTime = calcTimer();
  time_t rawtime;
  struct tm *timeinfo;
  time ( &rawtime );
  timeinfo = localtime ( &rawtime );
  timeinfo->tm_hour = startHours;
  timeinfo->tm_min = startMinutes;
  timeinfo->tm_sec = 0;
  time_t whenWakeUp = mktime ( timeinfo ) + todoTime;
  //before.. time_t whenWakeUp = time(NULL) + todoTime;
  
  //!!! delete all the timers
  wakeup_cancel_all();
  timerId = wakeup_schedule(whenWakeUp, 0, true);
  //!!! set timer..
  persist_write_int(s_timer,timerId);
  persist_write_int(s_startH,startHours);  
  persist_write_int(s_startM,startMinutes); 
  persist_write_int(s_workH,workHours);
  persist_write_int(s_workM,workMinutes);
  persist_write_int(s_breakH,breakMinutes);
  persist_write_int(s_breakM,breakHours);
}

static void trashAll() {
  startHours = -1;
  startMinutes = -1;
  workHours = 0;
  workMinutes = 0;
  breakMinutes = 0;
  breakHours = 0;
  checkInSet = false;
  workHourSet = false;
  breakSet = false;
  persist_delete(s_timer);
  wakeup_cancel_all();
}

static void wakeup_handler(WakeupId id, int32_t reason) {
  // The app has woken!
  trashAll();
}


static bool loadAll(){
  if ( persist_exists(s_timer) ) { //if this is set.. I'm sure the other are also set
      timerId = persist_read_int(s_timer);
      startHours = persist_read_int(s_startH);  
      startMinutes = persist_read_int(s_startM); 
      workHours = persist_read_int(s_workH);
      workMinutes = persist_read_int(s_workM);
      breakMinutes = persist_read_int(s_breakH);
      breakHours = persist_read_int(s_breakM);
      checkInSet = true;
      workHourSet = true;
      breakSet = true;
      return true;
  }
  return false;
}

static void estimateWhen() {
  TIME[0] = '\0';
  int tmpH = startHours + breakHours + workHours;
  int tmpM = startMinutes + breakMinutes + workMinutes;
  if( tmpM > 60 ) {
    tmpH = tmpH + (tmpM/60); //hours
    tmpM = tmpM % 60; //minutes
  }
  
  if(tmpH >= 24 ) {
    tmpH = tmpH - 24;
  }
  strcat(TIME, Numbers[tmpH]);
  strcat(TIME, " : ");
  strcat(TIME, Numbers[tmpM]);
  text_layer_set_text(summary_layer, TIME );
}

static void getHourFromStrTime(char *ntime, int *hour,int *min) {
  // input example: 9:30 or 14:02 
  int i;
  int ch[4];
  int p = 0;
  for(i=0;ntime[i]!='\0';i++) {    
    if( ntime[i] != ':') {
      ch[p] = ntime[i] - '0';
      p++;
    }
  }
  if (p == 4) {
    *hour = ch[0]*10 + ch[1];
    *min = ch[2]*10 + ch[3];
  }
   
  if (p == 3) {
    *hour = ch[0];
    *min = ch[1]*10 + ch[2];
  }
}

//set the first configuration screen to now h/m
static void upTime(int hour, int minutes) {  

  int tmpHours = startHours;
  int tmpMinutes = startMinutes;

  switch (stage){
    //case SUMMARYSCREEN:
    case CHECKINSCREEN:
    break;
    case WORKHOURSSCREEN:
      tmpHours = workHours;
      tmpMinutes = workMinutes;
    break;
    case BREAKSCREEN:
      tmpHours = breakHours;
      tmpMinutes = breakMinutes;
    break;
    default:  
      APP_LOG(APP_LOG_LEVEL_DEBUG, "STATE INCONSISTENT!");
      return;
  }
  
  if(tmpHours + hour > 23 ) {
    tmpHours = 0;
  } else {
    tmpHours += hour;
  }
  
  if(tmpMinutes + minutes > 59 ) {
    tmpMinutes = 0;
  } else {
    tmpMinutes += minutes;    
  }
  TIME[0] = '\0';
  strcat(TIME, Numbers[tmpHours]);
  strcat(TIME, " : ");
  strcat(TIME, Numbers[tmpMinutes]);
  text_layer_set_text(hourmin_layer, TIME );
  
  switch (stage){
    case CHECKINSCREEN:
      startHours = tmpHours;
      startMinutes = tmpMinutes;
      break;
    case WORKHOURSSCREEN:
      workHours = tmpHours;
      workMinutes = tmpMinutes;
      break;
    case BREAKSCREEN:
      breakHours = tmpHours;
      breakMinutes = tmpMinutes;
    break;
    default:
      break;
  }

}

//set the first configuration screen to now h/m
static void startScreenToNow() {
  clock_copy_time_string(NOWTIME,20);
  int hh=0,mm=0;
  APP_LOG(APP_LOG_LEVEL_DEBUG, "1c");
  getHourFromStrTime(NOWTIME,&hh,&mm);
  //upTime(hh,mm);
  APP_LOG(APP_LOG_LEVEL_DEBUG, "1e");
  startHours = hh;
  startMinutes = mm;
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Time is %s %d %d", NOWTIME,hh,mm);
}

static void commonTextLayer(TextLayer *l, const char *msg) {
  text_layer_set_text(l, msg);
  text_layer_set_text_alignment(l, GTextAlignmentCenter);
  text_layer_set_font(l, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
}

static void commonTextLayerRight(TextLayer *l, const char *msg) {
  text_layer_set_text(l, msg);
  text_layer_set_text_alignment(l, GTextAlignmentRight);
  text_layer_set_font(l, fonts_get_system_font(FONT_KEY_GOTHIC_24));
}

void setMe(){
  if( !checkInSet ) {
      startScreenToNow();
      checkInSet = true;  
  }
}

static void nextStage() {
  //check the current stage and pass to the next..
  if(stage == SUMMARYSCREEN) {     //current layer
    layer_remove_from_parent(text_layer_get_layer(summary_layer));
    //layer_remove_from_parent(text_layer_get_layer(lowbtn_layer));
    layer_add_child(window_get_root_layer(window), text_layer_get_layer(hourmin_layer));
  } else if( stage == CHECKINSCREEN ) {
    checkInSet = true;
    if( !workHourSet ) {
      workHours = 8;
      workMinutes = 0;
      text_layer_set_text(hourmin_layer, "08:00" );//default working hours
    }    
  }  else if( stage == WORKHOURSSCREEN ) {
    workHourSet = true;
    if( !breakSet )  {
      breakHours = 0;
      breakMinutes = 0;
      text_layer_set_text(hourmin_layer, "00:00" );//default working hours
    }
  } else if( stage == BREAKSCREEN ) {
    breakSet = true;
    saveAndSet();
    estimateWhen();
    layer_remove_from_parent(text_layer_get_layer(hourmin_layer));
    layer_add_child(window_get_root_layer(window), text_layer_get_layer(summary_layer));
    //layer_add_child(window_get_root_layer(window), text_layer_get_layer(reset_layer));
  }
  stage ++;
  if(stage == NSTAGE) {
    stage = 0;
  }
  if(stage != SUMMARYSCREEN) {
        setMe();
  }
  upTime(0,0);

  text_layer_set_text(help_layer, Titles[stage]);

  if( stage == SUMMARYSCREEN ) {
    text_layer_set_text(lowbtn_layer, "RST");
    text_layer_set_text(highbtn_layer, "");
  } else {
    text_layer_set_text(highbtn_layer, "Ho+");
    text_layer_set_text(lowbtn_layer, "Mi+");
  }
}



static void select_click_handler(ClickRecognizerRef recognizer, void *context) {
  nextStage();
}

static void up_click_handler(ClickRecognizerRef recognizer, void *context) {
  if(stage != SUMMARYSCREEN)
    upTime(1,0);
}

static void down_click_handler(ClickRecognizerRef recognizer, void *context) {
  if(stage != SUMMARYSCREEN)
    upTime(0,1);
  else {
    commonTextLayer(summary_layer, "Not Set");
    trashAll(); //!!!! da modificare .. deve essere segnalato
  }
}



void pressed_up_handler(ClickRecognizerRef recognizer, void *context) {
  //tick_timer_service_subscribe(TimeUnits tick_units, TickHandler handler)
}

void released_up_handler(ClickRecognizerRef recognizer, void *context) {
  
}

static void click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, select_click_handler);
  //window_long_click_subscribe(BUTTON_ID_UP, 0, pressed_up_handler , released_up_handler);
  window_single_click_subscribe(BUTTON_ID_UP, up_click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, down_click_handler);
}

static void window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);
  APP_LOG(APP_LOG_LEVEL_DEBUG, "L1");
  summary_layer = text_layer_create((GRect) { .origin = { 0, 88 }, .size = { bounds.size.w, 30 } });
  lowbtn_layer = text_layer_create((GRect) { .origin = { bounds.size.w - 45 ,bounds.size.h - 35 }, .size = { 40, 30 } });
  highbtn_layer = text_layer_create((GRect) { .origin = { bounds.size.w - 45 , 5 }, .size = { 40, 30 } });
  hourmin_layer = text_layer_create((GRect) { .origin = { 0, 88 }, .size = { bounds.size.w, 30 } });
  help_layer = text_layer_create((GRect) { .origin={ 0, 30 }, .size = { bounds.size.w, 30} });
  APP_LOG(APP_LOG_LEVEL_DEBUG, "L2");
  commonTextLayer(hourmin_layer, "--:--");
  commonTextLayer(help_layer, Titles[0]);
  commonTextLayerRight(lowbtn_layer,"RST");
  commonTextLayerRight(highbtn_layer,"");
  if(!loadAll())
      commonTextLayer(summary_layer, "Not Set");
  else{
      commonTextLayer(summary_layer, "");
      estimateWhen();
  }
  
  wakeup_service_subscribe(wakeup_handler);
  if (launch_reason() == APP_LAUNCH_WAKEUP) {
    // The app was started by a wakeup
    commonTextLayer(summary_layer, "You did it!");
    vibes_double_pulse();
    trashAll();
  }
  
  layer_add_child(window_layer, text_layer_get_layer(summary_layer));
  layer_add_child(window_layer, text_layer_get_layer(help_layer));
  layer_add_child(window_layer, text_layer_get_layer(lowbtn_layer)); 
  layer_add_child(window_layer, text_layer_get_layer(highbtn_layer)); 
  APP_LOG(APP_LOG_LEVEL_DEBUG, "L6");
}

static void window_unload(Window *window) {
  text_layer_destroy(summary_layer);
  text_layer_destroy(hourmin_layer);
  text_layer_destroy(help_layer);
  text_layer_destroy(lowbtn_layer);
  text_layer_destroy(highbtn_layer);
}

static void init(void) {
  window = window_create();
  window_set_click_config_provider(window, click_config_provider);
  window_set_window_handlers(window, (WindowHandlers) {
	  .load = window_load,
    .unload = window_unload,
  });
  const bool animated = true;
  window_stack_push(window, animated);
}

static void deinit(void) {
  window_destroy(window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}