#include <furi.h>
#include <furi_hal_speaker.h>
#include <gui/gui.h>
#include <input/input.h>
#include <stdlib.h>

#define STG_COLUMNS 6
#define EMPTY_DIGIT 0xFF

typedef enum {
    StateTitle,
    StatePlaying,
    StateGameOver
} GameState;

typedef struct {
    GameState state;
    uint32_t score;
    uint32_t high_score;
    uint8_t stage;
    uint8_t part;
    uint8_t lives;
    uint8_t ammo;
    uint8_t turret;
    uint8_t field[STG_COLUMNS];
    uint8_t invaders_to_spawn;
    uint32_t kill_sum;
    bool ufo_pending;
    
    uint32_t tick_counter;
    uint32_t tick_interval;
} GameData;

typedef enum {
    EventTypeInput,
    EventTypeTick,
} EventType;

typedef struct {
    EventType type;
    union {
        InputEvent input;
    } data;
} AppEvent;

static bool speaker_acquired = false;
static uint32_t speaker_ticks = 0;

static void start_sound(float freq, uint32_t ticks) {
    if(!speaker_acquired) {
        if(furi_hal_speaker_acquire(10)) {
            speaker_acquired = true;
        }
    }
    if(speaker_acquired) {
        furi_hal_speaker_start(freq, 0.2f);
        speaker_ticks = ticks;
    }
}

static void update_sound() {
    if(speaker_acquired) {
        if(speaker_ticks > 0) {
            speaker_ticks--;
            if(speaker_ticks == 0) {
                furi_hal_speaker_stop();
                furi_hal_speaker_release();
                speaker_acquired = false;
            }
        }
    }
}

static void start_stage(GameData* game) {
    game->ammo = 30;
    game->lives = 3;
    for(int i = 0; i < STG_COLUMNS; i++) {
        game->field[i] = EMPTY_DIGIT;
    }
    game->invaders_to_spawn = 16;
    game->kill_sum = 0;
    game->ufo_pending = false;
    
    // Speed scaling (halved speed, so double the tick intervals)
    int speed = (22 - (game->stage * 2)) * 2;
    if(speed < 8) speed = 8;
    game->tick_interval = speed;
    game->tick_counter = 0;
    
    start_sound(1320.0f, 4); // High success beep
}

static void draw_segment(Canvas* canvas, int x, int y, int w, int h, uint8_t segments) {
    int mid_y = y + h / 2;
    
    // a (top)
    if(segments & (1 << 0)) {
        canvas_draw_line(canvas, x + 1, y, x + w - 2, y);
        canvas_draw_line(canvas, x + 1, y + 1, x + w - 2, y + 1);
    }
    // b (top-right)
    if(segments & (1 << 1)) {
        canvas_draw_line(canvas, x + w - 1, y + 1, x + w - 1, mid_y - 1);
        canvas_draw_line(canvas, x + w - 2, y + 1, x + w - 2, mid_y - 1);
    }
    // c (bottom-right)
    if(segments & (1 << 2)) {
        canvas_draw_line(canvas, x + w - 1, mid_y + 1, x + w - 1, y + h - 2);
        canvas_draw_line(canvas, x + w - 2, mid_y + 1, x + w - 2, y + h - 2);
    }
    // d (bottom)
    if(segments & (1 << 3)) {
        canvas_draw_line(canvas, x + 1, y + h - 1, x + w - 2, y + h - 1);
        canvas_draw_line(canvas, x + 1, y + h - 2, x + w - 2, y + h - 2);
    }
    // e (bottom-left)
    if(segments & (1 << 4)) {
        canvas_draw_line(canvas, x, mid_y + 1, x, y + h - 2);
        canvas_draw_line(canvas, x + 1, mid_y + 1, x + 1, y + h - 2);
    }
    // f (top-left)
    if(segments & (1 << 5)) {
        canvas_draw_line(canvas, x, y + 1, x, mid_y - 1);
        canvas_draw_line(canvas, x + 1, y + 1, x + 1, mid_y - 1);
    }
    // g (middle)
    if(segments & (1 << 6)) {
        canvas_draw_line(canvas, x + 1, mid_y, x + w - 2, mid_y);
        canvas_draw_line(canvas, x + 1, mid_y - 1, x + w - 2, mid_y - 1);
    }
}

