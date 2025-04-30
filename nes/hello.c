#include <nes.h>
#include <string.h> // For memset

// --- Constants ---
// PPU VRAM Addresses
#define NAMETABLE_A     0x2000
#define ATTRIBUTE_A     0x23C0
#define PALETTE_RAM     0x3F00

// Sprite Constants
#define OAM_ADDRESS     0x0200 // Standard RAM address for OAM buffer
#define SPRITE_TILE     0x05   // Tile index for our sprite (from CHR ROM - MAKE SURE THIS EXISTS!)
                               // TRY 0x00 or 0x01 if 0x05 doesn't work!
#define SPRITE_PALETTE  0      // Use sprite palette 0 ($3F10 - $3F13)

// Screen Boundaries (approximate safe area)
#define MIN_X 8
#define MAX_X 240
#define MIN_Y 16
#define MAX_Y 216

// --- Global Variables ---

// OAM buffer - MUST be placed at a page boundary, $0200 is conventional.
// Using a pointer constant is the most direct way to ensure this in C.
// NOTE: Ensure your linker setup doesn't place other critical data here!
unsigned char* const oam_buffer = (unsigned char*)OAM_ADDRESS;

// Sprite position variables
unsigned char sprite_x;
unsigned char sprite_y;

// --- PPU Helper Functions ---

// Set PPU VRAM Address
void ppu_set_address(unsigned int addr) {
    PPU.vram.address = (unsigned char)(addr >> 8);
    PPU.vram.address = (unsigned char)(addr & 0xFF);
}

// Write to PPU VRAM Data
void ppu_write_data(unsigned char data) {
    PPU.vram.data = data;
}

// Trigger OAM DMA transfer from OAM_ADDRESS ($0200)
void trigger_oam_dma(void) {
    APU.sprite.dma = 0x02; // Write the high byte of the RAM address ($02xx)
    // The PPU hardware handles the 256-byte transfer from $0200-$02FF
    // This halts the CPU for ~513 cycles
}

// --- Input Reading ---
unsigned char read_joypad1(void) {
    unsigned char status = 0;
    unsigned char i;

    // Strobe the controller to latch current button states
    JOYPAD[0] = 1;
    // Small delay might be needed between strobe writes on some hardware/emulators
    // for (i=0; i<10; ++i); // Optional small delay
    JOYPAD[0] = 0;

    // Read the 8 button states sequentially
    // Order: A, B, Select, Start, Up, Down, Left, Right
    for (i = 0; i < 8; ++i) {
        status >>= 1; // Shift previous bits
        if (JOYPAD[0] & 1) { // Read the lowest bit
             status |= 0x80; // Set the top bit if button is pressed
        }
    }
    // Returns raw bits: R L D U T S B A (Right is MSB, A is LSB)
    return status;
}

// --- Main Program ---

// Define our palettes (Background + Sprite)
// Background palette 0-3: $3F00 - $3F0F
// Sprite palette 0-3:     $3F10 - $3F1F
const unsigned char palette[32] = {
    // Background palettes (example: solid blue BG - using COLOR_BLUE)
    COLOR_BLACK, COLOR_BLUE, COLOR_BLUE, COLOR_BLUE,            // BG Palette 0 (Fixed: Used COLOR_BLUE)
    COLOR_BLACK, COLOR_GRAY1, COLOR_GRAY2, COLOR_GRAY3,         // BG Palette 1
    COLOR_BLACK, COLOR_GREEN, COLOR_LIGHTGREEN, COLOR_WHITE,    // BG Palette 2
    COLOR_BLACK, COLOR_RED, COLOR_LIGHTRED, COLOR_WHITE,        // BG Palette 3

    // Sprite palettes (example: white/red/blue sprite)
    COLOR_BLACK, COLOR_WHITE, COLOR_RED, COLOR_BLUE,            // Sprite Palette 0 ($3F10-$3F13)
    COLOR_BLACK, COLOR_YELLOW, COLOR_ORANGE, COLOR_BROWN,       // Sprite Palette 1 ($3F14-$3F17)
    COLOR_BLACK, COLOR_CYAN, COLOR_LIGHTBLUE, COLOR_WHITE,      // Sprite Palette 2 ($3F18-$3F1B)
    COLOR_BLACK, COLOR_VIOLET, COLOR_LIGHTRED, COLOR_WHITE      // Sprite Palette 3 ($3F1C-$3F1F)
};

// Define the text from the first example (Optional, can be removed if only moving sprite)
#define TEXT_X 10
#define TEXT_Y 12
#define TEXT_STR "HELLO NES!"
#define TEXT_PALETTE_IDX 1 // Use Background Palette 1 for text

// Function to set attribute byte (needed if displaying text)
void set_attribute_byte(unsigned int addr, unsigned char value) {
    waitvsync();
    ppu_set_address(addr);
    ppu_write_data(value);
}

// Function to set tile palette (needed if displaying text)
void set_tile_palette(unsigned char x, unsigned char y, unsigned char pal_idx) {
    unsigned int attr_addr;
    unsigned char attr_byte;
    unsigned char shift_val;
    attr_addr = ATTRIBUTE_A + ((y / 4) * 8) + (x / 4);
    if ((y % 4) < 2) { shift_val = ((x % 4) < 2) ? 0 : 2; }
    else             { shift_val = ((x % 4) < 2) ? 4 : 6; }
    // Simplified: Assume attribute byte is 0 initially or we overwrite
    attr_byte = (pal_idx & 0x03) << shift_val;
    set_attribute_byte(attr_addr, attr_byte);
}


