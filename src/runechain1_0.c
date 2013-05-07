#include "pebble_os.h"
#include "pebble_app.h"
#include "pebble_fonts.h"


#define MY_UUID { 0xF0, 0xAB, 0x45, 0x6E, 0xB3, 0xDC, 0x42, 0x93, 0x94, 0x06, 0xBB, 0x72, 0xF0, 0xD4, 0x57, 0xF5 }
PBL_APP_INFO(MY_UUID,
             "Runechain", "Glenn Loos-Austin",
             1, 0, /* App version */
             RESOURCE_ID_MENU_ICON,
             APP_INFO_WATCH_FACE);

Window _window;
AppTimerHandle timer_handle;
AppContextRef _storedctx;

Layer _topFrame;
Layer _bottomFrame;
Layer _clockFrame;
InverterLayer _invertit;

#define CHAIN_SIZE 9
BmpContainer _topRunes[CHAIN_SIZE];
BmpContainer _bottomRunes[CHAIN_SIZE];
BmpContainer _clockA[4];
BmpContainer _clockB[4];
BmpContainer _colon3;
BmpContainer _colon4;

PropertyAnimation _digitA_anim[4];
PropertyAnimation _digitB_anim[4];

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

void layer_init_and_add(Layer *newlayer, GRect rect, Layer *parentLayer, bool clipping) {
    layer_init(newlayer,rect);
    layer_add_child(parentLayer,newlayer);
    layer_set_clips(newlayer,clipping);
}

void remove_container(BmpContainer *bmp_container) {
    layer_remove_from_parent(&bmp_container->layer.layer);            //remove it from layer so it can be safely deinited
    bmp_deinit_container(bmp_container);                              //deinit the old image.
}

void set_container_image(BmpContainer *bmp_container, const int resource_id, GPoint origin, Layer *targetLayer) {
    
    remove_container(bmp_container);
    
    bmp_init_container(resource_id, bmp_container);                   //init the container with the new image
    
    GRect frame = layer_get_frame(&bmp_container->layer.layer);       //posiiton the new image with the supplied coordinates.
    frame.origin.x = origin.x;
    frame.origin.y = origin.y;
    layer_set_frame(&bmp_container->layer.layer, frame);

    layer_add_child(targetLayer, &bmp_container->layer.layer);        //add the new image to the target layer.
}

void bmp_set_position(BmpContainer *bmp,GRect newloc) {
    layer_set_frame(&bmp->layer.layer,newloc);
}

void bmp_set_y_position(BmpContainer *bmp,int yPos) {
    GRect rect = layer_get_frame(&bmp->layer.layer);
    rect.origin.y = yPos;
    bmp_set_position(bmp,rect);
}

void bmp_shift_position(BmpContainer *bmp,int xoffset,int yoffset) {
    GRect rect = layer_get_frame(&bmp->layer.layer);
    rect.origin.y += yoffset;
    rect.origin.x += xoffset; 
    bmp_set_position(bmp,rect);
}

void layer_shift_position(Layer *layer,int xoffset, int yoffset) {
    GRect rect = layer_get_frame(layer);
    rect.origin.y += yoffset;
    rect.origin.x += xoffset;
    layer_set_frame(layer,rect);
}

long get_seconds()
{
    PblTm t;
    get_time(&t);
    
    // Convert time to seconds since epoch.
    return t.tm_sec	// start seconds
    + t.tm_min*60 // add minutes
    + t.tm_hour*3600 // add hours
    + t.tm_yday*86400 // add days
    + (t.tm_year-70)*31536000 // add years since 1970
    + ((t.tm_year-69)/4)*86400 // add a day after leap years, starting in 1973
    - ((t.tm_year-1)/100)*86400 // remove a leap day every 100 years, starting in 2001
    + ((t.tm_year+299)/400)*86400; // add a leap day back every 400 years, starting in 2001
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
        return hour;
    }
    
    unsigned short display_hour = hour % 12;
    
    // Converts "0" to "12"
    return display_hour ? display_hour : 12;
    
}