static uint8_t get_digit_segments(uint8_t val) {
    if(val == EMPTY_DIGIT) return 0x00;
    
    static const uint8_t map[] = {
        0x3F, // '0': abcdef
        0x06, // '1': bc
        0x5B, // '2': abdeg
        0x4F, // '3': abcdg
        0x66, // '4': bcfg
        0x6D, // '5': acdfg
        0x7D, // '6': acdefg
        0x07, // '7': abc
        0x7F, // '8': abcdefg
        0x6F  // '9': abcdfg
    };
    
    if(val < 10) {
        return map[val];
    } else if(val == 10) {
        // 'n' (UFO)
        return 0x54; // c, e, g
    }
    
    return 0x00;
}

static uint8_t get_lives_segments(uint8_t lives) {
    if(lives == 3) return 0x49; // a, g, d (≡)
    if(lives == 2) return 0x09; // a, d   (二)
    if(lives == 1) return 0x40; // g      (ー)
    return 0x00;
}

static void draw_callback(Canvas* canvas, void* context) {
    GameData* game = context;
    if(!game) return;
    
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    
    if(game->state == StateTitle) {
        canvas_draw_rframe(canvas, 2, 2, 124, 60, 4);
        canvas_draw_rframe(canvas, 4, 4, 120, 56, 3);
        
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 16, AlignCenter, AlignCenter, "CALC INVADER");
        
        canvas_set_font(canvas, FontSecondary);
        char buf[32];
        snprintf(buf, sizeof(buf), "HIGH SCORE: %05lu", game->high_score);
        canvas_draw_str_aligned(canvas, 64, 32, AlignCenter, AlignCenter, buf);
        
        canvas_draw_str_aligned(canvas, 64, 48, AlignCenter, AlignCenter, "Press OK to Start");
    } else if(game->state == StateGameOver) {
        canvas_draw_rframe(canvas, 2, 2, 124, 60, 4);
        
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 16, AlignCenter, AlignCenter, "GAME OVER");
        
        canvas_set_font(canvas, FontSecondary);
        char buf[32];
        snprintf(buf, sizeof(buf), "FINAL SCORE: %05lu", game->score);
        canvas_draw_str_aligned(canvas, 64, 32, AlignCenter, AlignCenter, buf);
        
        canvas_draw_str_aligned(canvas, 64, 48, AlignCenter, AlignCenter, "Press OK to Restart");
    } else if(game->state == StatePlaying) {
        canvas_set_font(canvas, FontSecondary);
        char buf[32];
        snprintf(buf, sizeof(buf), "SCORE: %05lu", game->score);
        canvas_draw_str(canvas, 6, 11, buf);
        
        snprintf(buf, sizeof(buf), "HI: %05lu", game->high_score);
        canvas_draw_str_aligned(canvas, 122, 11, AlignRight, AlignBottom, buf);
        
        canvas_draw_line(canvas, 0, 13, 127, 13);
        canvas_draw_frame(canvas, 3, 15, 122, 28);
        
        int x_start = 5;
        int y_pos = 18;
        int digit_w = 12;
        int digit_h = 22;
        int gap = 3;
        
        // Digit 1: Turret
        uint8_t turret_segs = get_digit_segments(game->turret);
        draw_segment(canvas, x_start, y_pos, digit_w, digit_h, turret_segs);
        
        // Digit 2: Lives
        uint8_t lives_segs = get_lives_segments(game->lives);
        draw_segment(canvas, x_start + (digit_w + gap), y_pos, digit_w, digit_h, lives_segs);
        
        // Decimal point
        canvas_draw_box(canvas, x_start + 2 * (digit_w + gap) - 2, y_pos + digit_h - 2, 2, 2);
        
        // Digit 3-8: Field
        for(int i = 0; i < STG_COLUMNS; i++) {
            int dx = x_start + (i + 2) * (digit_w + gap);
            uint8_t segs = get_digit_segments(game->field[i]);
            draw_segment(canvas, dx, y_pos, digit_w, digit_h, segs);
        }
        
        snprintf(buf, sizeof(buf), "STG: %u-%u", game->part, game->stage);
        canvas_draw_str(canvas, 6, 54, buf);
        
        snprintf(buf, sizeof(buf), "BEAM: %u", game->ammo);
        canvas_draw_str_aligned(canvas, 122, 54, AlignRight, AlignBottom, buf);
        
        canvas_draw_str_aligned(canvas, 64, 63, AlignCenter, AlignBottom, "[D-Pad] Aim   [OK] Fire");
    }
}

