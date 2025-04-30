#include <nes.h>
#include <string.h> // For memset
//#include <stdio.h> // Removed stdio.h

// --- Constants ---
// PPU VRAM Addresses
#define NAMETABLE_A     0x2000
#define ATTRIBUTE_A     0x23C0
#define PALETTE_RAM     0x3F00

// Sprite Constants - Using $0200
#define OAM_ADDRESS     0x0200
#define OAM_PAGE        0x02

// Player Sprite (Sprite 0)
#define PLAYER_SPRITE_TILE     0x05   // Make sure this tile exists!
#define PLAYER_SPRITE_PALETTE  0
#define PLAYER_OAM_OFFSET      0
#define PLAYER_SPRITE_WIDTH    8
#define PLAYER_SPRITE_HEIGHT   8

// --- Enemy Configuration (Manual) ---
#define ENEMY_SPRITE_TILE      0x06   // Make sure this tile exists!
#define ENEMY_SPRITE_PALETTE   1
#define ENEMY_SPRITE_WIDTH     8
#define ENEMY_SPRITE_HEIGHT    8
#define ENEMY_0_OAM_OFFSET     4
#define ENEMY_1_OAM_OFFSET     8
#define ENEMY_2_OAM_OFFSET     12

// Screen Boundaries
#define MIN_X 8
#define MAX_X 240
#define MIN_Y 16
#define MAX_Y 216

// --- Global Variables ---
unsigned char* const oam_buffer = (unsigned char*)OAM_ADDRESS;
unsigned char player_x;
unsigned char player_y;
unsigned char enemy_0_x, enemy_0_y, enemy_0_active;
unsigned char enemy_1_x, enemy_1_y, enemy_1_active;
unsigned char enemy_2_x, enemy_2_y, enemy_2_active;

// --- Score Variables ---
unsigned int score = 0;
unsigned char score_changed = 0; // Still set, but display update is commented out

// --- PPU Helper Functions --- (Same)
void ppu_set_address(unsigned int addr) { PPU.vram.address = (addr >> 8); PPU.vram.address = (addr & 0xFF); }
void ppu_write_data(unsigned char data) { PPU.vram.data = data; }
void trigger_oam_dma(void) { APU.sprite.dma = OAM_PAGE; }

// --- Input Reading --- (Same)
unsigned char read_joypad1(void) {
    unsigned char status = 0; unsigned char i; JOYPAD[0] = 1; JOYPAD[0] = 0;
    for (i = 0; i < 8; ++i) { status >>= 1; if (JOYPAD[0] & 1) { status |= 0x80; }}
    return status;
}

// --- Palette Definition --- (Same)
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

// --- Text Display Setup ---
#define SCORE_TEXT_X 10 // X position for "SCORE "
#define SCORE_TEXT_Y 2  // Y position for text
#define SCORE_DIGIT_X (SCORE_TEXT_X + 6) // X position where digits start
#define SCORE_MAX_DIGITS 5 // Display up to 5 digits
#define SCORE_TEXT_PALETTE_IDX 1
void set_attribute_byte(unsigned int addr, unsigned char value){ waitvsync(); ppu_set_address(addr); ppu_write_data(value); }
void set_tile_palette(unsigned char x, unsigned char y, unsigned char pal_idx, unsigned char width) {
    unsigned int start_attr_addr = ATTRIBUTE_A + ((y / 4) * 8) + (x / 4);
    unsigned int end_attr_addr = ATTRIBUTE_A + ((y / 4) * 8) + ((x + width - 1) / 4);
    unsigned int current_attr_addr;
    unsigned char attr_byte_value = (pal_idx & 0x03) * 0x55;
    for (current_attr_addr = start_attr_addr; current_attr_addr <= end_attr_addr; ++current_attr_addr) {
        set_attribute_byte(current_attr_addr, attr_byte_value);
    }
}

// --- Collision Check Function --- (Same)
unsigned char check_collision(unsigned char x1, unsigned char y1, unsigned char x2, unsigned char y2) {
    unsigned char collided;
    collided = (x1 < (x2 + ENEMY_SPRITE_WIDTH) && (x1 + PLAYER_SPRITE_WIDTH) > x2 && y1 < (y2 + ENEMY_SPRITE_HEIGHT) && (y1 + PLAYER_SPRITE_HEIGHT) > y2);
    return collided;
}

// --- Simple Integer to ASCII for Score --- (Exists but not used by main loop)
void write_score_digits_vram(unsigned int s) {
    unsigned char i;
    unsigned int divisor = 10000;
    unsigned char digit_tile;
    for(i = 0; i < SCORE_MAX_DIGITS; ++i) {
        digit_tile = (s / divisor) % 10;
        ppu_write_data(digit_tile + 0x30);
        divisor /= 10;
    }
}

