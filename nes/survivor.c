#include <nes.h>
#include <string.h> // For memset
//#include <stdlib.h> // Commented out

// --- Constants ---
// PPU VRAM Addresses
#define NAMETABLE_A     0x2000
#define ATTRIBUTE_A     0x23C0
#define PALETTE_RAM     0x3F00

// Sprite Constants
#define OAM_ADDRESS     0x0200

// Player Sprite (Sprite 0)
#define PLAYER_SPRITE_TILE     0x05   // Tile index for player (MAKE SURE THIS EXISTS!)
#define PLAYER_SPRITE_PALETTE  0      // Use sprite palette 0 ($3F10 - $3F13)
#define PLAYER_OAM_OFFSET      0      // OAM Bytes 0-3

// --- Single Enemy Configuration ---
#define ENEMY_SPRITE_TILE      0x06   // Tile index for the enemy (MAKE SURE THIS EXISTS!)
#define ENEMY_SPRITE_PALETTE   1      // Use sprite palette 1 ($3F14 - $3F17) for difference
#define ENEMY_OAM_OFFSET       4      // OAM Bytes 4-7 (for Sprite 1)

// --- REMOVED or COMMENTED OUT Multi-Enemy Array Constants ---
//#define MAX_ENEMIES            8
//#define ENEMY_OAM_START_OFFSET 4

// Screen Boundaries
#define MIN_X 8
#define MAX_X 240
#define MIN_Y 16
#define MAX_Y 216

// --- Global Variables ---

// OAM buffer
unsigned char* const oam_buffer = (unsigned char*)OAM_ADDRESS;

// Player position variables
unsigned char player_x;
unsigned char player_y;

// --- Single Enemy Variables ---
unsigned char enemy_x;
unsigned char enemy_y;

// --- REMOVED or COMMENTED OUT Multi-Enemy Arrays ---
//unsigned char enemy_x[MAX_ENEMIES];
//unsigned char enemy_y[MAX_ENEMIES];
//unsigned char enemy_active[MAX_ENEMIES];

// --- PPU Helper Functions --- (Included)
void ppu_set_address(unsigned int addr) { PPU.vram.address = (addr >> 8); PPU.vram.address = (addr & 0xFF); }
void ppu_write_data(unsigned char data) { PPU.vram.data = data; }
void trigger_oam_dma(void) { APU.sprite.dma = 0x02; }

// --- Input Reading --- (Included)
unsigned char read_joypad1(void) {
    unsigned char status = 0; unsigned char i; JOYPAD[0] = 1; JOYPAD[0] = 0;
    for (i = 0; i < 8; ++i) { status >>= 1; if (JOYPAD[0] & 1) { status |= 0x80; }} return status;
}

// --- Palette Definition --- (Included)
const unsigned char palette[32] = {
    COLOR_BLACK, COLOR_BLUE, COLOR_BLUE, COLOR_BLUE,            // BG Pal 0
    COLOR_BLACK, COLOR_GRAY1, COLOR_GRAY2, COLOR_GRAY3,         // BG Pal 1
    COLOR_BLACK, COLOR_GREEN, COLOR_LIGHTGREEN, COLOR_WHITE,    // BG Pal 2
    COLOR_BLACK, COLOR_RED, COLOR_LIGHTRED, COLOR_WHITE,        // BG Pal 3
    COLOR_BLACK, COLOR_WHITE, COLOR_RED, COLOR_BLUE,            // Sprite Pal 0 (Player)
    COLOR_BLACK, COLOR_YELLOW, COLOR_ORANGE, COLOR_BROWN,       // Sprite Pal 1 (Enemy - uses palette 1 -> yellow/orange/brown)
    COLOR_BLACK, COLOR_CYAN, COLOR_LIGHTBLUE, COLOR_WHITE,      // Sprite Pal 2
    COLOR_BLACK, COLOR_VIOLET, COLOR_LIGHTRED, COLOR_WHITE      // Sprite Pal 3
}; // NOTE: Palette 1 used for the enemy here. Change ENEMY_SPRITE_PALETTE if you want a different color set.

// --- Text Display Setup --- (Included)
#define TEXT_X 10
#define TEXT_Y 12
#define TEXT_STR "HELLO NES!"
#define TEXT_PALETTE_IDX 1
void set_attribute_byte(unsigned int addr, unsigned char value){ waitvsync(); ppu_set_address(addr); ppu_write_data(value); }
void set_tile_palette(unsigned char x, unsigned char y, unsigned char pal_idx){ unsigned int a; unsigned char b, s; a = ATTRIBUTE_A + ((y / 4) * 8) + (x / 4); if ((y % 4) < 2) { s = ((x % 4) < 2) ? 0 : 2; } else { s = ((x % 4) < 2) ? 4 : 6; } b = (pal_idx & 0x03) << s; set_attribute_byte(a, b); }


