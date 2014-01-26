#include "pebble.h"

// If compiled with IS_APP, a few button/click handlers will be enabled to aid in debugging.
// (Also change watchapp/watchface in appinfo.json to 'false'. (watchfaces override the handlers.))
//#define IS_APP
static void handle_timer();

typedef struct {
  BitmapLayer *bitmaplayer;
  GBitmap *bitmap;
} BitmapContainer; 

static Window *_window;
static Layer *_windowLayer;

static AppTimer *timer_handle;

Layer *_topFrame;
Layer *_bottomFrame;
Layer *_clockFrame;
InverterLayer *_invertit;

static uint32_t countdown;

#define CHAIN_SIZE 9
BitmapContainer _topRunes[CHAIN_SIZE];
BitmapContainer _bottomRunes[CHAIN_SIZE];
BitmapContainer _clockA[4];
BitmapContainer _clockB[4];
BitmapContainer _colon3;
BitmapContainer _colon4;

PropertyAnimation *_digitA_anim[4];
PropertyAnimation *_digitB_anim[4];

int _clockNeededDigit[4];
int _clockNeededRunes[20];
//int _clockBneededIndex[4];
#define CLOCK_X 24
#define CLOCK_Y 64
GRect _clockAtarget4[] = {{{CLOCK_X,CLOCK_Y},{22,40}},{{CLOCK_X+22,CLOCK_Y},{22,40}},{{CLOCK_X+51,CLOCK_Y},{22,40}},{{CLOCK_X+73,CLOCK_Y},{22,40}}};
GRect _clockAtarget3[] = {{{-100,-100},{22,40}},{{CLOCK_X+10,CLOCK_Y},{22,40}},{{CLOCK_X+39,CLOCK_Y},{22,40}},{{CLOCK_X+61,CLOCK_Y},{22,40}}};
int _clockStart = 0;

int _animStage = 0;
int _animSecond = 0;

int _runeList_top[CHAIN_SIZE];
int _runeList_bottom[CHAIN_SIZE];

static long _seed;

#define BASE 0
#define BASEB 10
#define FAINT 20
#define FAINTB 30

#define OFF -1
#define FLICKER -2
#define FAINTFLICKER -3

int _chainBase = FAINT;

const int ID[] = {
    RESOURCE_ID_RUNE_A0,
    RESOURCE_ID_RUNE_A1,
    RESOURCE_ID_RUNE_A2,
    RESOURCE_ID_RUNE_A3,
    RESOURCE_ID_RUNE_A4,
    RESOURCE_ID_RUNE_A5,
    RESOURCE_ID_RUNE_A6,
    RESOURCE_ID_RUNE_A7,
    RESOURCE_ID_RUNE_A8,
    RESOURCE_ID_RUNE_A9,
    RESOURCE_ID_RUNE_B0,
    RESOURCE_ID_RUNE_B1,
    RESOURCE_ID_RUNE_B2,
    RESOURCE_ID_RUNE_B3,
    RESOURCE_ID_RUNE_B4,
    RESOURCE_ID_RUNE_B5,
    RESOURCE_ID_RUNE_B6,
    RESOURCE_ID_RUNE_B7,
    RESOURCE_ID_RUNE_B8,
    RESOURCE_ID_RUNE_B9,
    RESOURCE_ID_FAINT_A0,
    RESOURCE_ID_FAINT_A1,
    RESOURCE_ID_FAINT_A2,
    RESOURCE_ID_FAINT_A3,
    RESOURCE_ID_FAINT_A4,
    RESOURCE_ID_FAINT_A5,
    RESOURCE_ID_FAINT_A6,
    RESOURCE_ID_FAINT_A7,
    RESOURCE_ID_FAINT_A8,
    RESOURCE_ID_FAINT_A9,
    RESOURCE_ID_FAINT_B0,
    RESOURCE_ID_FAINT_B1,
    RESOURCE_ID_FAINT_B2,
    RESOURCE_ID_FAINT_B3,
    RESOURCE_ID_FAINT_B4,
    RESOURCE_ID_FAINT_B5,
    RESOURCE_ID_FAINT_B6,
    RESOURCE_ID_FAINT_B7,
    RESOURCE_ID_FAINT_B8,
    RESOURCE_ID_FAINT_B9
};