// --- Score Display Update Function --- (Exists but not used by main loop)
void update_score_display(void) {
    unsigned int addr = NAMETABLE_A + (SCORE_TEXT_Y * 32) + SCORE_DIGIT_X;
    ppu_set_address(addr);
    write_score_digits_vram(score);
}

int main(void) {
    unsigned char i;
    unsigned char joy_status;
    unsigned int vram_addr;

    // --- Initial Setup ---
    waitvsync();
    PPU.control = 0x00; // NMI OFF
    PPU.mask = 0x00;    // Screen OFF
    ppu_set_address(PALETTE_RAM); for (i = 0; i < 32; ++i) { ppu_write_data(palette[i]); }
    ppu_set_address(NAMETABLE_A); for (vram_addr = 0; vram_addr < 960; ++vram_addr) { ppu_write_data(0x00); }
    ppu_set_address(ATTRIBUTE_A); for (i = 0; i < 64; ++i) { ppu_write_data(0x00); }
    set_tile_palette(SCORE_TEXT_X, SCORE_TEXT_Y, SCORE_TEXT_PALETTE_IDX, 6 + SCORE_MAX_DIGITS);

    // Write static "SCORE " text ONCE
    vram_addr = NAMETABLE_A + (SCORE_TEXT_Y * 32) + SCORE_TEXT_X;
    waitvsync(); ppu_set_address(vram_addr);
    ppu_write_data('S'-'A'+0x41); ppu_write_data('C'-'A'+0x41); ppu_write_data('O'-'A'+0x41);
    ppu_write_data('R'-'A'+0x41); ppu_write_data('E'-'A'+0x41); ppu_write_data(0x00); // Space (tile 0)

    // Initial score display write (writes digits "00000")
    ppu_set_address(NAMETABLE_A + (SCORE_TEXT_Y * 32) + SCORE_DIGIT_X); // Set address for initial digits
    write_score_digits_vram(score); // Write initial zeros

    // Initialize OAM Buffer RAM
    memset(oam_buffer, 0xF0, 256);

    // Initialize Player State
    player_x = 128; player_y = 112;

    // Initialize Enemy States
    enemy_0_x = 30; enemy_0_y = 30; enemy_0_active = 1;
    enemy_1_x = 200; enemy_1_y = 40; enemy_1_active = 1;
    enemy_2_x = 50; enemy_2_y = 200; enemy_2_active = 1;

    // --- Write INITIAL OAM Data ---
    waitvsync();
    if(1){ /* Player */ oam_buffer[PLAYER_OAM_OFFSET + 0] = player_y - 1; oam_buffer[PLAYER_OAM_OFFSET + 1] = PLAYER_SPRITE_TILE; oam_buffer[PLAYER_OAM_OFFSET + 2] = (PLAYER_SPRITE_PALETTE & 0x03); oam_buffer[PLAYER_OAM_OFFSET + 3] = player_x; }
    if (enemy_0_active) { /* Enemy 0 */ oam_buffer[ENEMY_0_OAM_OFFSET + 0] = enemy_0_y - 1; oam_buffer[ENEMY_0_OAM_OFFSET + 1] = ENEMY_SPRITE_TILE; oam_buffer[ENEMY_0_OAM_OFFSET + 2] = (ENEMY_SPRITE_PALETTE & 0x03); oam_buffer[ENEMY_0_OAM_OFFSET + 3] = enemy_0_x; }
    if (enemy_1_active) { /* Enemy 1 */ oam_buffer[ENEMY_1_OAM_OFFSET + 0] = enemy_1_y - 1; oam_buffer[ENEMY_1_OAM_OFFSET + 1] = ENEMY_SPRITE_TILE; oam_buffer[ENEMY_1_OAM_OFFSET + 2] = (ENEMY_SPRITE_PALETTE & 0x03); oam_buffer[ENEMY_1_OAM_OFFSET + 3] = enemy_1_x; }
    if (enemy_2_active) { /* Enemy 2 */ oam_buffer[ENEMY_2_OAM_OFFSET + 0] = enemy_2_y - 1; oam_buffer[ENEMY_2_OAM_OFFSET + 1] = ENEMY_SPRITE_TILE; oam_buffer[ENEMY_2_OAM_OFFSET + 2] = (ENEMY_SPRITE_PALETTE & 0x03); oam_buffer[ENEMY_2_OAM_OFFSET + 3] = enemy_2_x; }

    // Turn Rendering On
    waitvsync();
    PPU.scroll = 0x00; PPU.scroll = 0x00;
    PPU.mask = 0x1E;

    // --- Main Game Loop ---
    while (1) {
        // Collision Detection and Scoring (score incremented, flag set)
        if (enemy_0_active && check_collision(player_x, player_y, enemy_0_x, enemy_0_y)) { enemy_0_active = 0; score++; score_changed = 1; }
        if (enemy_1_active && check_collision(player_x, player_y, enemy_1_x, enemy_1_y)) { enemy_1_active = 0; score++; score_changed = 1; }
        if (enemy_2_active && check_collision(player_x, player_y, enemy_2_x, enemy_2_y)) { enemy_2_active = 0; score++; score_changed = 1; }

        // VBlank Critical Section Start
        waitvsync();

        // --- Update Score Display --- *** DISABLED ***
        /*
        if (score_changed) {
             // update_score_display(); // VRAM writes commented out
             score_changed = 0; // Reset flag anyway
        }
        */
        // We still reset the flag if it was set, even though we don't update display
        if (score_changed) { score_changed = 0; }


        // --- Update OAM Buffer ---
        // Player
        oam_buffer[PLAYER_OAM_OFFSET + 0] = player_y - 1; oam_buffer[PLAYER_OAM_OFFSET + 3] = player_x;
        // Enemy 0
        if (enemy_0_active) { oam_buffer[ENEMY_0_OAM_OFFSET + 0] = enemy_0_y - 1; oam_buffer[ENEMY_0_OAM_OFFSET + 3] = enemy_0_x; }
        else { oam_buffer[ENEMY_0_OAM_OFFSET + 0] = 0xF0; oam_buffer[ENEMY_0_OAM_OFFSET + 1] = 0; oam_buffer[ENEMY_0_OAM_OFFSET + 2] = 0; oam_buffer[ENEMY_0_OAM_OFFSET + 3] = 0; }
        // Enemy 1
        if (enemy_1_active) { oam_buffer[ENEMY_1_OAM_OFFSET + 0] = enemy_1_y - 1; oam_buffer[ENEMY_1_OAM_OFFSET + 3] = enemy_1_x; }
        else { oam_buffer[ENEMY_1_OAM_OFFSET + 0] = 0xF0; oam_buffer[ENEMY_1_OAM_OFFSET + 1] = 0; oam_buffer[ENEMY_1_OAM_OFFSET + 2] = 0; oam_buffer[ENEMY_1_OAM_OFFSET + 3] = 0; }
        // Enemy 2
        if (enemy_2_active) { oam_buffer[ENEMY_2_OAM_OFFSET + 0] = enemy_2_y - 1; oam_buffer[ENEMY_2_OAM_OFFSET + 3] = enemy_2_x; }
        else { oam_buffer[ENEMY_2_OAM_OFFSET + 0] = 0xF0; oam_buffer[ENEMY_2_OAM_OFFSET + 1] = 0; oam_buffer[ENEMY_2_OAM_OFFSET + 2] = 0; oam_buffer[ENEMY_2_OAM_OFFSET + 3] = 0; }

        // Trigger OAM DMA
        trigger_oam_dma();
        // VBlank Critical Section End

        // Non-Critical Processing
        // Read Player Input
        joy_status = read_joypad1();
        // Update Player Position Variables
        if ((joy_status & 0x10) && player_y > MIN_Y) player_y--; if ((joy_status & 0x20) && player_y < MAX_Y) player_y++; if ((joy_status & 0x40) && player_x > MIN_X) player_x--; if ((joy_status & 0x80) && player_x < MAX_X) player_x++;
        // Update Enemy Position Variables
        if (enemy_0_active) { if (enemy_0_y < player_y) enemy_0_y++; else if (enemy_0_y > player_y) enemy_0_y--; if (enemy_0_x < player_x) enemy_0_x++; else if (enemy_0_x > player_x) enemy_0_x--; }
        if (enemy_1_active) { if (enemy_1_y < player_y) enemy_1_y++; else if (enemy_1_y > player_y) enemy_1_y--; if (enemy_1_x < player_x) enemy_1_x++; else if (enemy_1_x > player_x) enemy_1_x--; }
        if (enemy_2_active) { if (enemy_2_y < player_y) enemy_2_y++; else if (enemy_2_y > player_y) enemy_2_y--; if (enemy_2_x < player_x) enemy_2_x++; else if (enemy_2_x > player_x) enemy_2_x--; }

    } // End while(1)

    return 0;
}