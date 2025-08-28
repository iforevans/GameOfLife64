// Conway's Game of Life for Commodore 64
// By Ifor Evans
// Tooling: VS Code, VS64 Extension, and the Oscar64 Compiler

// Toroidal 40x25 grid. Pointer-swapped cell buffers + single screen buffer.
// Start menu (Random / Draw / Presets) prints in lower/uppercase (PETSCII)

#include <stdlib.h>
#include <conio.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>

#define WIDTH 40
#define HEIGHT 25
#define BWIDTH (WIDTH + 2)
#define BHEIGHT (HEIGHT + 2)

// PETSCII codes for cursor keys on C64:
// RIGHT=0x1D, LEFT=0x9D, DOWN=0x11, UP=0x91
const unsigned char KEY_RIGHT = 0x1D;
const unsigned char KEY_LEFT  = 0x9D;
const unsigned char KEY_DOWN  = 0x11;
const unsigned char KEY_UP    = 0x91;

// Map (y,x) to correct index in our 1D buffer (row-major)
#define IDX(y,x) ((y) * BWIDTH + (x))

// Cell buffers (swapped via pointers)
static unsigned char buf0[BHEIGHT * BWIDTH];
static unsigned char buf1[BHEIGHT * BWIDTH];
static unsigned char *current = buf0;
static unsigned char *next    = buf1;

// Single off-screen display buffer (chars for the frame to be shown next)
static unsigned char screenBuf[WIDTH * HEIGHT];

// Live and Dead chars
#define LIVE_CHAR 0x51
#define DEAD_CHAR ' '

// C64 screen memory
static unsigned char *screen = (unsigned char *)0x0400;

// --- Preset patterns ---
static const signed char P_BLOCK[][2]   = { {0,0},{1,0},{0,1},{1,1} };
static const signed char P_BLINKER[][2] = { {0,0},{1,0},{2,0} };
static const signed char P_GLIDER[][2]  = { {1,0},{2,1},{0,2},{1,2},{2,2} };
static const signed char P_GGUN[][2] =
{
    {0,4},{1,4},{0,5},{1,5},
    {10,4},{10,5},{10,6},{11,3},{11,7},{12,2},{12,8},{13,2},{13,8},{14,5},{15,3},{15,7},{16,4},{16,5},{16,6},{17,5},
    {20,2},{20,3},{20,4},{21,2},{21,3},{21,4},{22,1},{22,5},{24,0},{24,1},{24,5},{24,6},
    {34,2},{34,3},{35,2},{35,3}
};

#define N_BLOCK   (sizeof(P_BLOCK)/sizeof(P_BLOCK[0]))
#define N_BLINKER (sizeof(P_BLINKER)/sizeof(P_BLINKER[0]))
#define N_GLIDER  (sizeof(P_GLIDER)/sizeof(P_GLIDER[0]))
#define N_GGUN    (sizeof(P_GGUN)/sizeof(P_GGUN[0]))

// Branch-free rule tables
// (This is very cool, and I cannot take credit for this innovation)
static const unsigned char next_from_dead[9]  = {0,0,0,1,0,0,0,0,0};
static const unsigned char next_from_alive[9] = {0,0,1,1,0,0,0,0,0};

// Charset helpers
static inline void set_uppercase(void)
{
    volatile unsigned char * const D018 = (unsigned char*)0xD018;
    *D018 = (unsigned char)(*D018 & ~0x02);
}

static inline void set_lowercase(void)
{
    volatile unsigned char * const D018 = (unsigned char*)0xD018;
    *D018 = (unsigned char)(*D018 | 0x02);
}

// Copy horizontal and vertical borders to make the wrapping logic simpler
void update_borders(void)
{
    // Horizontal wrap: fix left/right border cells for each inner row.
    unsigned char *row = current + IDX(1,0);
    for (int y = 1; y <= HEIGHT; ++y, row += BWIDTH)
    {
        row[0] = row[WIDTH];        // left border <= right edge
        row[BWIDTH - 1] = row[1];   // right border <= left edge
    }

    // Vertical wrap: copy whole rows in one go (includes the updated borders).
    memcpy(current + IDX(0,0),           current + IDX(HEIGHT,0),  BWIDTH);          // top border row
    memcpy(current + IDX(BHEIGHT - 1,0), current + IDX(1,0),       BWIDTH);          // bottom border row
}

