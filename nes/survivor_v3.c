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
#define PLAYER_SPRITE_TILE     0x05   // !!! TILE $05 MUST HAVE GRAPHICS IN YOUR CHR !!!
#define PLAYER_SPRITE_PALETTE  0
#define PLAYER_OAM_OFFSET      0 // OAM starts at index 0 (bytes 0-3)
#define PLAYER_SPRITE_WIDTH    8
#define PLAYER_SPRITE_HEIGHT   8

// Enemy Configuration
#define ENEMY_SPRITE_TILE      0x06   // !!! TILE $06 MUST HAVE GRAPHICS IN YOUR CHR !!!
#define ENEMY_SPRITE_PALETTE   1
#define ENEMY_SPRITE_WIDTH     8
#define ENEMY_SPRITE_HEIGHT    8
#define MAX_ENEMIES            30 // Max active enemies (ensure MAX_ENEMIES + 1 <= MAX_SPRITES)

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

// --- Global Variables ---
// Use a pointer for OAM buffer for clarity that it maps to hardware address
unsigned char* const oam_buffer = (unsigned char*)OAM_ADDRESS;
unsigned char player_x;
unsigned char player_y;
Enemy enemies[MAX_ENEMIES];
unsigned char active_enemy_count = 0;
unsigned int score = 0;
unsigned char score_changed = 0; // Flag to update score display
unsigned char frame_count = 0;   // Counter for spawning
unsigned int random_seed = 1;    // Seed for PRNG

// --- PRNG ---
// Simple Pseudo-Random Number Generator (Linear Congruential Generator)
unsigned char pseudo_rand(void) {
    random_seed = (random_seed * 1103515245 + 12345);
    // Return one byte of the result
    return (unsigned char)((random_seed >> 8) & 0xFF);
}

// --- PPU Helpers ---
// Set the PPU VRAM address for subsequent reads/writes
void ppu_set_address(unsigned int addr) {
    PPU.vram.address = (addr >> 8); // High byte
    PPU.vram.address = (addr & 0xFF); // Low byte
}

// Write a single byte to the PPU VRAM at the current address
void ppu_write_data(unsigned char data) {
    PPU.vram.data = data;
}

// Trigger the OAM DMA transfer (copies 256 bytes from CPU RAM page OAM_PAGE to PPU OAM)
void trigger_oam_dma(void) {
    APU.sprite.dma = OAM_PAGE;
    // This takes 513-514 CPU cycles, happens "instantly" from C code perspective
}

// --- Input ---
// Read the status of Joypad 1
unsigned char read_joypad1(void) {
    unsigned char status = 0, i;
    JOYPAD[0] = 1; // Strobe controller 1 to latch button states
    JOYPAD[0] = 0; // Clear strobe
    // Read the 8 button states (A, B, Select, Start, Up, Down, Left, Right)
    for (i = 0; i < 8; ++i) {
        status >>= 1; // Make space for the next bit
        if (JOYPAD[0] & 1) { // Check the least significant bit
            status |= 0x80; // Set the most significant bit if button is pressed
        }
    }
    // In nes.h: JOY_A_MASK=0x80, JOY_B_MASK=0x40, ..., JOY_RIGHT_MASK=0x01
    return status;
}

// --- Palette ---
// Define the color palettes (32 bytes total) - USING THE USER'S ORIGINAL
const unsigned char palette[32] = {
    /* Background Palettes */
    COLOR_BLACK, COLOR_BLUE, COLOR_BLUE, COLOR_BLUE,            // BG Pal 0
    COLOR_BLACK, COLOR_WHITE, COLOR_RED, COLOR_YELLOW,          // BG Pal 1 (Text)
    COLOR_BLACK, COLOR_GREEN, COLOR_LIGHTGREEN, COLOR_WHITE,    // BG Pal 2
    COLOR_BLACK, COLOR_RED, COLOR_LIGHTRED, COLOR_WHITE,        // BG Pal 3

    /* Sprite Palettes */
    COLOR_BLACK, COLOR_WHITE, COLOR_RED, COLOR_BLUE,            // Sprite Pal 0 (Player)
    COLOR_BLACK, COLOR_YELLOW, COLOR_ORANGE, COLOR_BROWN,       // Sprite Pal 1 (Enemy)
    COLOR_BLACK, COLOR_CYAN, COLOR_LIGHTBLUE, COLOR_WHITE,      // Sprite Pal 2
    COLOR_BLACK, COLOR_VIOLET, COLOR_LIGHTRED, COLOR_WHITE      // Sprite Pal 3
};