int main(void) {
    unsigned char i;
    unsigned char joy_status;
    unsigned int vram_addr;

    // --- Initial Setup ---
    waitvsync();
    PPU.control = 0x80; // NMI On, Sprites $0000, BG $0000, 8x8, VRAM inc +1
    PPU.mask = 0x00;    // Screen OFF

    // Load Palettes
    ppu_set_address(PALETTE_RAM);
    for (i = 0; i < 32; ++i) { ppu_write_data(palette[i]); }

    // Clear Name Table & Attributes & Display Text
    ppu_set_address(NAMETABLE_A); for (vram_addr = 0; vram_addr < 960; ++vram_addr) { ppu_write_data(0x20); }
    ppu_set_address(ATTRIBUTE_A); for (i = 0; i < 64; ++i) { ppu_write_data(0x00); }
    vram_addr = NAMETABLE_A + (TEXT_Y * 32) + TEXT_X; waitvsync(); ppu_set_address(vram_addr);
    { const char* ptr = TEXT_STR; unsigned char len = strlen(ptr); for (i = 0; i < len; ++i) { unsigned char t; if (ptr[i]>='A'&&ptr[i]<='Z') t=ptr[i]-'A'+0x41; else if (ptr[i]>='a'&&ptr[i]<='z') t=ptr[i]-'a'+0x41; else if (ptr[i]>='0'&&ptr[i]<='9') t=ptr[i]-'0'+0x30; else if (ptr[i]==' ') t=0x20; else if (ptr[i]=='!') t=0x21; else t=0x20; ppu_write_data(t); } for(i = 0; i < len; ++i) { set_tile_palette(TEXT_X + i, TEXT_Y, TEXT_PALETTE_IDX); }}

    // Initialize OAM Buffer
    memset(oam_buffer, 0xF0, 256); // Hide all sprites initially

    // Initialize Player Sprite (Sprite 0)
    player_x = 128;
    player_y = 112;
    oam_buffer[PLAYER_OAM_OFFSET + 0] = player_y - 1;
    oam_buffer[PLAYER_OAM_OFFSET + 1] = PLAYER_SPRITE_TILE;
    oam_buffer[PLAYER_OAM_OFFSET + 2] = (PLAYER_SPRITE_PALETTE & 0x03);
    oam_buffer[PLAYER_OAM_OFFSET + 3] = player_x;

    // --- Initialize Enemy Sprite (Sprite 1) --- ADDED
    enemy_x = 50;  // Start somewhere else
    enemy_y = 50;
    oam_buffer[ENEMY_OAM_OFFSET + 0] = enemy_y - 1;                 // Y Pos
    oam_buffer[ENEMY_OAM_OFFSET + 1] = ENEMY_SPRITE_TILE;           // Tile
    oam_buffer[ENEMY_OAM_OFFSET + 2] = (ENEMY_SPRITE_PALETTE & 0x03); // Attributes (Palette 1)
    oam_buffer[ENEMY_OAM_OFFSET + 3] = enemy_x;                     // X Pos

    // --- REMOVED or COMMENTED OUT Multi-Enemy Init Loop ---
    /*
    for (i = 0; i < MAX_ENEMIES; ++i) {
        enemy_active[i] = 0;
    }
    */

    // Turn Rendering On
    waitvsync();
    PPU.scroll = 0x00; PPU.scroll = 0x00;
    PPU.mask = 0x1E;   // Show BG and Sprites

    // --- Main Game Loop ---
    while (1) {
        waitvsync(); // Sync to VBlank Start

        // --- Update Enemy Logic --- ADDED
        // Simple AI: Move 1 pixel towards player per frame
        if (enemy_y < player_y) {
            enemy_y++;
        } else if (enemy_y > player_y) {
            enemy_y--;
        }
        if (enemy_x < player_x) {
            enemy_x++;
        } else if (enemy_x > player_x) {
            enemy_x--;
        }

        // --- Update OAM Buffer in RAM ($0200) ---

        // Update Player Sprite 0 position
        oam_buffer[PLAYER_OAM_OFFSET + 0] = player_y - 1;
        oam_buffer[PLAYER_OAM_OFFSET + 3] = player_x;

        // Update Enemy Sprite 1 position --- ADDED
        oam_buffer[ENEMY_OAM_OFFSET + 0] = enemy_y - 1;
        oam_buffer[ENEMY_OAM_OFFSET + 3] = enemy_x;


        // --- Trigger OAM DMA --- (Included)
        trigger_oam_dma(); // Copy entire buffer ($0200-$02FF) to PPU OAM

        // --- Read Player Input --- (Included)
        joy_status = read_joypad1();

        // --- Update Player Logic --- (Included)
        if ((joy_status & 0x10) && player_y > MIN_Y) player_y--; // Up
        if ((joy_status & 0x20) && player_y < MAX_Y) player_y++; // Down
        if ((joy_status & 0x40) && player_x > MIN_X) player_x--; // Left
        if ((joy_status & 0x80) && player_x < MAX_X) player_x++; // Right

        // (Loop repeats)
    }

    return 0; // Never reached
}