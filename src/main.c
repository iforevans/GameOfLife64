// Game of Life implementation for Commodore 64 using Oscar64 C compiler.
// Toroidal 40x25 grid. Uses pointer-swapped cell buffers and a single screen buffer.

#include <stdlib.h>
#include <conio.h>
#include <string.h>
#include <stdbool.h>

#define WIDTH 40
#define HEIGHT 25
#define BWIDTH (WIDTH + 2)
#define BHEIGHT (HEIGHT + 2)

// Map (y,x) to correct index in our 1D buffer
// Remember arrays in C are in Row Major Order ...
#define IDX(y,x) ((y) * BWIDTH + (x))    

// Current and Next Gen Cell buffers (swapped via pointers)
static unsigned char buf0[BHEIGHT * BWIDTH];
static unsigned char buf1[BHEIGHT * BWIDTH];
static unsigned char *current = buf0;    
static unsigned char *next    = buf1;    

// Single off-screen display buffer (chars for the frame to be shown next)
static unsigned char screenBuf[WIDTH * HEIGHT];

//  Live and Dead chars
#define LIVE_CHAR 0x51
#define DEAD_CHAR ' '

// C64 screen memory
static unsigned char *screen = (unsigned char *)0x0400;

void update_borders(void)
{
    // Horizontal wrap: fix left/right border cells for each inner row.
    // Use a row pointer to avoid recalculating IDX() every time.
    unsigned char *row = current + IDX(1,0);
    for (int y = 1; y <= HEIGHT; ++y, row += BWIDTH) 
    {
        row[0] = row[WIDTH];        // left border <= right edge
        row[BWIDTH - 1] = row[1];   // right border <= left edge
    }

    // Vertical wrap: copy whole rows in one go (includes the updated borders).
    // Top border row (y=0) <= bottom inner row (y=HEIGHT)
    memcpy(current + IDX(0,0), current + IDX(HEIGHT,0), BWIDTH);
    
    // Bottom border row (y=BHEIGHT-1) <= top inner row (y=1)
    memcpy(current + IDX(BHEIGHT - 1,0),  current + IDX(1,0), BWIDTH);
}

// Compute next gen and build the NEXT frame's characters in screenBuf
void calc_next_gen(void)
{
    // Tell the optimizer these point to *non-overlapping* buffers.
    unsigned char * cur = current;
    unsigned char * nxt = next;

    // Branch-free rule tables: next state given alive? and neighbor count.
    // Lookup is faster that multiple IFs and doesn't get messed with when optimized by the compiler
    //  This is really cool, and I cannot take credit for this idea ...
    static const unsigned char next_from_dead[9]  = {0,0,0,1,0,0,0,0,0};
    static const unsigned char next_from_alive[9] = {0,0,1,1,0,0,0,0,0};

    for (int y = 1; y <= HEIGHT; ++y) 
    {
        // Row offset in screenBuf
        int srow = (y - 1) * WIDTH;          
        // Start index of row y in bordered grid
        int base = y * BWIDTH;               

        for (int x = 1; x <= WIDTH; ++x) 
        {
            // Compute neighbor count using flat indices (no IDX() call overhead)
            unsigned char n =
                cur[base - BWIDTH + x - 1] + cur[base - BWIDTH + x] + cur[base - BWIDTH + x + 1] +
                cur[base            + x - 1]                          + cur[base            + x + 1] +
                cur[base + BWIDTH   + x - 1] + cur[base + BWIDTH   + x] + cur[base + BWIDTH   + x + 1];

            unsigned char alive = cur[base + x];
            unsigned char v = alive ? next_from_alive[n] : next_from_dead[n];

            nxt[base + x] = v;                                      // write next state
            screenBuf[srow + (x - 1)] = v ? LIVE_CHAR : DEAD_CHAR;  // build next frame text
        }
    }
}

void initialize_grid(void)
{
    //  Get a (semi)random seed for srand by using the current raster line
    unsigned char *raster = (unsigned char *)0xd012;
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

int main(void)
{
    initialize_grid();
    set_colours();
    clrscr();

    while (true) 
    {
        // Show the frame prepared during the previous iteration
        update_display();

        // Prepare borders for current cells
        update_borders();

        // Compute next gen AND build the NEXT frame's characters in screenBuf
        calc_next_gen();

        // Swap cell buffers (next becomes current)
        { unsigned char *tmp = current; current = next; next = tmp; }

        // Exit on key press
        if (kbhit()) 
        { 
            getch(); 
            break; 
        }
    }
    return 0;
}
