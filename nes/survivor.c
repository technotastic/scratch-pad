#include <nes.h>
#include <string.h> // For memset

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
#define PLAYER_SPRITE_WIDTH    8      // For collision
#define PLAYER_SPRITE_HEIGHT   8

// --- Enemy Configuration (Manual) ---
#define ENEMY_SPRITE_TILE      0x06   // Make sure this tile exists!
#define ENEMY_SPRITE_PALETTE   1
#define ENEMY_SPRITE_WIDTH     8
#define ENEMY_SPRITE_HEIGHT    8
// Enemy 0 (Uses Sprite 1)
#define ENEMY_0_OAM_OFFSET     4
// Enemy 1 (Uses Sprite 2)
#define ENEMY_1_OAM_OFFSET     8
// Enemy 2 (Uses Sprite 3) - ADDED
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
// --- Individual Enemy Variables ---
unsigned char enemy_0_x, enemy_0_y, enemy_0_active;
unsigned char enemy_1_x, enemy_1_y, enemy_1_active;
unsigned char enemy_2_x, enemy_2_y, enemy_2_active; // ADDED enemy 2

// --- Score Variables --- ADDED
unsigned int score = 0;
unsigned char score_changed = 0; // Flag to trigger display update
char score_digits[6]; // Buffer for ASCII score (5 digits + null)

// --- PPU Helper Functions --- (Same)
void ppu_set_address(unsigned int addr) { PPU.vram.address = (addr >> 8); PPU.vram.address = (addr & 0xFF); }
void ppu_write_data(unsigned char data) { PPU.vram.data = data; }
void trigger_oam_dma(void) { APU.sprite.dma = OAM_PAGE; } // Uses 0x02