//////////////////////////////////
// Pebble API Utility Functions //
//////////////////////////////////

void layer_create_and_add(Layer **newlayer, GRect rect, Layer *parentLayer, bool clipping) {
    *newlayer=layer_create(rect);
    layer_add_child(parentLayer,*newlayer);
    layer_set_clips(*newlayer,clipping);
}

void remove_container(BitmapContainer *bmp_container) {
    layer_remove_from_parent(bitmap_layer_get_layer(bmp_container->bitmaplayer));            //remove it from layer so it can be safely deinited
    bitmap_layer_destroy(bmp_container->bitmaplayer);                                        //deinit the old image.
    gbitmap_destroy(bmp_container->bitmap); 
}

void set_container_image(BitmapContainer *bmp_container, const int resource_id, GPoint origin, Layer *targetLayer) {
    
    if (bmp_container->bitmaplayer != NULL) { remove_container(bmp_container); }

    bmp_container->bitmap = gbitmap_create_with_resource(resource_id);                       // setup bitmap and it's layer

#ifdef DEBUG
    if (bmp_container->bitmap==NULL) {
      APP_LOG(APP_LOG_LEVEL_DEBUG, "ResourceID:%d",resource_id); 
      APP_LOG(APP_LOG_LEVEL_DEBUG, "BITMAP NULL");
    } 
#endif // DEBUG    

    GRect frame = (GRect) { .origin = origin, .size = bmp_container->bitmap->bounds.size };
    bmp_container->bitmaplayer=bitmap_layer_create(frame);
    
    bitmap_layer_set_bitmap(bmp_container->bitmaplayer, bmp_container->bitmap);              //init the container with the new image
    layer_add_child(targetLayer, bitmap_layer_get_layer(bmp_container->bitmaplayer));        //add the new image to the target layer.
}

void bmp_set_position(BitmapContainer bmp_container, GRect newloc) {
  layer_set_frame(bitmap_layer_get_layer(bmp_container.bitmaplayer),newloc);
}

void bmp_set_y_position(BitmapContainer bmp_container, int yPos) {
    GRect rect = layer_get_frame(bitmap_layer_get_layer(bmp_container.bitmaplayer));
    rect.origin.y = yPos;
    bmp_set_position(bmp_container,rect);
}

void bmp_shift_position(BitmapContainer bmp_container, int xoffset, int yoffset) {
    GRect rect = layer_get_frame(bitmap_layer_get_layer(bmp_container.bitmaplayer));
    rect.origin.y += yoffset;
    rect.origin.x += xoffset; 
    bmp_set_position(bmp_container,rect);
}

void layer_shift_position(Layer *layer, int xoffset, int yoffset) {
    GRect rect = layer_get_frame(layer);
    rect.origin.y += yoffset;
    rect.origin.x += xoffset;
    layer_set_frame(layer,rect);
}


//
// End Pebble API Utility Functions.
//



//////////////////////////////////
// C Utility Functions          //
//////////////////////////////////

int random(int max)
{
    //  random code thanks to FlashBIOS's forum post on the topic
    //  http://forums.getpebble.com/discussion/comment/28908/#Comment_28908
    
    _seed = (((_seed * 214013L + 2531011L) >> 16) & 32767);
    
    return ((_seed % max) + 1);
}

//
// End C Utility Functions
//



//////////////////////////////////
// Runechain Clock Functions    //
//////////////////////////////////

unsigned short get_display_hour(unsigned short hour) {
    
    if (clock_is_24h_style()) {
        return (hour);
    }
    
    unsigned short display_hour = hour % 12;
    
    // Converts "0" to "12"
    return(display_hour ? display_hour : 12);
    
}