static void input_callback(InputEvent* input_event, void* context) {
    FuriMessageQueue* event_queue = context;
    AppEvent event = {.type = EventTypeInput, .data = {.input = *input_event}};
    furi_message_queue_put(event_queue, &event, FuriWaitForever);
}

static void timer_callback(void* context) {
    FuriMessageQueue* event_queue = context;
    AppEvent event = {.type = EventTypeTick};
    furi_message_queue_put(event_queue, &event, 0);
}

int32_t calc_inv_app(void* p) {
    UNUSED(p);
    
    srand(furi_get_tick());
    
    FuriMessageQueue* event_queue = furi_message_queue_alloc(8, sizeof(AppEvent));
    
    GameData* game = malloc(sizeof(GameData));
    memset(game, 0, sizeof(GameData));
    game->state = StateTitle;
    game->high_score = 0;
    
    ViewPort* view_port = view_port_alloc();
    view_port_draw_callback_set(view_port, draw_callback, game);
    view_port_input_callback_set(view_port, input_callback, event_queue);
    
    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);
    
    FuriTimer* timer = furi_timer_alloc(timer_callback, FuriTimerTypePeriodic, event_queue);
    furi_timer_start(timer, furi_kernel_get_tick_frequency() / 20); // 20 Hz, 50ms ticks
    
    AppEvent event;
    bool running = true;
    
    while(running) {
        if(furi_message_queue_get(event_queue, &event, FuriWaitForever) == FuriStatusOk) {
            if(event.type == EventTypeInput) {
                // Exit app if back key is pressed (short press)
                if(event.data.input.key == InputKeyBack && event.data.input.type == InputTypeShort) {
                    running = false;
                } else if(event.data.input.type == InputTypePress) {
                    if(game->state == StateTitle) {
                        if(event.data.input.key == InputKeyOk || event.data.input.key == InputKeyRight) {
                            game->score = 0;
                            game->stage = 1;
                            game->part = 1;
                            game->turret = 0;
                            game->state = StatePlaying;
                            start_stage(game);
                            view_port_update(view_port);
                        }
                    } else if(game->state == StateGameOver) {
                        if(event.data.input.key == InputKeyOk || event.data.input.key == InputKeyRight) {
                            game->state = StateTitle;
                            view_port_update(view_port);
                        }
                    } else if(game->state == StatePlaying) {
                        if(event.data.input.key == InputKeyLeft || 
                           event.data.input.key == InputKeyRight || 
                           event.data.input.key == InputKeyUp || 
                           event.data.input.key == InputKeyDown) {
                            // Cycle turret number (0->1->2->...->9->n(10)->0)
                            game->turret = (game->turret + 1) % 11;
                            start_sound(440.0f, 1); // 50ms click
                            view_port_update(view_port);
                        } else if(event.data.input.key == InputKeyOk) {
                            // FIRE!
                            if(game->ammo > 0) {
                                game->ammo--;
                                start_sound(880.0f, 1); // 50ms fire beep
                                
                                // Check hit
                                for(int i = 0; i < STG_COLUMNS; i++) {
                                    if(game->field[i] == game->turret) {
                                        if(game->turret == 10) {
                                            // UFO hit!
                                            game->score += 300;
                                            start_sound(1760.0f, 2); // 100ms high beep
                                        } else {
                                            // Normal hit
                                            uint32_t points = 0;
                                            if(game->part == 1) {
                                                points = (i + 1) * 10;
                                            } else {
                                                points = (i + 1) * 20;
                                            }
                                            game->score += points;
                                            game->kill_sum += game->turret;
                                            start_sound(1760.0f, 2);
                                            
                                            if(game->kill_sum % 10 == 0) {
                                                game->ufo_pending = true;
                                            }
                                        }
                                        game->field[i] = EMPTY_DIGIT;
                                        break; // Only destroy the leftmost matching digit
                                    }
                                }
                                
                                if(game->score > game->high_score) {
                                    game->high_score = game->score;
                                }
                                
                                // Out of ammo check
                                if(game->ammo == 0) {
                                    // Check if there are still invaders left
                                    bool field_empty = true;
                                    for(int i = 0; i < STG_COLUMNS; i++) {
                                        if(game->field[i] != EMPTY_DIGIT) {
                                            field_empty = false;
                                            break;
                                        }
                                    }
                                    if(!field_empty) {
                                        game->state = StateGameOver;
                                        start_sound(220.0f, 6);
                                    }
                                }
                                view_port_update(view_port);
                            }
                        }
                    }
                }
            } else if(event.type == EventTypeTick) {
                if(game->state == StatePlaying) {
                    game->tick_counter++;
                    if(game->tick_counter >= game->tick_interval) {
                        game->tick_counter = 0;
                        
                        // Check breach
                        if(game->field[0] != EMPTY_DIGIT) {
                            game->lives--;
                            start_sound(220.0f, 6); // error beep
                            
                            // Clear field
                            for(int i = 0; i < STG_COLUMNS; i++) {
                                game->field[i] = EMPTY_DIGIT;
                            }
                            
                            if(game->lives == 0) {
                                game->state = StateGameOver;
                                if(game->score > game->high_score) {
                                    game->high_score = game->score;
                                }
                            }
                        } else {
                            // Shift left
                            for(int i = 0; i < STG_COLUMNS - 1; i++) {
                                game->field[i] = game->field[i+1];
                            }
                            game->field[STG_COLUMNS - 1] = EMPTY_DIGIT;
                            
                            // Spawn new invader
                            if(game->invaders_to_spawn > 0) {
                                uint8_t spawned;
                                if(game->ufo_pending) {
                                    spawned = 10; // UFO 'n'
                                    game->ufo_pending = false;
                                } else {
                                    spawned = rand() % 10;
                                }
                                game->invaders_to_spawn--;
                                
                                if(game->part == 1) {
                                    game->field[STG_COLUMNS - 1] = spawned;
                                } else {
                                    game->field[STG_COLUMNS - 2] = spawned;
                                }
                            }
                        }
                        
                        // Check stage clear
                        bool field_empty = true;
                        for(int i = 0; i < STG_COLUMNS; i++) {
                            if(game->field[i] != EMPTY_DIGIT) {
                                field_empty = false;
                                break;
                            }
                        }
                        
                        if(game->invaders_to_spawn == 0 && field_empty) {
                            game->stage++;
                            if(game->stage > 9) {
                                if(game->part == 1) {
                                    game->part = 2;
                                    game->stage = 1;
                                } else {
                                    game->part = 1;
                                    game->stage = 1;
                                }
                            }
                            start_stage(game);
                        }
                        
                        view_port_update(view_port);
                    }
                }
                update_sound();
            }
        }
    }
    
    // Cleanup sound if playing
    if(speaker_acquired) {
        furi_hal_speaker_stop();
        furi_hal_speaker_release();
    }
    
    furi_timer_stop(timer);
    furi_timer_free(timer);
    
    gui_remove_view_port(gui, view_port);
    view_port_free(view_port);
    furi_message_queue_free(event_queue);
    furi_record_close(RECORD_GUI);
    
    free(game);
    
    return 0;
}
