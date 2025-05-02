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

// Player Sprite (Sprite 0)
#define PLAYER_SPRITE_TILE     0x05   // !!! TILE $05 MUST HAVE GRAPHICS !!!
#define PLAYER_SPRITE_PALETTE  0
#define PLAYER_OAM_OFFSET      0 // OAM starts at index 0
#define PLAYER_SPRITE_WIDTH    8
#define PLAYER_SPRITE_HEIGHT   8

// Enemy Configuration
#define ENEMY_SPRITE_TILE      0x06   // !!! TILE $06 MUST HAVE GRAPHICS !!!
#define ENEMY_SPRITE_PALETTE   1
#define ENEMY_SPRITE_WIDTH     8
#define ENEMY_SPRITE_HEIGHT    8
#define MAX_ENEMIES            30 // Max active enemies (ensure MAX_ENEMIES + 1 <= MAX_SPRITES)

// Screen Boundaries / Spawning
#define MIN_X 8
#define MAX_X 240
#define MIN_Y 16
#define MAX_Y 216
#define SPAWN_INTERVAL  60
#define SPAWN_MARGIN    16

// --- Structures ---
typedef struct { unsigned char x, y, active; } Enemy;

// --- Global Variables ---
unsigned char* const oam_buffer = (unsigned char*)OAM_ADDRESS;
unsigned char player_x;
unsigned char player_y;
Enemy enemies[MAX_ENEMIES];
unsigned char active_enemy_count = 0;
unsigned int score = 0;
unsigned char score_changed = 0;
unsigned char frame_count = 0;
unsigned int random_seed = 1;

// --- PRNG ---
unsigned char pseudo_rand(void) {
    random_seed = (random_seed * 1103515245 + 12345);
    return (unsigned char)((random_seed >> 8) & 0xFF);
}

// --- PPU Helpers ---
void ppu_set_address(unsigned int addr) { PPU.vram.address = (addr >> 8); PPU.vram.address = (addr & 0xFF); }
void ppu_write_data(unsigned char data) { PPU.vram.data = data; }
void trigger_oam_dma(void) { APU.sprite.dma = OAM_PAGE; }

// --- Input ---
unsigned char read_joypad1(void) {
    unsigned char status = 0, i;
    JOYPAD[0] = 1; JOYPAD[0] = 0; // Strobe
    for (i = 0; i < 8; ++i) { status >>= 1; if (JOYPAD[0] & 1) { status |= 0x80; } }
    return status;
}

// --- Palette ---
const unsigned char palette[32] = {
    COLOR_BLACK, COLOR_BLUE, COLOR_BLUE, COLOR_BLUE,            // BG Pal 0
    COLOR_BLACK, COLOR_WHITE, COLOR_RED, COLOR_YELLOW,          // BG Pal 1 (Text)
    COLOR_BLACK, COLOR_GREEN, COLOR_LIGHTGREEN, COLOR_WHITE,    // BG Pal 2
    COLOR_BLACK, COLOR_RED, COLOR_LIGHTRED, COLOR_WHITE,        // BG Pal 3
    COLOR_BLACK, COLOR_WHITE, COLOR_RED, COLOR_BLUE,            // Sprite Pal 0 (Player)
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
    unsigned int start_attr_addr; unsigned char start_attr_col, end_attr_col, attr_row, current_attr_col, attr_byte_value;
    attr_row = y_tile / 4; start_attr_col = x_tile / 4; end_attr_col = (x_tile + width_in_tiles - 1) / 4;
    attr_byte_value = (pal_idx & 0x03) * 0x55;
    for (current_attr_col = start_attr_col; current_attr_col <= end_attr_col; ++current_attr_col) {
        start_attr_addr = ATTRIBUTE_A + (attr_row * 8) + current_attr_col;
        ppu_set_address(start_attr_addr); ppu_write_data(attr_byte_value);
    }
}

void write_score_digits_vram(unsigned int s) {
    unsigned char i, digit_tile, leading_zero = 1; unsigned int divisor = 10000;
    for(i = 0; i < SCORE_MAX_DIGITS; ++i) {
        digit_tile = (s / divisor) % 10;
        if (digit_tile != 0 || !leading_zero || i == SCORE_MAX_DIGITS - 1) {
            ppu_write_data(digit_tile + 0x30); leading_zero = 0;
        } else { ppu_write_data(0x00); }
        divisor /= 10;
    }
}