void clock_compute(bool upcomingTime) {
    time_t now=time(NULL);
    struct tm *current_time = localtime(&now);

    unsigned short display_hour = get_display_hour(current_time->tm_hour);
    _clockNeededDigit[0] = display_hour/10;
    _clockNeededDigit[1] = display_hour%10;
    _clockNeededDigit[2] = current_time->tm_min/10;
    _clockNeededDigit[3] = current_time->tm_min%10;
    
    if (upcomingTime) {
        //let's look at the _next_ minute's requirements, instead of this minute.
        _clockNeededDigit[3]++;
        if (_clockNeededDigit[3]==10) {
            _clockNeededDigit[3]=0;
            _clockNeededDigit[2]++;
            if(_clockNeededDigit[2]==6) {
                _clockNeededDigit[2]=0;
                _clockNeededDigit[1]++;
                if(_clockNeededDigit[1]==10) {
                    _clockNeededDigit[1] = 0;
                    //obviously not complete, but we have a way to animate non-present digits anyway.
                }
            }
        }
    }
    _clockStart = 0;
    if (_clockNeededDigit[0] == 0 ) _clockStart = 1;
    
    for (int i=0;i<20;i++) {
        _clockNeededRunes[i] = 0;
    }
    for(int i=_clockStart;i<4;i++) {
        _clockNeededRunes[_clockNeededDigit[i]] ++;
        _clockNeededRunes[10+_clockNeededDigit[i]] ++;
    }
}

void jumpstart_animation() {
    clock_compute(true); //figure out original digits to display.
    _animStage = 1;
    _animSecond = 53;
    countdown=800;
    timer_handle = app_timer_register(100 /* milliseconds */, handle_timer, NULL);
}

void chain_move() {
    for(int i=0;i<CHAIN_SIZE;i++) {
        bmp_shift_position(_topRunes[i],2,0);
        bmp_shift_position(_bottomRunes[i],-2,0);
        GRect rect = layer_get_frame(bitmap_layer_get_layer(_bottomRunes[i].bitmaplayer));

        if (rect.origin.x < -30)
        {
            clock_compute(true);
            for (int j=0;j<CHAIN_SIZE;j++) {
                if(_runeList_top[j] > -1) _clockNeededRunes[_runeList_top[j]]--;
                if(_runeList_bottom[j] > -1) _clockNeededRunes[_runeList_bottom[j]]--;
            }
            
            int new_rune[] = {random(20)-1,random(20)-1};
            int forced_rune_index = 0;
            
            for (int j=0;j<20;j++) {
                if(_clockNeededRunes[j] > 0) {
                    new_rune[forced_rune_index] = j;
                    forced_rune_index++;
                    if (forced_rune_index > 1) break;
                }
            }
            clock_compute(true);
            
            _runeList_top[CHAIN_SIZE-1-i] = new_rune[0];
            _runeList_bottom[i] = new_rune[1];

            if(_chainBase > -1) {
	      set_container_image(&_bottomRunes[i], ID[_chainBase+_runeList_bottom[i]], GPoint(rect.origin.x+CHAIN_SIZE*22,0), _bottomFrame);
	      bitmap_layer_set_compositing_mode(_bottomRunes[i].bitmaplayer, GCompOpClear);
            
	      rect = layer_get_frame(bitmap_layer_get_layer(_topRunes[CHAIN_SIZE-1-i].bitmaplayer));
	      set_container_image(&_topRunes[CHAIN_SIZE-1-i], ID[_chainBase+_runeList_top[CHAIN_SIZE-1-i]], GPoint(rect.origin.x-CHAIN_SIZE*22,0), _topFrame);
	      bitmap_layer_set_compositing_mode(_topRunes[CHAIN_SIZE-1-i].bitmaplayer, GCompOpClear);
            }else{
	      layer_set_hidden(bitmap_layer_get_layer(_bottomRunes[i].bitmaplayer),true);
	      layer_set_hidden(bitmap_layer_get_layer(_topRunes[CHAIN_SIZE-1-i].bitmaplayer),true);
            }
        }
    }
}