// --- Text Display ---
#define SCORE_TEXT_X 10 // Tile X position for "SCORE "
#define SCORE_TEXT_Y 2  // Tile Y position for score line
#define SCORE_DIGIT_X (SCORE_TEXT_X + 6) // Tile X position for first score digit
#define SCORE_MAX_DIGITS 5 // How many digits to display for the score
#define SCORE_TEXT_PALETTE_IDX 1 // Use BG Palette 1 for the score text

// Helper to set the palette for a region of the background attribute table
// NOTE: This is a simplified version, might cause palette clashes if tiles
//       aren't aligned to 16x16 pixel (2x2 tile) attribute boundaries.
void set_tile_palette(unsigned char x_tile, unsigned char y_tile, unsigned char pal_idx, unsigned char width_in_tiles) {
    unsigned int start_attr_addr;
    unsigned char start_attr_col, end_attr_col, attr_row, current_attr_col;
    unsigned char attr_byte_value, shift_amount, mask;

    attr_row = y_tile / 4; // Which row in the attribute table (8 rows total)
    start_attr_col = x_tile / 4; // Which column in the attribute table (8 columns total)
    end_attr_col = (x_tile + width_in_tiles - 1) / 4; // Last column affected

    for (current_attr_col = start_attr_col; current_attr_col <= end_attr_col; ++current_attr_col) {
        // Calculate the address of the attribute byte
        start_attr_addr = ATTRIBUTE_A + (attr_row * 8) + current_attr_col;

        // Determine which 2x2 tile quadrant we are in within the 4x4 attribute block
        // Top-Left:    bits 1:0
        // Top-Right:   bits 3:2
        // Bottom-Left: bits 5:4
        // Bottom-Right:bits 7:6
        shift_amount = ((y_tile % 4) / 2) * 4 + ((x_tile % 4) / 2) * 2;
        mask = 0xFF ^ (3 << shift_amount); // Create a mask to clear the relevant 2 bits
        attr_byte_value = (pal_idx & 0x03) << shift_amount; // Prepare the new palette bits

        // NOTE: Reading attribute RAM is tricky/slow. This simplified version just overwrites
        // the whole attribute byte assuming adjacent areas use the same palette or are unused.
        // A more robust version would read the existing byte, mask, OR, and write back.
        // For simple text display, overwriting often works if the layout is planned.
        // Let's just set the whole byte for simplicity here, assuming it's okay for the score area.
        // We'll make all four 16x16 blocks in this attribute cell use the score palette.
        ppu_set_address(start_attr_addr);
        ppu_write_data((pal_idx * 0x55)); // Sets all 4 quadrants to pal_idx
    }
}


// Writes score digits to VRAM (assumes PPU address is already set)
// Assumes tiles '0' through '9' are at CHR addresses 0x30-0x39
void write_score_digits_vram(unsigned int s) {
    unsigned char i, digit_tile;
    unsigned char leading_zero = 1; // Flag to suppress leading zeros
    unsigned int divisor = 10000; // For 5 digits

    for(i = 0; i < SCORE_MAX_DIGITS; ++i) {
        digit_tile = (s / divisor) % 10; // Get the current digit

        if (digit_tile != 0 || !leading_zero || i == SCORE_MAX_DIGITS - 1) {
            // Write the digit tile ('0' + digit value)
            ppu_write_data(digit_tile + 0x30); // Assumes ASCII-like mapping in CHR
            leading_zero = 0; // Stop suppressing zeros after the first non-zero digit
        } else {
            // Write a blank tile (or space, tile 0x00 assumed blank) for leading zeros
            ppu_write_data(0x00); // Use tile $00 for space/blank
        }
        divisor /= 10; // Move to the next digit place
    }
}

