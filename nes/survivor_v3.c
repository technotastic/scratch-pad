#include <nes.h>
#include <string.h> // For memset

// --- Constants ---
// PPU VRAM Addresses
#define NAMETABLE_A     0x2000
#define ATTRIBUTE_A     0x23C0
#define PALETTE_RAM     0x3F00

// Sprite Constants - Using $0200
#define OAM_ADDRESS     0x0200
#define OAM_PAGE        0x02 // Page number for OAM DMA ($4014)
#define MAX_SPRITES     64   // NES hardware limit
#define HIDE_SPRITE_Y   0xF0 // Y coordinate to hide a sprite
#define LAST_VALID_OAM_INDEX 252 // (MAX_SPRITES - 1) * 4

// Player Sprite (Sprite 0)
#define PLAYER_SPRITE_TILE     0x05   // !!! TILE $05 MUST HAVE GRAPHICS IN YOUR CHR !!!
#define PLAYER_SPRITE_PALETTE  0
#define PLAYER_OAM_OFFSET      0 // OAM starts at index 0 (bytes 0-3)
#define PLAYER_SPRITE_WIDTH    8
#define PLAYER_SPRITE_HEIGHT   8
#define PLAYER_MAX_HEALTH      3 // How many hits the player can take
#define PLAYER_INVINCIBILITY_FRAMES 60 // Frames of invincibility after getting hit (~1 second)

// Enemy Configuration
#define ENEMY_SPRITE_TILE      0x06   // !!! TILE $06 MUST HAVE GRAPHICS IN YOUR CHR !!!
#define ENEMY_SPRITE_PALETTE   1
#define ENEMY_SPRITE_WIDTH     8
#define ENEMY_SPRITE_HEIGHT    8
#define MAX_ENEMIES            30 // Max active enemies (ensure MAX_ENEMIES + 1 + MAX_PROJECTILES <= MAX_SPRITES)

// Projectile Configuration
#define MAX_PROJECTILES        5  // Max player bullets on screen
#define PROJECTILE_SPRITE_TILE 0x07 // !!! TILE $07 MUST HAVE GRAPHICS IN YOUR CHR !!!
#define PROJECTILE_SPRITE_PALETTE 0 // Use player palette for projectiles
#define PROJECTILE_SPEED       2  // Pixels per frame movement
#define PROJECTILE_SPRITE_WIDTH 8 // Assuming 8x8 sprite
#define PROJECTILE_SPRITE_HEIGHT 8 // Assuming 8x8 sprite
#define FIRE_BUTTON_MASK       0x80 // Use 0x80 for Button A (standard mapping)

// Screen Boundaries / Spawning
#define MIN_X 8
#define MAX_X 240 // Max X considering player width (255 - 8)
#define MIN_Y 16
#define MAX_Y 216 // Max Y considering player height (224 - 8)
#define SPAWN_INTERVAL  60 // Frames between enemy spawns
#define SPAWN_MARGIN    16 // How far off-screen enemies spawn

// --- Structures ---
typedef struct {
    unsigned char x;
    unsigned char y;
    unsigned char active; // 0 = inactive, 1 = active
} Enemy;

typedef struct {
    unsigned char x, y;
    unsigned char active; // 0 = inactive, 1 = active
} Projectile;


// --- Global Variables ---
unsigned char* const oam_buffer = (unsigned char*)OAM_ADDRESS; // OAM buffer pointer
unsigned char player_x, player_y; // Player position
unsigned char player_health;      // Player health
unsigned char player_hit_timer;   // Player invincibility timer
Enemy enemies[MAX_ENEMIES];       // Enemy array
unsigned char active_enemy_count; // Count of active enemies
Projectile projectiles[MAX_PROJECTILES]; // Projectile array
unsigned int score;               // Game score
unsigned char score_changed;      // Score update flag
unsigned char frame_count;        // Frame counter for spawning
unsigned int random_seed = 1;     // PRNG seed
static unsigned char last_joy_status = 0; // Previous joypad state