//#define OFF -1
//#define FLICKER -2
//#define FAINTFLICKER -3


void chain_update(int newBase) {
    _chainBase = newBase;
    int _usebase;
    
    GRect rect;
    for(int i=0;i<CHAIN_SIZE;i++) {
      rect = layer_get_frame(bitmap_layer_get_layer(_bottomRunes[i].bitmaplayer));
        switch (_chainBase) {
            case -2:
                if (random(2) == 1) {
                    _usebase = BASE;
                } else {
                    _usebase = FAINT;
                }
                break;
            case -3:
                if (random(2) == 1) {
                    _usebase = FAINT;
                } else {
                    _usebase = -1;                    
                }
                break;
            default:
                _usebase = _chainBase;
        }
        
        if(_usebase > -1)
        {
  	    if(_runeList_bottom[i] > -1) {set_container_image(&_bottomRunes[i], ID[_usebase+_runeList_bottom[i]], GPoint(rect.origin.x,0), _bottomFrame);}
            rect = layer_get_frame(bitmap_layer_get_layer(_topRunes[i].bitmaplayer));
            if(_runeList_top[i] > -1) {set_container_image(&_topRunes[i], ID[_usebase+_runeList_top[i]], GPoint(rect.origin.x,0), _topFrame);}
            layer_set_hidden(bitmap_layer_get_layer(_bottomRunes[i].bitmaplayer),false);
            layer_set_hidden(bitmap_layer_get_layer(_topRunes[i].bitmaplayer),false);
        }
        else
        {
	    layer_set_hidden(bitmap_layer_get_layer(_bottomRunes[i].bitmaplayer),true);
	    layer_set_hidden(bitmap_layer_get_layer(_topRunes[i].bitmaplayer),true);
        }
        
        bitmap_layer_set_compositing_mode(_topRunes[i].bitmaplayer, GCompOpClear);
        bitmap_layer_set_compositing_mode(_bottomRunes[i].bitmaplayer, GCompOpClear);
    }
}

//
// End Runechain Clock functions
//




#ifdef IS_APP
//////////////////////////////////
// Click Handler Functions      //
//////////////////////////////////

void up_click_handler(ClickRecognizerRef recognizer, Window *window) {
    chain_update(BASE);
    jumpstart_animation();
}

void down_click_handler(ClickRecognizerRef recognizer, Window *window) {
    chain_update(FAINT);
}

void select_click_handler(ClickRecognizerRef recognizer, Window *window) {
    for(int i=0;i<CHAIN_SIZE;i++) {
        bmp_shift_position(_topRunes[i],0,2);
        bmp_shift_position(_bottomRunes[i],0,-2);
    }
    layer_shift_position(_topFrame,0,2);
    layer_shift_position(_bottomFrame,0,-2);
}

void main_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, (ClickHandler)select_click_handler);
  window_single_click_subscribe(BUTTON_ID_UP, (ClickHandler)up_click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, (ClickHandler)down_click_handler);
}

//
// End Click Handler Functions
//
#endif // IS_APP

static void handle_timer() {
    countdown--;
    chain_move();
    if(_animStage>0) {
        if (countdown>50) {
            countdown = (countdown * 8)/10;
        } else {
            countdown = 50;
        }
	timer_handle = app_timer_register(countdown /* milliseconds */, handle_timer, NULL);
        if(_chainBase == -2 || _chainBase == -3) {
            chain_update(_chainBase);
        }
    }
}

// Original animation plan: (Not exactly what's currently happening.)
// stage 1, 54 seconds, accelerate to fast rune speed, dim out existing time if present.
// stage 2, 57 seconds, flicker brighter runes, complete remove existing time.
// stage 3, 58 seconds, dim all runes, animate bright runes to final locations
// stage 4, 59 seconds, wiggle in place while flashing.
// stage 5, 00 seconds, bright flash of final time, return to rest state (eventually: dim chain runes to nothing?)
// stage 6, 02 seconds, return faint chain runes set _animstate back to 0;