void update_score_display(void) {
    unsigned int addr = NAMETABLE_A + (SCORE_TEXT_Y * 32) + SCORE_DIGIT_X;
    ppu_set_address(addr); write_score_digits_vram(score);
}

// --- Collision ---
unsigned char check_collision(unsigned char x1, unsigned char y1, unsigned char x2, unsigned char y2) {
    return (x1 < (x2 + ENEMY_SPRITE_WIDTH) && (x1 + PLAYER_SPRITE_WIDTH) > x2 &&
            y1 < (y2 + ENEMY_SPRITE_HEIGHT) && (y1 + PLAYER_SPRITE_HEIGHT) > y2);
}

// --- Enemy Spawning ---
void spawn_enemy(void) {
    unsigned char i, spawn_side, spawn_pos_x, spawn_pos_y;
    for (i = 0; i < MAX_ENEMIES; ++i) {
        if (!enemies[i].active) {
            enemies[i].active = 1; active_enemy_count++;
            spawn_side = pseudo_rand() & 3;
            spawn_pos_x = pseudo_rand(); spawn_pos_y = pseudo_rand();
            switch (spawn_side) { // Simplified position calculation and wrap check
                case 0: enemies[i].x = MIN_X + (spawn_pos_x % (MAX_X - MIN_X + 1)); enemies[i].y = MIN_Y - SPAWN_MARGIN; if (enemies[i].y > MAX_Y) enemies[i].y = 0; break;
                case 1: enemies[i].x = MIN_X + (spawn_pos_x % (MAX_X - MIN_X + 1)); enemies[i].y = MAX_Y + SPAWN_MARGIN; if (enemies[i].y < MIN_Y) enemies[i].y = 255; break;
                case 2: enemies[i].x = MIN_X - SPAWN_MARGIN; enemies[i].y = MIN_Y + (spawn_pos_y % (MAX_Y - MIN_Y + 1)); if (enemies[i].x > MAX_X) enemies[i].x = 0; break;
                case 3: enemies[i].x = MAX_X + SPAWN_MARGIN; enemies[i].y = MIN_Y + (spawn_pos_y % (MAX_Y - MIN_Y + 1)); if (enemies[i].x < MIN_X) enemies[i].x = 255; break;
            }
            return;
        }
    }
}