// Calculate the next gen, and build the NEXT frame's characters in screenBuf
void calc_next_gen(void)
{
    unsigned char *cur = current;
    unsigned char *nxt = next;

    for (int y = 1; y <= HEIGHT; ++y)
    {
        // Row offset in screenBuf
        int srow = (y - 1) * WIDTH;
        // Start index of row y in bordered grid
        int base = y * BWIDTH;

        for (int x = 1; x <= WIDTH; ++x)
        {
            unsigned char neighbours =
                cur[base - BWIDTH + x - 1] +
                cur[base - BWIDTH + x] +
                cur[base - BWIDTH + x + 1] +
                cur[base + x - 1] +
                cur[base + x + 1] +
                cur[base + BWIDTH+ x - 1] +
                cur[base + BWIDTH + x] +
                cur[base + BWIDTH   + x + 1];

            unsigned char alive = cur[base + x];
            unsigned char v = alive ? next_from_alive[neighbours] : next_from_dead[neighbours];

            // Write next state
            nxt[base + x] = v;

            // Build next frame
            screenBuf[srow + (x - 1)] = v ? LIVE_CHAR : DEAD_CHAR;
        }
    }
}

void initialize_grid_random(void)
{
    //  Get a (semi)random seed for srand by using the current raster line
    volatile unsigned char *raster = (unsigned char *)0xD012;
    srand(*raster);

    //  Clear the grid
    memset(current, 0, BHEIGHT * BWIDTH);

    // Fill current cells and build the initial screenBuf (first frame)
    for (int y = 1; y <= HEIGHT; y++)
    {
        int srow = (y - 1) * WIDTH;
        for (int x = 1; x <= WIDTH; x++)
        {
            unsigned char v = (unsigned char)(rand() & 1);
            current[IDX(y,x)] = v;
            screenBuf[srow + (x - 1)] = v ? LIVE_CHAR : DEAD_CHAR;
        }
    }
}

// Build screenBuf from current (used after editing/presets), then show it
static void build_screen_from_current(void)
{
    for (int y = 1; y <= HEIGHT; ++y)
    {
        int srow = (y - 1) * WIDTH;
        for (int x = 1; x <= WIDTH; ++x)
        {
            unsigned char v = current[IDX(y,x)];
            screenBuf[srow + (x - 1)] = v ? LIVE_CHAR : DEAD_CHAR;
        }
    }
}

// Fast display: copy the prepared chars to the visible screen
void update_display(void)
{
    memcpy(screen, screenBuf, WIDTH * HEIGHT);
}

void set_colours(void)
{
    bgcolor(COLOR_BLACK);
    bordercolor(COLOR_BLACK);
    textcolor(COLOR_LT_GREEN);
}

// Clear entire inner grid (and screenBuf)
static void clear_grid(void)
{
    memset(current, 0, BHEIGHT * BWIDTH);
    for (int y = 0; y < HEIGHT; ++y) {
        memset(screenBuf + y * WIDTH, DEAD_CHAR, WIDTH);
    }
    update_display();
}

// Simple editor (cursor keys move, SPACE toggle, X clears all, C clears row, ENTER starts)
// Rewritten to use switch/case for key handling.
static void draw_editor(void)
{
    // Use graphics charset in editor so LIVE_CHAR shows as filled circle
    set_uppercase();

    // Set up display
    build_screen_from_current();
    update_display();

    // Valid values for co-ords = 1..WIDTH / 1..HEIGHT (0 is border wraps)
    // And initial position in the middle of the screen
    int cx = WIDTH/2, cy = HEIGHT/2;

    while (true)
    {
        // Highlight initial cursor cell (reverse video)
        int pos = (cy - 1) * WIDTH + (cx - 1);
        unsigned char orig = screen[pos];
        screen[pos] = (unsigned char)(orig | 0x80);   // reverse bit set

        // Wait for key
        unsigned char key = (unsigned char)getch();

        // Restore original cell
        screen[pos] = orig;

        switch (key)
        {
            // Toggle current cell?
            case ' ':
            {
                unsigned char v = (unsigned char)(current[IDX(cy,cx)] ^ 1);
                current[IDX(cy,cx)] = v;
                screenBuf[pos] = v ? LIVE_CHAR : DEAD_CHAR;
                screen[pos]     = screenBuf[pos];
            } break;

            // Clear all?
            case 'x':
            case 'X':
                clear_grid();
                break;

            // Clear row?
            case 'c':
            case 'C':
            {
                for (int x = 1; x <= WIDTH; ++x) current[IDX(cy,x)] = 0;
                memset(screenBuf + (cy - 1) * WIDTH, DEAD_CHAR, WIDTH);
                memcpy(screen + (cy - 1) * WIDTH, screenBuf + (cy - 1) * WIDTH, WIDTH);
            } break;

            // Start simulation?
            case 13:
            case 10:
                build_screen_from_current();
                update_display();
                return;

            case KEY_UP:
                cy = (cy > 1) ? (cy - 1) : HEIGHT;
                break;

            case KEY_DOWN:
                cy = (cy < HEIGHT) ? (cy + 1) : 1;
                break;

            case KEY_LEFT:
                cx = (cx > 1) ? (cx - 1) : WIDTH;
                break;

            case KEY_RIGHT:
                cx = (cx < WIDTH) ? (cx + 1) : 1;
                break;

            default:
                break;
        }
    }
}