// Updates the score display on the screen
void update_score_display(void) {
    unsigned int addr = NAMETABLE_A + (SCORE_TEXT_Y * 32) + SCORE_DIGIT_X;
    ppu_set_address(addr); // Set PPU address to where the score digits start
    write_score_digits_vram(score); // Write the digits
}

// --- Collision ---
// Simple Axis-Aligned Bounding Box (AABB) collision check
// Assumes both player and enemy are 8x8 pixels
unsigned char check_collision(unsigned char x1, unsigned char y1, unsigned char x2, unsigned char y2) {
    // Check for overlap on X axis and Y axis
    return (x1 < (x2 + ENEMY_SPRITE_WIDTH) && (x1 + PLAYER_SPRITE_WIDTH) > x2 &&
            y1 < (y2 + ENEMY_SPRITE_HEIGHT) && (y1 + PLAYER_SPRITE_HEIGHT) > y2);
}

// --- Enemy Spawning ---
void spawn_enemy(void) {
    unsigned char i;
    unsigned char spawn_side;
    signed int spawn_x_s, spawn_y_s; // Use signed temporary for calculation ease

    // Find an inactive enemy slot
    for (i = 0; i < MAX_ENEMIES; ++i) {
        if (!enemies[i].active) {
            enemies[i].active = 1;
            active_enemy_count++;

            // Choose a random side to spawn from (0=Top, 1=Bottom, 2=Left, 3=Right)
            spawn_side = pseudo_rand() & 3; // Use lower 2 bits for 0-3

            // Calculate spawn position just off-screen
            switch (spawn_side) {
                case 0: // Top
                    spawn_x_s = MIN_X + (pseudo_rand() % (MAX_X - MIN_X + 1));
                    spawn_y_s = MIN_Y - SPAWN_MARGIN;
                    break;
                case 1: // Bottom
                    spawn_x_s = MIN_X + (pseudo_rand() % (MAX_X - MIN_X + 1));
                    spawn_y_s = MAX_Y + SPAWN_MARGIN;
                    break;
                case 2: // Left
                    spawn_x_s = MIN_X - SPAWN_MARGIN;
                    spawn_y_s = MIN_Y + (pseudo_rand() % (MAX_Y - MIN_Y + 1));
                    break;
                case 3: // Right
                default: // Add default to handle potential randomness issues
                    spawn_x_s = MAX_X + SPAWN_MARGIN;
                    spawn_y_s = MIN_Y + (pseudo_rand() % (MAX_Y - MIN_Y + 1));
                    break;
            }

            // Clamp coordinates to unsigned char range after calculation
            // Note: Off-screen coordinates are fine initially, they will move on screen.
            // Be careful with underflow/overflow if SPAWN_MARGIN is large.
            if (spawn_x_s < 0) enemies[i].x = 0;
            else if (spawn_x_s > 255) enemies[i].x = 255;
            else enemies[i].x = (unsigned char)spawn_x_s;

            if (spawn_y_s < 0) enemies[i].y = 0;
            else if (spawn_y_s > 255) enemies[i].y = 255;
            else enemies[i].y = (unsigned char)spawn_y_s;

            return; // Exit after spawning one enemy
        }
    }
    // If no inactive slot found, do nothing
}