// --- Main ---
int main(void) {
    unsigned char i, joy_status, oam_idx;
    unsigned int vram_addr;

    // --- Initial Setup --- (Same as before)
    PPU.control = 0x00; PPU.mask = 0x00;
    waitvsync();
    ppu_set_address(PALETTE_RAM); for (i = 0; i < 32; ++i) { ppu_write_data(palette[i]); }
    ppu_set_address(NAMETABLE_A); for (vram_addr = 0; vram_addr < 960; ++vram_addr) { ppu_write_data(0x00); }
    ppu_set_address(ATTRIBUTE_A); for (i = 0; i < 64; ++i) { ppu_write_data(0x00); }
    set_tile_palette(SCORE_TEXT_X, SCORE_TEXT_Y, SCORE_TEXT_PALETTE_IDX, 6 + SCORE_MAX_DIGITS);
    vram_addr = NAMETABLE_A + (SCORE_TEXT_Y * 32) + SCORE_TEXT_X;
    ppu_set_address(vram_addr);
    ppu_write_data('S'-'A'+0x41); ppu_write_data('C'-'A'+0x41); ppu_write_data('O'-'A'+0x41);
    ppu_write_data('R'-'A'+0x41); ppu_write_data('E'-'A'+0x41); ppu_write_data(0x00);
    update_score_display();
    memset(oam_buffer, HIDE_SPRITE_Y, 256);
    player_x = 128; player_y = 112;
    for (i = 0; i < MAX_ENEMIES; ++i) { enemies[i].active = 0; } active_enemy_count = 0;
    random_seed = 123;

    // --- Turn Rendering On --- (Same as before)
    waitvsync();
    PPU.scroll = 0x00; PPU.scroll = 0x00;
    PPU.mask = 0x1E;    // Enable BG, Sprites, Left Columns ON
    PPU.control = 0x80; // NMI ON, 8x8 Sprites from $0000, BG from $0000

    // --- Main Loop ---
    while (1) {
        waitvsync(); // Sync to frame start

        // --- PPU Updates (DMA, VRAM writes) ---
        trigger_oam_dma(); // Send buffer prepared LAST frame to PPU
        if (score_changed) { update_score_display(); score_changed = 0; }

        // --- Prepare OAM Buffer for NEXT frame (Robust Method) ---
        // 1. Hide ALL sprites in the buffer first.
        memset(oam_buffer, HIDE_SPRITE_Y, 256);

        // 2. Write ONLY the player's current data into the buffer.
        // Ensure player_y is valid to avoid accidental hiding by player Y coord itself
        if (player_y >= 1 && player_y < HIDE_SPRITE_Y) {
            oam_buffer[PLAYER_OAM_OFFSET + 0] = player_y - 1; // Y pos (Y-1 rule)
            oam_buffer[PLAYER_OAM_OFFSET + 1] = PLAYER_SPRITE_TILE; // Tile
            oam_buffer[PLAYER_OAM_OFFSET + 2] = (PLAYER_SPRITE_PALETTE & 0x03); // Palette
            oam_buffer[PLAYER_OAM_OFFSET + 3] = player_x; // X pos
        } else {
             // Optional: If player Y is invalid, ensure sprite 0 is also hidden explicitly
             oam_buffer[PLAYER_OAM_OFFSET + 0] = HIDE_SPRITE_Y;
        }


        // 3. Add active enemies after the player.
        oam_idx = PLAYER_OAM_OFFSET + 4; // Start enemies at sprite slot 1
        for (i = 0; i < MAX_ENEMIES; ++i) {
            if (enemies[i].active) {
                 // Check if there is space left in OAM (max 64 sprites total)
                 if (oam_idx < (MAX_SPRITES * 4)) {
                    // Ensure enemy Y is valid before writing
                    if(enemies[i].y >= 1 && enemies[i].y < HIDE_SPRITE_Y) {
                         oam_buffer[oam_idx + 0] = enemies[i].y - 1;
                         oam_buffer[oam_idx + 1] = ENEMY_SPRITE_TILE; // CHECK CHR TILE $06 !!!
                         oam_buffer[oam_idx + 2] = (ENEMY_SPRITE_PALETTE & 0x03);
                         oam_buffer[oam_idx + 3] = enemies[i].x;
                         oam_idx += 4; // Move to next sprite slot *only if written*
                    }
                    // If enemy Y is invalid, we implicitly leave its OAM slot hidden due to memset
                 } else {
                    break; // Stop processing enemies if OAM buffer is full
                 }
            }
        }
        // 4. No need for a final loop to hide remaining sprites, memset did it.


        // --- Game Logic --- (Input, Spawning, Movement, Collision - Kept the same)
        // Input
        joy_status = read_joypad1();
        if ((joy_status & JOY_UP_MASK)    && player_y > MIN_Y) player_y--;
        if ((joy_status & JOY_DOWN_MASK)  && player_y < MAX_Y) player_y++;
        if ((joy_status & JOY_LEFT_MASK)  && player_x > MIN_X) player_x--;
        if ((joy_status & JOY_RIGHT_MASK) && player_x < MAX_X) player_x++;

        // Enemy Logic
        frame_count++;
        if ((frame_count >= SPAWN_INTERVAL) && (active_enemy_count < MAX_ENEMIES)) {
             spawn_enemy(); frame_count = 0;
        }
        for (i = 0; i < MAX_ENEMIES; ++i) {
            if (enemies[i].active) {
                // Move
                if (enemies[i].y < player_y) enemies[i].y++; else if (enemies[i].y > player_y) enemies[i].y--;
                if (enemies[i].x < player_x) enemies[i].x++; else if (enemies[i].x > player_x) enemies[i].x--;
                // Collide
                if (check_collision(player_x, player_y, enemies[i].x, enemies[i].y)) {
                    enemies[i].active = 0; active_enemy_count--; score++; score_changed = 1;
                }
            }
        }
    } // End while(1)
}