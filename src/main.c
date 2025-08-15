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

#define IDX(y,x) ((y) * BWIDTH + (x))    // map (y,x) to linear index

// Cell buffers (swapped via pointers)
static unsigned char buf0[BHEIGHT * BWIDTH];
static unsigned char buf1[BHEIGHT * BWIDTH];
static unsigned char *current = buf0;    // current generation buffer
static unsigned char *next    = buf1;    // next generation buffer

// Single off-screen display buffer (chars for the frame to be shown next)
#define LIVE_CHAR '*'
#define DEAD_CHAR ' '
static unsigned char screenBuf[WIDTH * HEIGHT];

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
    int y, x, count;
    for (y = 1; y <= HEIGHT; y++) 
    {
        int srow = (y - 1) * WIDTH;  // row offset in screenBuf
        for (x = 1; x <= WIDTH; x++) 
        {
            count  = current[IDX(y - 1, x - 1)];
            count += current[IDX(y - 1, x)];
            count += current[IDX(y - 1, x + 1)];
            count += current[IDX(y,     x - 1)];
            count += current[IDX(y,     x + 1)];
            count += current[IDX(y + 1, x - 1)];
            count += current[IDX(y + 1, x)];
            count += current[IDX(y + 1, x + 1)];

            unsigned char v = current[IDX(y,x)]
                              ? (unsigned char)((count == 2) || (count == 3))
                              : (unsigned char)(count == 3);

            next[IDX(y,x)] = v;
            screenBuf[srow + (x - 1)] = v ? LIVE_CHAR : DEAD_CHAR;  // prepare next frame text
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
    textcolor(COLOR_GREEN);
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
            getch(); break; 
        }
    }
    return 0;
}