int main(void) {
    unsigned char i;
    unsigned char joy_status;
    unsigned int vram_addr; // Needed for text display

    // --- Initial Setup ---

    // 1. Turn off screen during setup
    waitvsync();
    PPU.control = 0x80; // NMI Enabled, Sprites $0000, BG $0000, 8x8 Sprites, VRAM Inc +1
                        // Sprite pattern table at $0000 (bit 3 = 0)
                        // Change to 0x88 if sprites are at $1000
    PPU.mask = 0x00;    // Screen OFF

    // 2. Load Palettes (Background and Sprite)
    ppu_set_address(PALETTE_RAM);
    for (i = 0; i < 32; ++i) {
        ppu_write_data(palette[i]);
    }

    // 3. Clear Name Table (fill with tile 0x20 - space, for text visibility)
    ppu_set_address(NAMETABLE_A);
    for (vram_addr = 0; vram_addr < 960; ++vram_addr) {
         ppu_write_data(0x20); // Assuming tile $20 is a blank space
    }
    // Clear Attribute Table (set all to palette 0)
    ppu_set_address(ATTRIBUTE_A);
     for (i = 0; i < 64; ++i) {
        ppu_write_data(0x00); // All areas use palette 0 initially
    }

    // --- Display HELLO NES Text (from first example) ---
    vram_addr = NAMETABLE_A + (TEXT_Y * 32) + TEXT_X;
    waitvsync();
    ppu_set_address(vram_addr);
    { // Use block scope for ptr and len
        const char* ptr = TEXT_STR;
        unsigned char len = strlen(ptr);
        for (i = 0; i < len; ++i) {
            unsigned char tile_index;
            if (ptr[i] >= 'A' && ptr[i] <= 'Z') tile_index = ptr[i] - 'A' + 0x41;
            else if (ptr[i] >= 'a' && ptr[i] <= 'z') tile_index = ptr[i] - 'a' + 0x41;
            else if (ptr[i] >= '0' && ptr[i] <= '9') tile_index = ptr[i] - '0' + 0x30;
            else if (ptr[i] == ' ') tile_index = 0x20;
            else if (ptr[i] == '!') tile_index = 0x21;
            else tile_index = 0x20;
            ppu_write_data(tile_index);
        }
        // Set attribute bits for the text
        for(i = 0; i < len; ++i) {
            // Call for each tile; function handles attribute byte granularity
             set_tile_palette(TEXT_X + i, TEXT_Y, TEXT_PALETTE_IDX);
        }
    }
    // --- End Text Display ---


    // 4. Initialize OAM Buffer in RAM ($0200)
    // Hide all sprites initially by setting Y >= $F0
    memset(oam_buffer, 0xF0, 256); // Set all Y coordinates offscreen

    // Initialize our visible sprite (Sprite 0)
    sprite_x = 128; // Center X
    sprite_y = 112; // Center Y

    // OAM Byte 0: Y position (top edge - 1)
    oam_buffer[0] = sprite_y - 1; // OAM Y is screen Y - 1
    // OAM Byte 1: Tile index
    oam_buffer[1] = SPRITE_TILE;
    // OAM Byte 2: Attributes
    // Bits 1:0 = Palette index (0-3, relative to $3F10)
    // Bit 5 = Priority (0: front, 1: behind BG)
    // Bit 6 = Flip Horizontally
    // Bit 7 = Flip Vertically
    oam_buffer[2] = (SPRITE_PALETTE & 0x03); // Use palette 0, no flip, front prio
    // OAM Byte 3: X position (left edge)
    oam_buffer[3] = sprite_x;

    // --- Turn Rendering On ---
    waitvsync();
    PPU.scroll = 0x00; // Reset scroll registers
    PPU.scroll = 0x00;
    PPU.mask = 0x1E;   // Screen ON: Show Background, Show Sprites, Show edges

    // --- Main Game Loop ---
    while (1) {
        waitvsync(); // Wait for the start of the vertical blank interval

        // --- Update OAM Buffer in RAM ($0200) ---
        // Update Sprite 0's position based on current sprite_x, sprite_y
        oam_buffer[0] = sprite_y - 1; // OAM Y is screen Y - 1
        oam_buffer[3] = sprite_x;
        // We could update tile/attributes here too if needed (oam_buffer[1], oam_buffer[2])

        // --- Trigger OAM DMA ---
        // Copy the entire 256 bytes from oam_buffer ($0200) to PPU OAM
        trigger_oam_dma();

        // --- Read Input ---
        joy_status = read_joypad1(); // Raw bits: R L D U T S B A

        // --- Update Logic ---
        // Check D-Pad bits (remembering the order from read_joypad1: R L D U ...)
        // Bit 4: Up (0x10)
        if ((joy_status & 0x10) && sprite_y > MIN_Y) {
            sprite_y--;
        }
        // Bit 5: Down (0x20)
        if ((joy_status & 0x20) && sprite_y < MAX_Y) {
            sprite_y++;
        }
        // Bit 6: Left (0x40)
        if ((joy_status & 0x40) && sprite_x > MIN_X) {
            sprite_x--;
        }
        // Bit 7: Right (0x80)
        if ((joy_status & 0x80) && sprite_x < MAX_X) {
            sprite_x++;
        }

        // (Loop repeats)
    }

    return 0; // Never reached
}