void clock_compute(bool upcomingTime) {
    PblTm current_time;
    get_time(&current_time);
    
    unsigned short display_hour = get_display_hour(current_time.tm_hour);
    _clockNeededDigit[0] = display_hour/10;
    _clockNeededDigit[1] = display_hour%10;
    _clockNeededDigit[2] = current_time.tm_min/10;
    _clockNeededDigit[3] = current_time.tm_min%10;
    
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

void jumpstart_animation(AppContextRef ctx) {
    clock_compute(true); //figure out original digits to display.
    _animStage = 1;
    _animSecond = 53;
    timer_handle = app_timer_send_event(ctx, 100 /* milliseconds */, 800);
}

void chain_move() {
    for(int i=0;i<CHAIN_SIZE;i++) {
        bmp_shift_position(&_topRunes[i],2,0);
        bmp_shift_position(&_bottomRunes[i],-2,0);
        GRect rect = layer_get_frame(&_bottomRunes[i].layer.layer);
        if (rect.origin.x < -30)
        {
            clock_compute(true);
            for (int j=0;j<10;j++) {
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

            set_container_image(&_bottomRunes[i], ID[_chainBase+_runeList_bottom[i]], GPoint(rect.origin.x+CHAIN_SIZE*22,0), &_bottomFrame);
            _bottomRunes[i].layer.compositing_mode = GCompOpClear;
            
            rect = layer_get_frame(&_topRunes[CHAIN_SIZE-1-i].layer.layer);
            set_container_image(&_topRunes[CHAIN_SIZE-1-i], ID[_chainBase+_runeList_top[CHAIN_SIZE-1-i]], GPoint(rect.origin.x-CHAIN_SIZE*22,0), &_topFrame);
            _topRunes[CHAIN_SIZE-1-i].layer.compositing_mode = GCompOpClear;
            
            if(_chainBase == -1) {
                layer_set_hidden(&_bottomRunes[i].layer.layer,true);
                layer_set_hidden(&_topRunes[CHAIN_SIZE-1-i].layer.layer,true);
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
        rect = layer_get_frame(&_bottomRunes[i].layer.layer);
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
            if(_runeList_bottom[i] > -1) set_container_image(&_bottomRunes[i], ID[_usebase+_runeList_bottom[i]], GPoint(rect.origin.x,0), &_bottomFrame);
            rect = layer_get_frame(&_topRunes[i].layer.layer);
            if(_runeList_top[i] > -1) set_container_image(&_topRunes[i], ID[_usebase+_runeList_top[i]], GPoint(rect.origin.x,0), &_topFrame);
            layer_set_hidden(&_bottomRunes[i].layer.layer,false);
            layer_set_hidden(&_topRunes[i].layer.layer,false);
        }
        else
        {
            layer_set_hidden(&_bottomRunes[i].layer.layer,true);
            layer_set_hidden(&_topRunes[i].layer.layer,true);
        }
        
        _topRunes[i].layer.compositing_mode = GCompOpClear;
        _bottomRunes[i].layer.compositing_mode = GCompOpClear;
    }
}

//
// End Runechain Clock functions
//




//////////////////////////////////
// Click Handler Functions      //
//////////////////////////////////

/*
void up_click_handler(ClickRecognizerRef recognizer, Window *window) {
     //chain_update(BASE);
    jumpstart_animation(_storedctx);
}

void down_click_handler(ClickRecognizerRef recognizer, Window *window) {
    //chain_update(FAINT);
}

void select_click_handler(ClickRecognizerRef recognizer, Window *window) {
    // for(int i=0;i<CHAIN_SIZE;i++) {
       // bmp_shift_position(&_topRunes[i],0,2);
       // bmp_shift_position(&_bottomRunes[i],0,-2);
    //}
    layer_shift_position(&_topFrame,0,2);
    layer_shift_position(&_bottomFrame,0,-2);
}

void main_config_provider(ClickConfig **config, Window *window) {
    config[BUTTON_ID_UP]->click.handler = (ClickHandler) up_click_handler;
    config[BUTTON_ID_DOWN]->click.handler = (ClickHandler) down_click_handler;
    config[BUTTON_ID_SELECT]->click.handler = (ClickHandler) select_click_handler;
    
    (void)window;
}
*/

//
// End Click Handler Functions
//



void handle_timer(AppContextRef ctx, AppTimerHandle handle, uint32_t countdown) {
    (void)ctx;
    (void)handle;
    countdown--;
    chain_move();
    if(_animStage>0) {
        if (countdown>50) {
            countdown = (countdown * 8)/10;
        } else {
            countdown = 50;
        }
        timer_handle = app_timer_send_event(ctx, countdown /* milliseconds */, countdown);
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

void handle_second_tick(AppContextRef ctx, PebbleTickEvent *t) {
    (void)ctx;
    unsigned short display_second = t->tick_time->tm_sec;
    if(_animStage == 0) {
        chain_move();
       // if (display_second == 10) {
       //     chain_update(FAINTFLICKER);
       // }
       // if (display_second == 15) {
       //     chain_update(FAINT);
       // }
        if (display_second == 53) {
            jumpstart_animation(ctx);
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
                    
                    property_animation_init_layer_frame(&_digitA_anim[i], &_clockA[i].layer.layer, NULL, &to_rect1);
                    property_animation_init_layer_frame(&_digitB_anim[i], &_clockB[i].layer.layer, NULL, &to_rect2);
                    
                    animation_set_duration(&_digitA_anim[i].animation, 1400+random(1000));
                    animation_set_duration(&_digitB_anim[i].animation, 1400+random(1000));
                    animation_set_curve(&_digitA_anim[i].animation,AnimationCurveEaseIn);
                    animation_set_curve(&_digitB_anim[i].animation,AnimationCurveEaseIn);
                    animation_schedule(&_digitA_anim[i].animation);
                    animation_schedule(&_digitB_anim[i].animation);
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
                            aPoint = layer_get_frame(&_topRunes[j].layer.layer).origin;
                            aPoint.y += 4; //convert to global y coordinates.
                            layer_set_frame(&_topRunes[j].layer.layer,GRect(aPoint.x,-60,22,40));
                            aPoint.x += 2; //adjust for motion.
                            _runeList_top[j] = -1;
                            break;
                        } else if (_runeList_bottom[j] == _clockNeededDigit[i]) {
                            aPoint = layer_get_frame(&_bottomRunes[j].layer.layer).origin;
                            aPoint.y += 124; //convert to global y coordinates.
                            layer_set_frame(&_bottomRunes[j].layer.layer,GRect(aPoint.x,60,22,40));
                            aPoint.x -= 2; //adjust for motion.
                            _runeList_bottom[j] = -1;
                            break;
                        }
                    }
                    for(int j=0;j<CHAIN_SIZE;j++) {
                        if (_runeList_bottom[j] == 10+_clockNeededDigit[i]) {
                            bPoint = layer_get_frame(&_bottomRunes[j].layer.layer).origin;
                            bPoint.y += 124; //convert to global y coordinates.
                            layer_set_frame(&_bottomRunes[j].layer.layer,GRect(bPoint.x,60,22,40));
                            bPoint.x -= 2; //adjust for motion.
                           _runeList_bottom[j] = -1;
                            break;
                        } else if (_runeList_top[j] == 10+_clockNeededDigit[i]) {
                            bPoint = layer_get_frame(&_topRunes[j].layer.layer).origin;
                            bPoint.y += 4; //convert to global y coordinates.
                            layer_set_frame(&_topRunes[j].layer.layer,GRect(bPoint.x,-60,22,40));
                            bPoint.x += 2; //adjust for motion.
                            _runeList_top[j] = -1;
                            break;
                        }
                    }
                                        
                    set_container_image(&_clockA[i], ID[BASE+_clockNeededDigit[i]], aPoint, &_clockFrame);
                    set_container_image(&_clockB[i], ID[BASEB+_clockNeededDigit[i]], bPoint, &_clockFrame);
                    _clockA[i].layer.compositing_mode = GCompOpClear;
                    _clockB[i].layer.compositing_mode = GCompOpClear;

                    GRect to_rect = _clockAtarget4[i];

                    if (_clockStart == 1) {
                        to_rect = _clockAtarget3[i];
                        layer_set_hidden(&_colon3.layer.layer,false);
                        layer_set_hidden(&_colon4.layer.layer,true);
                    } else {
                        layer_set_hidden(&_colon3.layer.layer,true);
                        layer_set_hidden(&_colon4.layer.layer,false);
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

                    property_animation_init_layer_frame(&_digitA_anim[i], &_clockA[i].layer.layer, NULL, &to_rect);
                    property_animation_init_layer_frame(&_digitB_anim[i], &_clockB[i].layer.layer, NULL, &to_rect);
                    
                    animation_set_duration(&_digitA_anim[i].animation, 2500+random(1000));
                    animation_set_duration(&_digitB_anim[i].animation, 2500+random(1000));
                    animation_set_curve(&_digitA_anim[i].animation,AnimationCurveEaseInOut);
                    animation_set_curve(&_digitB_anim[i].animation,AnimationCurveEaseInOut);
                    animation_schedule(&_digitA_anim[i].animation);
                    animation_schedule(&_digitB_anim[i].animation);

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
    //timer_handle = app_timer_send_event(ctx, 100 /* milliseconds */, 9);
}

void handle_deinit(AppContextRef ctx) {
    (void)ctx;
    for(int i=0;i<CHAIN_SIZE;i++) {
        remove_container(&_topRunes[i]);
        remove_container(&_bottomRunes[i]);
    }
    for(int i=0;i<4;i++) {
        remove_container(&_clockA[i]);
        remove_container(&_clockB[i]);
    }
    remove_container(&_colon3);
    remove_container(&_colon4);
}

void handle_init(AppContextRef ctx) {
    (void)ctx;
    
    _storedctx = ctx;
    
    window_init(&_window, "Runechain");
    window_stack_push(&_window, true /* Animated */);
    window_set_background_color(&_window, GColorWhite);
    window_set_fullscreen(&_window, true);

    _seed = get_seconds(); //for random.

    resource_init_current_app(&APP_RESOURCES);
    
    //window_set_click_config_provider(&_window, (ClickConfigProvider) main_config_provider);

    layer_init_and_add(&_topFrame,GRect(0, 4, 144, 40),&_window.layer,true);
    layer_init_and_add(&_bottomFrame,GRect(0, 124, 144, 40),&_window.layer,true);
    layer_init_and_add(&_clockFrame,GRect(0, 0, 144, 168),&_window.layer,true);
    
    for(int i=0;i<CHAIN_SIZE;i++) {
        _runeList_top[i] = random(20)-1;
        _runeList_bottom[i] = random(20)-1;

        set_container_image(&_topRunes[i], ID[_chainBase+_runeList_top[i]], GPoint(-4+i*22,0), &_topFrame);// &_window.layer);
        set_container_image(&_bottomRunes[i], ID[_chainBase+_runeList_bottom[i]], GPoint(-60+i*22,0), &_bottomFrame);// &_window.layer);
        _topRunes[i].layer.compositing_mode = GCompOpClear;
        _bottomRunes[i].layer.compositing_mode = GCompOpClear;
    }
    
    for (int i=0;i<4;i++) {
        set_container_image(&_clockA[i], ID[BASE+3], _clockAtarget4[i].origin, &_clockFrame);
        set_container_image(&_clockB[i], ID[BASE+13], _clockAtarget4[i].origin, &_clockFrame);
        _clockA[i].layer.compositing_mode = GCompOpClear;
        _clockB[i].layer.compositing_mode = GCompOpClear;
        layer_set_hidden(&_clockA[i].layer.layer,true);
        layer_set_hidden(&_clockB[i].layer.layer,true);
    }

    inverter_layer_init(&_invertit,GRect(0, 0, 144, 168));
    layer_add_child(&_window.layer,&_invertit.layer);
    
    set_container_image(&_colon3, RESOURCE_ID_COLON, GPoint(_clockAtarget3[2].origin.x-5,CLOCK_Y+12), &_window.layer);
    set_container_image(&_colon4, RESOURCE_ID_COLON, GPoint(_clockAtarget4[2].origin.x-5,CLOCK_Y+12), &_window.layer);
    layer_set_hidden(&_colon3.layer.layer,true);
    layer_set_hidden(&_colon4.layer.layer,true);

    jumpstart_animation(ctx);
}


void pbl_main(void *params) {
  PebbleAppHandlers handlers = {
    .init_handler = &handle_init,
      .deinit_handler = &handle_deinit,
      .timer_handler = &handle_timer,
      .tick_info = {
          .tick_handler = &handle_second_tick,
          .tick_units = SECOND_UNIT
      }
  };
  app_event_loop(params, &handlers);
}