static void handle_second_tick(struct tm *tick_time, TimeUnits units_changed) {
    unsigned short display_second = tick_time->tm_sec;

    if(_animStage == 0) {
        chain_move();
       // if (display_second == 10) {
       //     chain_update(FAINTFLICKER);
       // }
       // if (display_second == 15) {
       //     chain_update(FAINT);
       // }
        if (display_second == 53) {
            jumpstart_animation();
        }
    } else
    {
        _animSecond++;
        switch (_animSecond) {
            case 52: //shouldn't happen
            case 53: //shouldn't happen
            case 54: //speeding up.
            case 55: //speeding up.
                break;
            case 56:
                for (int i=0;i<4;i++) {
                    GRect to_rect1 = GRect(-30,-20+random(208),22,40);
                    GRect to_rect2 = GRect(150,-20+random(208),22,40);
                    
		    if (_digitA_anim[i] != NULL) {property_animation_destroy(_digitA_anim[i]);}
		    if (_digitB_anim[i] != NULL) {property_animation_destroy(_digitB_anim[i]);}
                    _digitA_anim[i]=property_animation_create_layer_frame(bitmap_layer_get_layer(_clockA[i].bitmaplayer), NULL, &to_rect1);
                    _digitB_anim[i]=property_animation_create_layer_frame(bitmap_layer_get_layer(_clockB[i].bitmaplayer), NULL, &to_rect2);
                    
                    animation_set_duration(&_digitA_anim[i]->animation, 1400+random(1000));
                    animation_set_duration(&_digitB_anim[i]->animation, 1400+random(1000));
                    animation_set_curve(&_digitA_anim[i]->animation, AnimationCurveEaseIn);
                    animation_set_curve(&_digitB_anim[i]->animation, AnimationCurveEaseIn);
                    animation_schedule(&_digitA_anim[i]->animation);
                    animation_schedule(&_digitB_anim[i]->animation);
                }
                break;
            case 57:
                break;
            case 58:
                _animStage = 2;
                chain_update(FLICKER);
            case 59:
                _animStage = 3;
                chain_update(FAINT);
                break;
            case 60:
                _animStage = 4;
                clock_compute(false); //do the actual time this time.
                for (int i=_clockStart;i<4;i++) {
                    GPoint aPoint = GPoint(-30,random(98)+15);
                    GPoint bPoint = GPoint(150,random(98)+15);
                    for(int j=CHAIN_SIZE-1;j>=0;j--) {
                        if (_runeList_top[j] == _clockNeededDigit[i]) {
			    aPoint = layer_get_frame(bitmap_layer_get_layer(_topRunes[j].bitmaplayer)).origin;
                            aPoint.y += 4; //convert to global y coordinates.
                            layer_set_frame(bitmap_layer_get_layer(_topRunes[j].bitmaplayer),GRect(aPoint.x,-60,22,40));
                            aPoint.x += 2; //adjust for motion.
                            _runeList_top[j] = -1;
                            break;
                        } else if (_runeList_bottom[j] == _clockNeededDigit[i]) {
			    aPoint = layer_get_frame(bitmap_layer_get_layer(_bottomRunes[j].bitmaplayer)).origin;
                            aPoint.y += 124; //convert to global y coordinates.
                            layer_set_frame(bitmap_layer_get_layer(_bottomRunes[j].bitmaplayer),GRect(aPoint.x,60,22,40));
                            aPoint.x -= 2; //adjust for motion.
                            _runeList_bottom[j] = -1;
                            break;
                        }
                    }
                    for(int j=0;j<CHAIN_SIZE;j++) {
                        if (_runeList_bottom[j] == 10+_clockNeededDigit[i]) {
			  bPoint = layer_get_frame(bitmap_layer_get_layer(_bottomRunes[j].bitmaplayer)).origin;
                            bPoint.y += 124; //convert to global y coordinates.
                            layer_set_frame(bitmap_layer_get_layer(_bottomRunes[j].bitmaplayer),GRect(bPoint.x,60,22,40));
                            bPoint.x -= 2; //adjust for motion.
                           _runeList_bottom[j] = -1;
                            break;
                        } else if (_runeList_top[j] == 10+_clockNeededDigit[i]) {
			  bPoint = layer_get_frame(bitmap_layer_get_layer(_topRunes[j].bitmaplayer)).origin;
                            bPoint.y += 4; //convert to global y coordinates.
                            layer_set_frame(bitmap_layer_get_layer(_topRunes[j].bitmaplayer),GRect(bPoint.x,-60,22,40));
                            bPoint.x += 2; //adjust for motion.
                            _runeList_top[j] = -1;
                            break;
                        }
                    }
                                        
                    set_container_image(&_clockA[i], ID[BASE+_clockNeededDigit[i]], aPoint, _clockFrame);
                    set_container_image(&_clockB[i], ID[BASEB+_clockNeededDigit[i]], bPoint, _clockFrame);
                    bitmap_layer_set_compositing_mode(_clockA[i].bitmaplayer, GCompOpClear);
                    bitmap_layer_set_compositing_mode(_clockB[i].bitmaplayer, GCompOpClear);

                    GRect to_rect = _clockAtarget4[i];

                    if (_clockStart == 1) {
                        to_rect = _clockAtarget3[i];
                        layer_set_hidden(bitmap_layer_get_layer(_colon3.bitmaplayer),false);
                        layer_set_hidden(bitmap_layer_get_layer(_colon4.bitmaplayer),true);
                    } else {
		        layer_set_hidden(bitmap_layer_get_layer(_colon3.bitmaplayer),true);
		        layer_set_hidden(bitmap_layer_get_layer(_colon4.bitmaplayer),false);
                    }
                    
#define ONE_KERN 4
                    if (_clockNeededDigit[i] == 1) {
                        switch (i) {
                            case 0:
                            case 1:
                                to_rect.origin.x += ONE_KERN;
                                break;
                            case 2:
                            case 3:
                                to_rect.origin.x -= ONE_KERN;
                                break;
                        }
                    }
                    if (i == 3 && _clockNeededDigit[2] == 1) {
                        to_rect.origin.x -= ONE_KERN;
                    }
                    if (i == 0 && _clockNeededDigit[1] == 1) {
                        to_rect.origin.x += ONE_KERN;
                    }

		    if (_digitA_anim[i] != NULL) {property_animation_destroy(_digitA_anim[i]);}
		    if (_digitB_anim[i] != NULL) {property_animation_destroy(_digitB_anim[i]);}
                    _digitA_anim[i]=property_animation_create_layer_frame(bitmap_layer_get_layer(_clockA[i].bitmaplayer), NULL, &to_rect);
                    _digitB_anim[i]=property_animation_create_layer_frame(bitmap_layer_get_layer(_clockB[i].bitmaplayer), NULL, &to_rect);
                    
                    animation_set_duration(&_digitA_anim[i]->animation, 2500+random(1000));
                    animation_set_duration(&_digitB_anim[i]->animation, 2500+random(1000));
                    animation_set_curve(&_digitA_anim[i]->animation,AnimationCurveEaseInOut);
                    animation_set_curve(&_digitB_anim[i]->animation,AnimationCurveEaseInOut);
                    animation_schedule(&_digitA_anim[i]->animation);
                    animation_schedule(&_digitB_anim[i]->animation);

                }
                chain_update(FAINT);
                break;
            case 61:
                _animStage = 5;
                chain_update(FAINT);
                break;
            case 62:
                break;
            case 63:
                _animStage = 6;
                chain_update(FAINTFLICKER);
                break;
            case 64:
            case 65:
                chain_update(FAINT);
                _animStage = 0;
                break;
                
            default:
                break;
        }
    }
    //timer_handle = app_timer_register(100 /* milliseconds */, handle_timer, NULL); // 9
}

