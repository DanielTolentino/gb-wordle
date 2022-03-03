// gfx_common.c

#include <gbdk/platform.h>
#include <gbdk/incbin.h>
#include <gb/gbdecompress.h>

#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "common.h"
#include "gfx_common.h"
#include "gameboy_color.h"

#include "window.h"

// Printing cursor position, updated by print_gotoxy
uint8_t * print_vram_addr = NULL;

uint8_t map_decomp_buf[DEVICE_SCREEN_WIDTH * DEVICE_SCREEN_HEIGHT];

uint8_t board_letters_decomp_buf[BOARD_LETTERS_COUNT * BOARD_LETTERS_TILES_PER * BOARD_LETTERS_BYTES_PER_TILE];
uint8_t font_letters_decomp_buf[FONT_LETTERS_COUNT * FONT_LETTERS_BYTES_PER_TILE];

// Cursor sprite flipping flags to allow use of same tile for all 4 corners
// const uint8_t sp_cursor_kbd_props[] = { 0x00, S_FLIPX, S_FLIPY, S_FLIPX | S_FLIPY };
// const uint8_t sp_cursor_offset_x[] = { 0, 8, 0, 8 };
// const uint8_t sp_cursor_offset_y[] = { 0, 0, 8, 8 };


// Blank tile, could be optimized
const uint8_t tile_blank[] = {0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00};


// Move printing cursor on screen
void print_gotoxy(uint8_t x, uint8_t y, uint8_t target) {

    if (target == PRINT_BKG)
        print_vram_addr = get_bkg_xy_addr(x,y);
    else
        print_vram_addr = get_win_xy_addr(x,y);
}


/*
// TODO: DELETE IF NOT IN USE ANYMORE - switch to on-demand printinf to vram or, re-add the font tiles if there is space
// TODO: OPTIMIZE?
// Draws a character (only handles A - Z)
// Advances to next VRAM location
void print_char(char letter) {

    if (letter == ' ') {
        set_vram_byte( print_vram_addr, BG_TILES_BLANK_START );
    }
    else if ((letter >= 'A') && (letter <= 'Z')) {
        set_vram_byte( print_vram_addr, (letter - 'A') + BG_TILES_FONT_START);
    }
    print_vram_addr++;
}*/


// Print a string on background or window at location set by print_gotoxy()
void print_str(char * txt) {

    uint8_t letter;
    uint8_t line_wrap_x_addr = (((uint8_t)print_vram_addr) & 0x1Fu);// (uint8_t)((uint16_t)print_vram_addr & 0x001Fu);

    while(*txt) {

        if (*txt == '\n') {
            // Mask out X location, reset it to line wrap location, move to next line
            print_vram_addr = (uint16_t *)(((uint16_t)print_vram_addr & 0xFFE0u) + (line_wrap_x_addr + 0x20u));
            txt++;
            continue;
        }
        else if(*txt >= 'A' && *txt <= 'Z') {
            letter = BG_TILES_FONT_START + (unsigned char)(*txt - 'A');
        } else if(*txt >= 'a' && *txt <= 'z') {
            letter = BG_TILES_FONT_START + (unsigned char)(*txt - 'a');
        } else if(*txt >= '0' && *txt <= '9') {
            letter = BG_TILES_FONT_NUM_START + (unsigned char)(*txt - '0');
        } else if(*txt == '.')
            letter = BG_TILES_FONT_PERIOD;
        else if(*txt == '!')
            letter = BG_TILES_FONT_EXCLAIM;
        else if(*txt == ':')
            letter = BG_TILES_FONT_COLON;
        else {
            // Default is blank tile for Space or any other unknown chars
            letter = BG_TILES_BLANK_START;
        }

        set_vram_byte(print_vram_addr++, letter);
        txt++; // Next character
    }
}


// Direct render a keyboard letter to VRAM to fill a keyboard entry
// Previous calls to set_1bpp_colors() will affect colors produced here
// Keyboard entries are 1 byte per entry on the map
void draw_letter_to_tileid(uint8_t letter, uint8_t index) {

    // if ((letter == ' ') || (letter == 0x00)) letter = BOARD_LETTERS_SPACE_CHAR;
    if (letter >= 'a') letter -= 'a';
    else if (letter >= 'A') letter -= 'A';

    // Draw letter into VRAM address for desired keyboard tile
    set_bkg_1bpp_data(index, 1, font_letters_decomp_buf + (letter * FONT_LETTERS_BYTES_PER_TILE));
}