// --- PRNG ---
unsigned char pseudo_rand(void) {
    random_seed = (random_seed * 1103515245 + 12345);
    return (unsigned char)((random_seed >> 8) & 0xFF);
}

// --- PPU Helpers ---
void ppu_set_address(unsigned int addr) {
    PPU.vram.address = (addr >> 8);
    PPU.vram.address = (addr & 0xFF);
}
void ppu_write_data(unsigned char data) {
    PPU.vram.data = data;
}
void trigger_oam_dma(void) {
    APU.sprite.dma = OAM_PAGE;
}

// --- Input ---
unsigned char read_joypad1(void) {
    unsigned char status = 0, i;
    JOYPAD[0] = 1; JOYPAD[0] = 0; // Strobe
    for (i = 0; i < 8; ++i) { status >>= 1; if (JOYPAD[0] & 1) { status |= 0x80; } }
    return status; // Bit order: A, B, Select, Start, Up, Down, Left, Right (7..0)
}

// --- Palette ---
const unsigned char palette[32] = {
    COLOR_BLACK, COLOR_BLUE, COLOR_BLUE, COLOR_BLUE,            // BG Pal 0
    COLOR_BLACK, COLOR_WHITE, COLOR_RED, COLOR_YELLOW,          // BG Pal 1 (Text)
    COLOR_BLACK, COLOR_GREEN, COLOR_LIGHTGREEN, COLOR_WHITE,    // BG Pal 2
    COLOR_BLACK, COLOR_RED, COLOR_LIGHTRED, COLOR_WHITE,        // BG Pal 3
    COLOR_BLACK, COLOR_WHITE, COLOR_RED, COLOR_BLUE,            // Sprite Pal 0 (Player & Projectiles)
    COLOR_BLACK, COLOR_YELLOW, COLOR_ORANGE, COLOR_BROWN,       // Sprite Pal 1 (Enemy)
    COLOR_BLACK, COLOR_CYAN, COLOR_LIGHTBLUE, COLOR_WHITE,      // Sprite Pal 2
    COLOR_BLACK, COLOR_VIOLET, COLOR_LIGHTRED, COLOR_WHITE      // Sprite Pal 3
};

// --- Text Display ---
#define SCORE_TEXT_X 10
#define SCORE_TEXT_Y 2
#define SCORE_DIGIT_X (SCORE_TEXT_X + 6)
#define SCORE_MAX_DIGITS 5
#define SCORE_TEXT_PALETTE_IDX 1

void set_tile_palette(unsigned char x_tile, unsigned char y_tile, unsigned char pal_idx, unsigned char width_in_tiles) {
    unsigned int start_attr_addr;
    unsigned char start_attr_col, end_attr_col, attr_row, current_attr_col;
    attr_row = y_tile / 4; start_attr_col = x_tile / 4; end_attr_col = (x_tile + width_in_tiles - 1) / 4;
    for (current_attr_col = start_attr_col; current_attr_col <= end_attr_col; ++current_attr_col) {
        start_attr_addr = ATTRIBUTE_A + (attr_row * 8) + current_attr_col;
        ppu_set_address(start_attr_addr); ppu_write_data((pal_idx * 0x55));
    }
}
void write_score_digits_vram(unsigned int s) {
    unsigned char i, digit_tile, leading_zero = 1; unsigned int divisor = 10000;
    for(i = 0; i < SCORE_MAX_DIGITS; ++i) {
        digit_tile = (s / divisor) % 10;
        if (digit_tile != 0 || !leading_zero || i == SCORE_MAX_DIGITS - 1) {
            ppu_write_data(digit_tile + 0x30); leading_zero = 0; // Use tile $30..$39
        } else { ppu_write_data(0x00); } // Use tile $00 (blank)
        divisor /= 10;
    }
}
void update_score_display(void) {
    unsigned int addr = NAMETABLE_A + (SCORE_TEXT_Y * 32) + SCORE_DIGIT_X;
    ppu_set_address(addr); write_score_digits_vram(score);
}