static void handle_deinit() {
    tick_timer_service_unsubscribe();
    app_timer_cancel(timer_handle);

    for (int i=0;i<4;i++) {
      if (_digitA_anim[i] != NULL) {property_animation_destroy(_digitA_anim[i]);}
      if (_digitB_anim[i] != NULL) {property_animation_destroy(_digitB_anim[i]);}
    }

    layer_remove_from_parent(inverter_layer_get_layer(_invertit));
    inverter_layer_destroy(_invertit);

    remove_container(&_colon4);
    remove_container(&_colon3);

    for(int i=0;i<4;i++) {
        remove_container(&_clockA[i]);
        remove_container(&_clockB[i]);
    }

    for(int i=0;i<CHAIN_SIZE;i++) {
        remove_container(&_topRunes[i]);
        remove_container(&_bottomRunes[i]);
    }

    layer_remove_from_parent(_clockFrame);
    layer_destroy(_clockFrame);
    layer_remove_from_parent(_bottomFrame);
    layer_destroy(_bottomFrame);
    layer_remove_from_parent(_topFrame);
    layer_destroy(_topFrame);

    window_destroy(_window);
}

static void handle_init() {
    _window = window_create();
    _windowLayer = window_get_root_layer(_window);
    window_stack_push(_window, true /* Animated */);
    window_set_background_color(_window, GColorWhite);
    window_set_fullscreen(_window, true);

    _seed = time(NULL);

#ifdef IS_APP
    // Define some event handlers for clicks
    window_set_click_config_provider(_window, (ClickConfigProvider)main_config_provider);
#endif // IS_APP

    layer_create_and_add(&_topFrame,GRect(0, 4, 144, 40),_windowLayer, true);
    layer_create_and_add(&_bottomFrame,GRect(0, 124, 144, 40),_windowLayer, true);
    layer_create_and_add(&_clockFrame,GRect(0, 0, 144, 168),_windowLayer, true);
    
    for(int i=0;i<CHAIN_SIZE;i++) {
        _runeList_top[i] = random(20)-1;
        _runeList_bottom[i] = random(20)-1;

        set_container_image(&_topRunes[i], ID[_chainBase+_runeList_top[i]], GPoint(-4+i*22,0), _topFrame);
        set_container_image(&_bottomRunes[i], ID[_chainBase+_runeList_bottom[i]], GPoint(-60+i*22,0), _bottomFrame);
        bitmap_layer_set_compositing_mode(_topRunes[i].bitmaplayer, GCompOpClear);
        bitmap_layer_set_compositing_mode(_bottomRunes[i].bitmaplayer, GCompOpClear);
    }
    
    for (int i=0;i<4;i++) {
        set_container_image(&_clockA[i], ID[BASE+3], _clockAtarget4[i].origin, _clockFrame);
        set_container_image(&_clockB[i], ID[BASE+13], _clockAtarget4[i].origin, _clockFrame);
        bitmap_layer_set_compositing_mode(_clockA[i].bitmaplayer, GCompOpClear);
        bitmap_layer_set_compositing_mode(_clockB[i].bitmaplayer, GCompOpClear);

        layer_set_hidden(bitmap_layer_get_layer(_clockA[i].bitmaplayer),true);
        layer_set_hidden(bitmap_layer_get_layer(_clockB[i].bitmaplayer),true);
    }

    _invertit=inverter_layer_create(GRect(0, 0, 144, 168));
    layer_add_child(_windowLayer,inverter_layer_get_layer(_invertit));
    
    set_container_image(&_colon3, RESOURCE_ID_COLON, GPoint(_clockAtarget3[2].origin.x-5,CLOCK_Y+12), _windowLayer);
    set_container_image(&_colon4, RESOURCE_ID_COLON, GPoint(_clockAtarget4[2].origin.x-5,CLOCK_Y+12), _windowLayer);
    layer_set_hidden(bitmap_layer_get_layer(_colon3.bitmaplayer),true);
    layer_set_hidden(bitmap_layer_get_layer(_colon4.bitmaplayer),true);

    jumpstart_animation();
    tick_timer_service_subscribe(SECOND_UNIT, handle_second_tick);
}

int main(void) {
  handle_init();
  app_event_loop();
  handle_deinit();
}