// Set up graphics
void gfx_load(void) {

    uint8_t c;

    // Map Data
    SET_PRINT_COLOR_NORMAL;

    // Load 2bpp blank tile
    gb_decompress_bkg_data((BG_TILES_BLANK_START), tile_blank);


    // == Font Numbers ==
    //
    // ** borrows font_letters_decomp_buf temporarily before it's actual use **
    //
    // Load 1bpp font num tiles
    gb_decompress(font_num_tiles, font_letters_decomp_buf);
    // Load tiles into vram for font printing
    set_bkg_1bpp_data(BG_TILES_FONT_NUM_START, BG_TILES_FONT_NUM_LEN, font_letters_decomp_buf);


    // == Dialog Window Tiles / Squares ==
    //
    // ** borrows board_letters_decomp_buf temporarily before it's actual use **
    //
    // Load 1bpp intro / window dialog tiles
    gb_decompress(intro_dialog_tiles, board_letters_decomp_buf);
    // Load tiles into vram
    set_bkg_1bpp_data(BG_TILES_INTRO_DIALOG_START, BG_TILES_INTRO_DIALOG_LEN, board_letters_decomp_buf);
    // Decompress map for later use
    gb_decompress(intro_dialog_map, map_decomp_buf);

    // == Font Letters ==
    // Load 1bpp font tiles (used by both keyboard for VRAM drawing and print as a VRAM tileset)
    gb_decompress(font_tiles, font_letters_decomp_buf);
    // Load tiles into vram for font printing
    set_bkg_1bpp_data(BG_TILES_FONT_START, BG_TILES_FONT_LEN, font_letters_decomp_buf);

    // == Game Board Letter Tiles / Squares ==
    // Decompress board letter tiles into a buffer, they get written to VRAM later as needed
    gb_decompress(board_letter_tiles, board_letters_decomp_buf);



    // == Cursors ==
    // Sprite Data
    // Load 4bpp gbcompressed sprite cursor data
    gb_decompress_sprite_data((SP_TILES_CURSOR_START), cursor_tiles);
    // set_sprite_data((SP_TILES_CURSOR_START), SP_TILES_CURSOR_COUNT_TOTAL, cursor_tiles);

    SPRITES_8x8;

    // Keyboard Cursor
    set_sprite_tile(SP_ID_CURSOR_KBD_START + 0u, SP_TILES_CURSOR_KBD_START);
    set_sprite_tile(SP_ID_CURSOR_KBD_START + 1u, SP_TILES_CURSOR_KBD_START);
    set_sprite_tile(SP_ID_CURSOR_KBD_START + 2u, SP_TILES_CURSOR_KBD_START);
    set_sprite_tile(SP_ID_CURSOR_KBD_START + 3u, SP_TILES_CURSOR_KBD_START);

    set_sprite_prop(SP_ID_CURSOR_KBD_START + 0u, 0x00u);
    set_sprite_prop(SP_ID_CURSOR_KBD_START + 1u, S_FLIPX);
    set_sprite_prop(SP_ID_CURSOR_KBD_START + 2u, S_FLIPY);
    set_sprite_prop(SP_ID_CURSOR_KBD_START + 3u, S_FLIPX | S_FLIPY);
    // Takes more ROM space:
    // c = 0;
    // for (uint8_t i = SP_ID_CURSOR_KBD_START; i < (SP_ID_CURSOR_KBD_START + SP_ID_CURSOR_KBD_LEN); i++) {
    //     set_sprite_tile(i, SP_TILES_CURSOR_KBD_START);
    //     set_sprite_prop(i, sp_cursor_kbd_props[c++]);
    // }


    // Board Row Cursor
    c = 0;
    for (uint8_t i = SP_ID_CURSOR_BOARD_START; i < (SP_ID_CURSOR_BOARD_START + SP_ID_CURSOR_BOARD_LEN); i++) {
        set_sprite_tile(i, SP_TILES_CURSOR_BOARD_START + c++);
        set_sprite_prop(i, CGB_PAL_3);
    }

    // Clear window and move it offscreen at the bottom
    move_win(0 + WIN_X_OFFSET, DEVICE_SCREEN_PX_HEIGHT); // Window is offscreen by default

    // Center screen by scrolling slightly to the left
    // move_bkg(252, 252);
    move_bkg(252, 0);
}