// --- Main ---
int main(void) {
    unsigned char i;
    unsigned char joy_status;
    unsigned int vram_addr;
    unsigned char oam_idx; // Index into oam_buffer (tracks current sprite slot * 4)

    // --- Initial Setup ---
    // Disable PPU rendering and NMI during setup
    PPU.control = 0x00; // NMI OFF, standard sprites/BG addresses
    PPU.mask = 0x00;    // Rendering OFF (BG and Sprites)

    // Wait for VBlank to ensure PPU is idle
    waitvsync();

    // Load palette data into PPU RAM ($3F00 - $3F1F)
    ppu_set_address(PALETTE_RAM);
    for (i = 0; i < 32; ++i) {
        ppu_write_data(palette[i]);
    }

    // Clear Nametable A ($2000 - $23BF) - Fill with tile $00 (blank)
    ppu_set_address(NAMETABLE_A);
    for (vram_addr = 0; vram_addr < 960; ++vram_addr) { // 32x30 tiles
        ppu_write_data(0x00);
    }

    // Clear Attribute Table A ($23C0 - $23FF) - Set all palettes to 0
    ppu_set_address(ATTRIBUTE_A);
    for (i = 0; i < 64; ++i) { // 8x8 attribute entries
        ppu_write_data(0x00); // All palette 0
    }

    // Set palette for the score text area
    set_tile_palette(SCORE_TEXT_X, SCORE_TEXT_Y, SCORE_TEXT_PALETTE_IDX, 6 + SCORE_MAX_DIGITS);

    // Write "SCORE " text to VRAM
    // Assumes A=0x41, B=0x42 etc. in your CHR font map
    vram_addr = NAMETABLE_A + (SCORE_TEXT_Y * 32) + SCORE_TEXT_X;
    ppu_set_address(vram_addr);
    ppu_write_data('S'-'A'+0x41); // S
    ppu_write_data('C'-'A'+0x41); // C
    ppu_write_data('O'-'A'+0x41); // O
    ppu_write_data('R'-'A'+0x41); // R
    ppu_write_data('E'-'A'+0x41); // E
    ppu_write_data(0x00); // Space (using blank tile $00)

    // Display initial score (0)
    update_score_display(); // Writes the digits '00000'

    // Initialize local OAM buffer (all sprites hidden)
    // This buffer resides at $0200-$02FF in CPU RAM
    memset(oam_buffer, HIDE_SPRITE_Y, 256);

    // Initialize player position (center screen approx)
    player_x = 128;
    player_y = 112;

    // Initialize enemies (all inactive)
    for (i = 0; i < MAX_ENEMIES; ++i) {
        enemies[i].active = 0;
    }
    active_enemy_count = 0;
    score = 0;
    score_changed = 0;
    frame_count = 0;

    // Seed the random number generator
    // You might want a better way to seed if possible (e.g., frame counter on title screen)
    random_seed = 123; // Fixed seed for now

    // --- Turn Rendering On ---
    // Wait for VBlank again before enabling PPU
    waitvsync();
    PPU.scroll = 0x00; // Reset scroll registers
    PPU.scroll = 0x00;
    PPU.mask = 0x1E;    // Enable BG, Sprites, Leftmost 8px of BG/Sprites ON
    PPU.control = 0x90; // NMI ON, Sprites from $0000, BG from $1000 (Change if needed!)
                        // If your BG tiles are in the first 256 CHR tiles, use 0x80
                        // Use 0x90 if sprites are $0000, BG $1000

    // --- Main Loop ---
    while (1) {
        // --- Start of Frame ---
        waitvsync(); // Wait for VBlank signal (NMI handler runs here implicitly)

        // --- PPU Updates (during VBlank) ---
        // 1. Send the OAM data prepared *last* frame to the PPU
        trigger_oam_dma();

        // 2. Update score display in VRAM if it changed
        if (score_changed) {
            update_score_display();
            score_changed = 0; // Reset flag
        }

        // --- Prepare OAM Buffer for NEXT frame (Best Practice) ---
        // 1. Hide ALL sprites in the buffer first. Crucial step!
        memset(oam_buffer, HIDE_SPRITE_Y, 256);

        // 2. Write ONLY the player's current data into the buffer.
        //    Check if player Y is valid for drawing (not 0 and not >= HIDE_SPRITE_Y)
        if (player_y >= 1 && player_y < HIDE_SPRITE_Y) {
            oam_buffer[PLAYER_OAM_OFFSET + 0] = player_y - 1; // Y pos (Y-1 rule)
            oam_buffer[PLAYER_OAM_OFFSET + 1] = PLAYER_SPRITE_TILE; // Tile index
            oam_buffer[PLAYER_OAM_OFFSET + 2] = (PLAYER_SPRITE_PALETTE & 0x03); // Attributes (Palette)
            oam_buffer[PLAYER_OAM_OFFSET + 3] = player_x; // X pos
        }
        // No else needed: if player Y is invalid, the memset already hid the sprite.

        // 3. Add active enemies after the player.
        oam_idx = PLAYER_OAM_OFFSET + 4; // Start enemies at sprite slot 1 (index 4)
        for (i = 0; i < MAX_ENEMIES; ++i) {
            if (enemies[i].active) {
                 // Check if there is space left in OAM (max 64 sprites total)
                 if (oam_idx < (MAX_SPRITES * 4)) {
                    // Ensure enemy Y is valid before writing to OAM
                    if(enemies[i].y >= 1 && enemies[i].y < HIDE_SPRITE_Y) {
                         oam_buffer[oam_idx + 0] = enemies[i].y - 1; // Y pos (Y-1)
                         oam_buffer[oam_idx + 1] = ENEMY_SPRITE_TILE; // Tile Index
                         oam_buffer[oam_idx + 2] = (ENEMY_SPRITE_PALETTE & 0x03); // Attributes (Palette)
                         oam_buffer[oam_idx + 3] = enemies[i].x; // X pos

                         oam_idx += 4; // Move to next sprite slot *only if this one was written*
                    }
                    // If enemy Y is invalid, we implicitly leave its OAM slot hidden (from memset)
                 } else {
                    break; // Stop processing enemies if OAM buffer is full
                 }
            }
        }
        // 4. No need for a final loop to hide remaining sprites, memset did it at the start.


        // --- Game Logic (updates state for the *next* frame) ---
        // Read Input
        joy_status = read_joypad1();
        if ((joy_status & JOY_UP_MASK)    && player_y > MIN_Y) player_y--;
        if ((joy_status & JOY_DOWN_MASK)  && player_y < MAX_Y) player_y++;
        if ((joy_status & JOY_LEFT_MASK)  && player_x > MIN_X) player_x--;
        if ((joy_status & JOY_RIGHT_MASK) && player_x < MAX_X) player_x++;

        // --- Enemy Logic ---
        // Spawning
        frame_count++;
        if ((frame_count >= SPAWN_INTERVAL) && (active_enemy_count < MAX_ENEMIES)) {
             spawn_enemy(); // Try to spawn a new enemy
             frame_count = 0; // Reset spawn timer
        }

        // Movement & Collision
        for (i = 0; i < MAX_ENEMIES; ++i) {
            if (enemies[i].active) {
                // Simple movement: Move one pixel towards player per frame
                if (enemies[i].y < player_y) enemies[i].y++;
                else if (enemies[i].y > player_y) enemies[i].y--;

                if (enemies[i].x < player_x) enemies[i].x++;
                else if (enemies[i].x > player_x) enemies[i].x--;

                // Check for collision with player
                if (check_collision(player_x, player_y, enemies[i].x, enemies[i].y)) {
                    // On collision: Deactivate enemy, decrement count, increase score
                    enemies[i].active = 0;
                    active_enemy_count--;
                    score++;
                    score_changed = 1; // Flag that score needs redraw next VBlank
                }
            }
        } // End enemy loop

        // --- End of Frame Logic ---
        // The loop repeats, starting with waitvsync()

    } // End while(1)

    // return 0; // Unreachable in NES programming
}