// --- Collision ---
unsigned char check_collision(unsigned char x1, unsigned char y1, unsigned char w1, unsigned char h1,
                              unsigned char x2, unsigned char y2, unsigned char w2, unsigned char h2) {
    return (x1 < (x2 + w2) && (x1 + w1) > x2 && y1 < (y2 + h2) && (y1 + h1) > y2);
}

// --- Enemy Spawning ---
void spawn_enemy(void) {
    unsigned char i, spawn_side; signed int spawn_x_s, spawn_y_s;
    for (i = 0; i < MAX_ENEMIES; ++i) {
        if (!enemies[i].active) {
            enemies[i].active = 1; active_enemy_count++;
            spawn_side = pseudo_rand() & 3;
            switch (spawn_side) {
                case 0: spawn_x_s = MIN_X + (pseudo_rand() % (MAX_X - MIN_X + 1)); spawn_y_s = MIN_Y - SPAWN_MARGIN; break;
                case 1: spawn_x_s = MIN_X + (pseudo_rand() % (MAX_X - MIN_X + 1)); spawn_y_s = MAX_Y + SPAWN_MARGIN; break;
                case 2: spawn_x_s = MIN_X - SPAWN_MARGIN; spawn_y_s = MIN_Y + (pseudo_rand() % (MAX_Y - MIN_Y + 1)); break;
                default:spawn_x_s = MAX_X + SPAWN_MARGIN; spawn_y_s = MIN_Y + (pseudo_rand() % (MAX_Y - MIN_Y + 1)); break;
            }
            if (spawn_x_s < 0) enemies[i].x = 0; else if (spawn_x_s > 255) enemies[i].x = 255; else enemies[i].x = (unsigned char)spawn_x_s;
            if (spawn_y_s < 0) enemies[i].y = 0; else if (spawn_y_s > 255) enemies[i].y = 255; else enemies[i].y = (unsigned char)spawn_y_s;
            return;
        }
    }
}

// --- Game Over ---
void game_over_halt(void) {
    PPU.mask = 0x00; // Turn off rendering
    while(1);        // Halt execution
}