// Draw a pattern (list of (dx,dy) pairs) with top-left anchor at (y0,x0)
static void draw_preset(int y0, int x0, const signed char (*pts)[2], int n)
{
    for (int i = 0; i < n; ++i)
    {
        int y = y0 + pts[i][1];
        int x = x0 + pts[i][0];
        if (y >= 1 && y <= HEIGHT && x >= 1 && x <= WIDTH)
        {
            current[IDX(y,x)] = 1;
            screenBuf[(y - 1) * WIDTH + (x - 1)] = LIVE_CHAR;
        }
    }
    update_display();
}

static void show_presets_menu(void)
{
    clrscr();
    gotoxy(0,0);
    printf(p"Presets:\r\r");
    printf(p"B=Block\r");
    printf(p"N=Blinker\r");
    printf(p"G=Glider\r");
    printf(p"U=Glider Gun\r\r");
    printf(p"Enter=cancel)\r");

    // Set start drawing pos
    int cx = WIDTH/2, cy = HEIGHT/2;

    // Wait for user keypress
    unsigned char key = (unsigned char)getch();
    switch (key)
    {
        case 'b': 
        case 'B':
            clear_grid();
            draw_preset(cy, cx, P_BLOCK, N_BLOCK);
            break;
        case 'n': 
        case 'N':
            clear_grid();
            draw_preset(cy, cx-1, P_BLINKER, N_BLINKER);
            break;
        case 'g': 
        case 'G':
            clear_grid();
            draw_preset(cy-1, cx-1, P_GLIDER, N_GLIDER);
            break;

        // Leave space for gliders to fly
        // It won't last long due to the C64's
        // small screen and the toroidal wraparound :-(
        case 'u': 
        case 'U':
            clear_grid();
            draw_preset(3, 2, P_GGUN, N_GGUN);
            break;
        default:
            break;
    }

    build_screen_from_current();
    update_display();
}

// Returns 1 to start the simulation, 0 to quit to BASIC.
static bool show_main_menu(void)
{
    // Menus in lower/uppercase charset (text looks normal)
    set_lowercase();         
    clrscr();
    gotoxy(0,0);
    printf(p"Conway's Game of Life\r\r");
    printf(p"1) Random start\r\r");
    printf(p"2) Draw your own\r");
    printf(p"   Cursor keys to move,\r");
    printf(p"   SPACE = toggle,\r");
    printf(p"   X = CLEAR ALL,\r");
    printf(p"   C = CLEAR ROW,\r");
    printf(p"   ENTER = START)\r\r");
    printf(p"3) Presets\r");
    printf(p"   Block, Blinker, Glider, Glider Gun\r\r");
    printf(p"4) Quit\r\r");
    printf(p"\rChoose 1-4: ");

    // Loop until done
    while (true)
    {
        //  Get user keypress
        unsigned char key = (unsigned char)getch();

        if (key == '1') 
        { 
            initialize_grid_random(); 
            return true;
        }

        if (key == '2') 
        { 
            clear_grid(); 
            draw_editor(); 
            return true; 
        }
        
        if (key == '3') 
        { 
            show_presets_menu(); 
            return true; 
        }

        if (key == '4') 
        { 
            return false; 
        }
    }
}

//  Main entry point for our app
int main(void)
{
    // Setup display
    set_colours();

    // Loop: menu -> simulate -> back to menu (until Quit)
    while (show_main_menu())
    {
        // Prepare to run simulation
        clrscr();
        set_uppercase();
        build_screen_from_current();
        update_display();

        // Simulation loop: any key returns to the main menu
        while (true)
        {
            // show frame prepared in previous iteration
            update_display();

            // wrap borders
            update_borders();

            // compute next gen + build next frame's chars
            calc_next_gen();

            // swap cells
            { unsigned char *tmp = current; current = next; next = tmp; }

            if (kbhit()) { getch(); break; }  // back to menu
        }
    }

    // Back to BASIC
    set_uppercase();
    clrscr();
    gotoxy(0,0);
    printf(p"goodbye!\r");
    
    // All done
    return 0;
}
