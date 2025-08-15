// Game of Life implementation for Commodore 64 using Oscar64 C compiler.
// This program simulates Conway's Game of Life on a toroidal grid (wraps around edges).
// The grid is 40x25 to match the C64 screen size.
// Cells are represented as 1 (alive) or 0 (dead).
// The simulation runs in an infinite loop until a key is pressed.

#include <stdlib.h>  // For rand() and srand()
#include <conio.h>   // For clrscr(), kbhit(), getch()
#include <string.h>  // For memset() and memcpy()
#include <stdbool.h> // true & false

// Define grid dimensions: actual screen is 40x25, but we add borders for wrapping
#define WIDTH 40    // Screen width
#define HEIGHT 25   // Screen height
#define BWIDTH (WIDTH + 2)   // Bordered width (extra columns on left and right)
#define BHEIGHT (HEIGHT + 2) // Bordered height (extra rows on top and bottom)

// Changed to 1D arrays + an index macro --------------------------------
#define IDX(y,x) ((y) * BWIDTH + (x))    // map (y,x) to linear index
unsigned char current[BHEIGHT * BWIDTH];
unsigned char next[BHEIGHT * BWIDTH];
// -------------------------------------------------------------------------------

// Pointer to the C64 screen memory starting at $0400.
// Each byte represents a character on the screen.
unsigned char *screen = (unsigned char *)0x0400;

// Function to update the border cells to simulate toroidal wrapping.
// This copies the opposite edges to the borders, handling horizontal and vertical wraps.
void update_borders() 
{
    int y, x;

    // Handle horizontal wrapping: left border copies from right edge, right from left.
    for (y = 1; y <= HEIGHT; y++) 
    {
        current[IDX(y,0)]          = current[IDX(y,WIDTH)];      // Left border = right edge
        current[IDX(y,BWIDTH - 1)] = current[IDX(y,1)];          // Right border = left edge
    }

    // Handle vertical wrapping: top border copies from bottom edge, bottom from top.
    for (x = 0; x < BWIDTH; x++) 
    {
        current[IDX(0,x)]           = current[IDX(HEIGHT,x)];    // Top border = bottom row
        current[IDX(BHEIGHT - 1,x)] = current[IDX(1,x)];         // Bottom border = top row
    }
}

// Function to compute the next generation based on Game of Life rules.
// For each cell, count live neighbors and decide if it lives, dies, or is born.
void calc_next_gen() 
{
    int y, x;
    int count;

    // Loop over the inner grid (don't include the borders!)
    for (y = 1; y <= HEIGHT; y++) 
    {
        for (x = 1; x <= WIDTH; x++) 
        {
            // Count the 8 neighbors (Moore neighborhood)
            count  = current[IDX(y - 1, x - 1)];
            count += current[IDX(y - 1, x)];
            count += current[IDX(y - 1, x + 1)];
            count += current[IDX(y,     x - 1)];
            count += current[IDX(y,     x + 1)];
            count += current[IDX(y + 1, x - 1)];
            count += current[IDX(y + 1, x)];
            count += current[IDX(y + 1, x + 1)];
            
            // Apply Conway's rules
            // - If alive (current[y][x] == 1): survives if 2 or 3 neighbors
            // - If dead: born if exactly 3 neighbors
            if (current[IDX(y,x)]) 
            {
                next[IDX(y,x)] = (count == 2 || count == 3) ? 1 : 0;
            } 
            else 
            {
                next[IDX(y,x)] = (count == 3) ? 1 : 0;
            }
        }
    }
}

void initialize_grid()
{
    // Use the current raster position as a (semi)random seed for srand()
    unsigned char *raster = (unsigned char *)0xd012;
    srand(*raster);

    // Initialize the current grid to all dead (0)
    memset(current, 0, sizeof(current));

    // Initialize random start grid
    int y, x;
    for (y = 1; y <= HEIGHT; y++) 
    {
        for (x = 1; x <= WIDTH; x++) 
        {
            // Set cell to 1 (alive) with 50% chance (rand() & 1)
            current[IDX(y,x)] = rand() & 1;
        }
    }
}

// Function to display the current grid on the screen.
void update_display() 
{
    int y, x;
    // Loop over the inner grid
    for (y = 1; y <= HEIGHT; y++) 
    {
        for (x = 1; x <= WIDTH; x++) 
        {
            // Calculate screen position (row-major order)
            int pos = (y - 1) * WIDTH + (x - 1);

            // Set the cell & colour
            screen[pos] = current[IDX(y,x)] ? '*' : ' ';
        }
    }
}

void set_colours()
{
    // Set background and border to black (color 0)
    bgcolor(COLOR_BLACK);
    bordercolor(COLOR_BLACK);
    textcolor(COLOR_GREEN);
}

// Main function: entry point of the program.
int main() 
{
    //  Set the start grid
    initialize_grid();

    // Clear the screen
    set_colours();
    clrscr();

    //  Run main loop until user presses a key
    while (true) 
    {
        update_display(); // Update screen with current state
        update_borders(); // Handle wrapping
        calc_next_gen();  // Calculate next generation
        
        // Copy next to current for the next iteration
        memcpy(current, next, sizeof(next));

        // Key press?
        if (kbhit()) 
        {
            getch();  // Consume the key
            break;    // We're done!
        }
    }

    // All done
    return 0;  
}