// --- Input Reading --- (Same)
unsigned char read_joypad1(void) {
    unsigned char status = 0; unsigned char i; JOYPAD[0] = 1; JOYPAD[0] = 0;
    for (i = 0; i < 8; ++i) { status >>= 1; if (JOYPAD[0] & 1) { status |= 0x80; }} return status;
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

// --- Text Display Setup --- (Same)
#define TEXT_X 10
#define TEXT_Y 2
#define TEXT_STR "SCORE " // Base text, score appended later
#define SCORE_DIGIT_X (TEXT_X + 6) // X position where digits start
#define SCORE_MAX_DIGITS 5 // Display up to 5 digits
#define TEXT_PALETTE_IDX 1
void set_attribute_byte(unsigned int addr, unsigned char value){ waitvsync(); ppu_set_address(addr); ppu_write_data(value); }
void set_tile_palette(unsigned char x, unsigned char y, unsigned char pal_idx){ unsigned int a; unsigned char b, s; a = ATTRIBUTE_A + ((y / 4) * 8) + (x / 4); if ((y % 4) < 2) { s = ((x % 4) < 2) ? 0 : 2; } else { s = ((x % 4) < 2) ? 4 : 6; } b = (pal_idx & 0x03) << s; set_attribute_byte(a, b); }

// --- Collision Check Function --- ADDED
// Simple AABB check for 8x8 sprites
unsigned char check_collision(unsigned char x1, unsigned char y1, unsigned char x2, unsigned char y2) {
    return (x1 < (x2 + ENEMY_SPRITE_WIDTH) &&
            (x1 + PLAYER_SPRITE_WIDTH) > x2 &&
            y1 < (y2 + ENEMY_SPRITE_HEIGHT) &&
            (y1 + PLAYER_SPRITE_HEIGHT) > y2);
}

// --- Score Display Update Functions --- ADDED
// Basic unsigned int to ASCII (up to 5 digits)
void score_to_ascii(unsigned int s) {
    unsigned char i;
    unsigned int divisor = 10000; // Start with largest power of 10 for 5 digits

    // Handle leading zeros or smaller numbers by writing spaces first
    // (or handle this during VRAM write) - Let's convert all 5 digits.
    for (i=0; i<SCORE_MAX_DIGITS; ++i) {
        score_digits[i] = (s / divisor) % 10; // Get the digit
        s %= divisor; // Remove that digit's value
        divisor /= 10;
    }
    // Convert digits to ASCII '0'-'9'
    for (i=0; i<SCORE_MAX_DIGITS; ++i) {
        score_digits[i] += '0';
    }
    score_digits[SCORE_MAX_DIGITS] = '\0'; // Null terminate
}

// Writes the current score digits to VRAM
void update_score_display(void) {
    unsigned char i;
    unsigned int addr = NAMETABLE_A + (TEXT_Y * 32) + SCORE_DIGIT_X;
    score_to_ascii(score); // Update the score_digits buffer

    // PPU must be pointing to the right place before writing
    ppu_set_address(addr);
    for (i = 0; i < SCORE_MAX_DIGITS; ++i) {
        // Convert ASCII digit character '0'-'9' to NES tile index $30-$39
        ppu_write_data(score_digits[i] - '0' + 0x30);
    }
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
    // Draw "SCORE " part of the text initially
    vram_addr = NAMETABLE_A + (TEXT_Y * 32) + TEXT_X; waitvsync(); ppu_set_address(vram_addr);
    { const char* ptr = TEXT_STR; unsigned char len = strlen(ptr); for (i = 0; i < len; ++i) { unsigned char t; if (ptr[i]>='A'&&ptr[i]<='Z') t=ptr[i]-'A'+0x41; else if (ptr[i]==' ') t=0x20; else t=0x20; ppu_write_data(t); } for(i = 0; i < len + SCORE_MAX_DIGITS; ++i) { set_tile_palette(TEXT_X + i, TEXT_Y, TEXT_PALETTE_IDX); }} // Apply palette to score area too
    // Initial score display write
    update_score_display();

    // Initialize OAM Buffer RAM
    memset(oam_buffer, 0xF0, 256); // Hide all sprites initially

    // Initialize Player State
    player_x = 128; player_y = 112;

    // Initialize Enemy States
    enemy_0_x = 30; enemy_0_y = 30; enemy_0_active = 1;
    enemy_1_x = 200; enemy_1_y = 40; enemy_1_active = 1;
    enemy_2_x = 50; enemy_2_y = 200; enemy_2_active = 1; // ADDED Enemy 2 init

    // --- Write INITIAL OAM Data ---
    waitvsync();
    // Player
    oam_buffer[PLAYER_OAM_OFFSET + 0] = player_y - 1; oam_buffer[PLAYER_OAM_OFFSET + 1] = PLAYER_SPRITE_TILE; oam_buffer[PLAYER_OAM_OFFSET + 2] = (PLAYER_SPRITE_PALETTE & 0x03); oam_buffer[PLAYER_OAM_OFFSET + 3] = player_x;
    // Enemy 0
    if (enemy_0_active) { oam_buffer[ENEMY_0_OAM_OFFSET + 0] = enemy_0_y - 1; oam_buffer[ENEMY_0_OAM_OFFSET + 1] = ENEMY_SPRITE_TILE; oam_buffer[ENEMY_0_OAM_OFFSET + 2] = (ENEMY_SPRITE_PALETTE & 0x03); oam_buffer[ENEMY_0_OAM_OFFSET + 3] = enemy_0_x; }
    // Enemy 1
    if (enemy_1_active) { oam_buffer[ENEMY_1_OAM_OFFSET + 0] = enemy_1_y - 1; oam_buffer[ENEMY_1_OAM_OFFSET + 1] = ENEMY_SPRITE_TILE; oam_buffer[ENEMY_1_OAM_OFFSET + 2] = (ENEMY_SPRITE_PALETTE & 0x03); oam_buffer[ENEMY_1_OAM_OFFSET + 3] = enemy_1_x; }
    // Enemy 2 - ADDED
    if (enemy_2_active) { oam_buffer[ENEMY_2_OAM_OFFSET + 0] = enemy_2_y - 1; oam_buffer[ENEMY_2_OAM_OFFSET + 1] = ENEMY_SPRITE_TILE; oam_buffer[ENEMY_2_OAM_OFFSET + 2] = (ENEMY_SPRITE_PALETTE & 0x03); oam_buffer[ENEMY_2_OAM_OFFSET + 3] = enemy_2_x; }
    // --- End Initial OAM Write ---

    // Turn Rendering On
    waitvsync();
    PPU.scroll = 0x00; PPU.scroll = 0x00;
    PPU.mask = 0x1E;   // Show BG and Sprites

    // --- Main Game Loop ---
    while (1) {

        // --- Collision Detection and Scoring (Before VBlank wait) --- ADDED
        if (enemy_0_active && check_collision(player_x, player_y, enemy_0_x, enemy_0_y)) {
            enemy_0_active = 0; // Deactivate enemy
            score++;            // Increment score
            score_changed = 1;  // Mark score display for update
        }
        if (enemy_1_active && check_collision(player_x, player_y, enemy_1_x, enemy_1_y)) {
            enemy_1_active = 0;
            score++;
            score_changed = 1;
        }
        if (enemy_2_active && check_collision(player_x, player_y, enemy_2_x, enemy_2_y)) {
            enemy_2_active = 0;
            score++;
            score_changed = 1;
        }
        // --- End Collision Checks ---

        // --- VBlank Critical Section Start ---
        waitvsync();

        // --- Update Score Display (if needed, during VBlank) --- ADDED
        if (score_changed) {
             update_score_display(); // Writes to VRAM
             score_changed = 0; // Reset flag
        }

        // --- Update OAM Buffer (Positions mostly) ---
        // Player
        oam_buffer[PLAYER_OAM_OFFSET + 0] = player_y - 1;
        oam_buffer[PLAYER_OAM_OFFSET + 3] = player_x;
        oam_buffer[PLAYER_OAM_OFFSET + 1] = PLAYER_SPRITE_TILE; // Keep updating just in case
        oam_buffer[PLAYER_OAM_OFFSET + 2] = (PLAYER_SPRITE_PALETTE & 0x03);
        // Enemy 0
        if (enemy_0_active) { oam_buffer[ENEMY_0_OAM_OFFSET + 0] = enemy_0_y - 1; oam_buffer[ENEMY_0_OAM_OFFSET + 3] = enemy_0_x; }
        else { oam_buffer[ENEMY_0_OAM_OFFSET + 0] = 0xF0; } // Hide Y if inactive
        // Enemy 1
        if (enemy_1_active) { oam_buffer[ENEMY_1_OAM_OFFSET + 0] = enemy_1_y - 1; oam_buffer[ENEMY_1_OAM_OFFSET + 3] = enemy_1_x; }
        else { oam_buffer[ENEMY_1_OAM_OFFSET + 0] = 0xF0; }
        // Enemy 2 - ADDED
        if (enemy_2_active) { oam_buffer[ENEMY_2_OAM_OFFSET + 0] = enemy_2_y - 1; oam_buffer[ENEMY_2_OAM_OFFSET + 3] = enemy_2_x; }
        else { oam_buffer[ENEMY_2_OAM_OFFSET + 0] = 0xF0; }
        // --- End OAM Position Updates ---

        // --- Trigger OAM DMA ---
        trigger_oam_dma();
        // --- VBlank Critical Section End ---


        // --- Non-Critical Processing (After DMA) ---

        // Read Player Input
        joy_status = read_joypad1();

        // Update Player Position Variables for NEXT frame
        if ((joy_status & 0x10) && player_y > MIN_Y) player_y--;
        if ((joy_status & 0x20) && player_y < MAX_Y) player_y++;
        if ((joy_status & 0x40) && player_x > MIN_X) player_x--;
        if ((joy_status & 0x80) && player_x < MAX_X) player_x++;

        // Update Enemy Position Variables for NEXT frame
        if (enemy_0_active) { if (enemy_0_y < player_y) enemy_0_y++; else if (enemy_0_y > player_y) enemy_0_y--; if (enemy_0_x < player_x) enemy_0_x++; else if (enemy_0_x > player_x) enemy_0_x--; }
        if (enemy_1_active) { if (enemy_1_y < player_y) enemy_1_y++; else if (enemy_1_y > player_y) enemy_1_y--; if (enemy_1_x < player_x) enemy_1_x++; else if (enemy_1_x > player_x) enemy_1_x--; }
        if (enemy_2_active) { if (enemy_2_y < player_y) enemy_2_y++; else if (enemy_2_y > player_y) enemy_2_y--; if (enemy_2_x < player_x) enemy_2_x++; else if (enemy_2_x > player_x) enemy_2_x--; } // ADDED Enemy 2 logic
        // --- End Non-Critical Processing ---

    } // End while(1)

    return 0;
}