// --- Main Function ---
void main(void) {
    unsigned char i, j; // Loop counters
    unsigned char joy_status;
    unsigned int vram_addr;
    unsigned char oam_idx; // OAM buffer index

    // Declare draw_player here, OUTSIDE the main loop.
    // We will just set its value inside the loop.
    // THIS IS A CHANGE from the previous attempt.
    unsigned char draw_player;


    // --- Initial Setup ---
    PPU.control = 0x00; PPU.mask = 0x00; // PPU Off
    waitvsync();
    ppu_set_address(PALETTE_RAM); for (i = 0; i < 32; ++i) ppu_write_data(palette[i]); // Load palettes
    ppu_set_address(NAMETABLE_A); for (vram_addr = 0; vram_addr < 960; ++vram_addr) ppu_write_data(0x00); // Clear nametable
    ppu_set_address(ATTRIBUTE_A); for (i = 0; i < 64; ++i) ppu_write_data(0x00); // Clear attributes
    set_tile_palette(SCORE_TEXT_X, SCORE_TEXT_Y, SCORE_TEXT_PALETTE_IDX, 6 + SCORE_MAX_DIGITS); // Set score palette
    vram_addr = NAMETABLE_A + (SCORE_TEXT_Y * 32) + SCORE_TEXT_X; // Write "SCORE "
    ppu_set_address(vram_addr);
    ppu_write_data('S'-'A'+0x41); ppu_write_data('C'-'A'+0x41); ppu_write_data('O'-'A'+0x41);
    ppu_write_data('R'-'A'+0x41); ppu_write_data('E'-'A'+0x41); ppu_write_data(0x00);

    memset(oam_buffer, HIDE_SPRITE_Y, 256); // Clear OAM buffer in RAM

    // Init Player
    player_x = 128; player_y = 112; player_health = PLAYER_MAX_HEALTH; player_hit_timer = 0;
    // Init Enemies
    for (i = 0; i < MAX_ENEMIES; ++i) enemies[i].active = 0; active_enemy_count = 0;
    // Init Projectiles
    for(i = 0; i < MAX_PROJECTILES; ++i) projectiles[i].active = 0;
    // Init Game State
    score = 0; score_changed = 1; frame_count = 0; last_joy_status = 0; random_seed = 123;

    // --- Turn Rendering On ---
    waitvsync();
    PPU.scroll = 0x00; PPU.scroll = 0x00; // Reset scroll
    PPU.mask = 0x1E;    // BG ON, Sprites ON, Left Columns ON
    PPU.control = 0x90; // NMI ON, Sprites $0000, BG $1000 (Use 0x80 if BG is $0000)

    // --- Main Game Loop ---
    while (1) {
        waitvsync(); // Wait for VBlank

        // --- PPU Updates (during VBlank) ---
        trigger_oam_dma(); // Send OAM data from LAST frame
        if (score_changed) { update_score_display(); score_changed = 0; } // Update score if needed

        // --- Prepare OAM Buffer for NEXT frame ---
        memset(oam_buffer, HIDE_SPRITE_Y, 256); // Clear RAM buffer first
        oam_idx = PLAYER_OAM_OFFSET; // Reset OAM index for this frame


        // !!! CRITICAL DRAW_PLAYER SECTION !!!
        // Check syntax immediately before and after this block carefully.

        // Set default value for draw_player for this frame.
        // draw_player was declared OUTSIDE the loop this time.
        draw_player = 1;

        // Check if player is invincible and should flash (be hidden)
        if (player_hit_timer > 0) {
            if ((player_hit_timer % 8) < 4) { // Hidden part of the flash cycle
                 draw_player = 0; // Set flag to NOT draw player this frame
            }
        }

        // Now, USE the draw_player flag to decide OAM write
        // Ensure the line above this has a correct ending (like ';')
        // Line 392 was pointing around here.
        if (draw_player && player_y >= 1 && player_y < HIDE_SPRITE_Y) {
            oam_buffer[oam_idx + 0] = player_y - 1;
            oam_buffer[oam_idx + 1] = PLAYER_SPRITE_TILE;
            oam_buffer[oam_idx + 2] = (PLAYER_SPRITE_PALETTE & 0x03);
            oam_buffer[oam_idx + 3] = player_x;
        }
        // Ensure the line below this starts correctly.
        oam_idx += 4; // Always advance index past player sprite slot

        // !!! END OF CRITICAL SECTION !!!


        // Write Active Enemies to OAM
        for (i = 0; i < MAX_ENEMIES; ++i) {
            if (enemies[i].active) {
                 if (oam_idx <= LAST_VALID_OAM_INDEX) {
                    if(enemies[i].y >= 1 && enemies[i].y < HIDE_SPRITE_Y) {
                         oam_buffer[oam_idx + 0] = enemies[i].y - 1;
                         oam_buffer[oam_idx + 1] = ENEMY_SPRITE_TILE;
                         oam_buffer[oam_idx + 2] = (ENEMY_SPRITE_PALETTE & 0x03);
                         oam_buffer[oam_idx + 3] = enemies[i].x;
                         oam_idx += 4;
                    }
                 } else { break; } // OAM full
            }
        }

        // Write Active Projectiles to OAM
        for (i = 0; i < MAX_PROJECTILES; ++i) {
            if (projectiles[i].active) {
                if (oam_idx <= LAST_VALID_OAM_INDEX) {
                    if(projectiles[i].y >= 1 && projectiles[i].y < HIDE_SPRITE_Y) {
                         oam_buffer[oam_idx + 0] = projectiles[i].y - 1;
                         oam_buffer[oam_idx + 1] = PROJECTILE_SPRITE_TILE;
                         oam_buffer[oam_idx + 2] = (PROJECTILE_SPRITE_PALETTE & 0x03);
                         oam_buffer[oam_idx + 3] = projectiles[i].x;
                         oam_idx += 4;
                    }
                } else { break; } // OAM full
            }
        }

        // --- Game Logic ---
        if (player_hit_timer > 0) player_hit_timer--; // Update invincibility timer

        joy_status = read_joypad1(); // Read input

        // Player Movement
        if ((joy_status & JOY_UP_MASK) && player_y > MIN_Y) player_y--;
        if ((joy_status & JOY_DOWN_MASK) && player_y < MAX_Y) player_y++;
        if ((joy_status & JOY_LEFT_MASK) && player_x > MIN_X) player_x--;
        if ((joy_status & JOY_RIGHT_MASK) && player_x < MAX_X) player_x++;

        // Player Firing (Button A - 0x80)
        if ((joy_status & FIRE_BUTTON_MASK) && !(last_joy_status & FIRE_BUTTON_MASK)) {
            for (i = 0; i < MAX_PROJECTILES; ++i) {
                if (!projectiles[i].active) {
                    projectiles[i].active = 1;
                    projectiles[i].x = player_x; projectiles[i].y = player_y;
                    break;
                }
            }
        }
        last_joy_status = joy_status; // Store for next frame

        // --- Projectile Logic ---
        for (i = 0; i < MAX_PROJECTILES; ++i) {
            if (projectiles[i].active) {
                // 1. Move
                if (projectiles[i].y > (MIN_Y + PROJECTILE_SPEED)) {
                    projectiles[i].y -= PROJECTILE_SPEED;
                } else {
                    projectiles[i].active = 0;
                    continue; // Off screen, go to next projectile
                }

                // 2. Collide with Enemies
                for (j = 0; j < MAX_ENEMIES; ++j) {
                     if (enemies[j].active) {
                         if (check_collision(projectiles[i].x, projectiles[i].y, PROJECTILE_SPRITE_WIDTH, PROJECTILE_SPRITE_HEIGHT,
                                             enemies[j].x, enemies[j].y, ENEMY_SPRITE_WIDTH, ENEMY_SPRITE_HEIGHT))
                         {
                             projectiles[i].active = 0; // Deactivate projectile
                             enemies[j].active = 0;     // Deactivate enemy
                             active_enemy_count--;
                             score++; score_changed = 1;
                             // Since projectile is now inactive, break inner loop and outer loop will continue to next i
                             break; // Stop checking this projectile against other enemies
                         }
                     }
                 } // End enemy loop (j)
            } // End if projectile active
        } // End projectile loop (i)


        // --- Enemy Logic ---
        frame_count++; // Spawning Timer
        if ((frame_count >= SPAWN_INTERVAL) && (active_enemy_count < MAX_ENEMIES)) {
             spawn_enemy(); frame_count = 0;
        }

        // Enemy Movement & Player Collision
        for (i = 0; i < MAX_ENEMIES; ++i) {
            if (enemies[i].active) {
                // 1. Move
                if (enemies[i].y < player_y) enemies[i].y++; else if (enemies[i].y > player_y) enemies[i].y--;
                if (enemies[i].x < player_x) enemies[i].x++; else if (enemies[i].x > player_x) enemies[i].x--;

                // 2. Collide with Player (only if player not invincible)
                if (player_hit_timer == 0) {
                     if (check_collision(player_x, player_y, PLAYER_SPRITE_WIDTH, PLAYER_SPRITE_HEIGHT,
                                         enemies[i].x, enemies[i].y, ENEMY_SPRITE_WIDTH, ENEMY_SPRITE_HEIGHT))
                    {
                        player_health--; player_hit_timer = PLAYER_INVINCIBILITY_FRAMES;
                        // Keep enemy active after hitting player? Or deactivate?
                        // enemies[i].active = 0; active_enemy_count--; // Uncomment to kill enemy on touch

                        if (player_health == 0) {
                            game_over_halt(); // Check for Game Over
                        }
                        // Don't check collision with other enemies in same frame if player just got hit
                        // (This is implicitly handled by the hit timer)
                    }
                } // End if player not invincible
            } // End if enemy active
        } // End enemy loop (i)

    } // End while(1)

} // End main()