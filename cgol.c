
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <curses.h>

#define SAVE_FILE_NAME  "cgol_array.h"

// Bytes per column
#define COL_BYTES   (NUM_COLS / 8)

// Set for 5x5 pixel cells on eink screen
// Must be divisible by 8
#define NUM_ROWS    160
#define NUM_COLS    120

#define CELL_DEAD   0
#define CELL_ALIVE  1

#define GET_CELL_OFFSET(x) ((x) >> 3)
#define GET_CELL_BIT(x) ((x) & 0x7)
#define GET_CELL(x, array) ( \
    !!(((array)[GET_CELL_OFFSET((x))]) & (1 << GET_CELL_BIT((x)))) \
    )
#define SET_CELL(x, array, state) { \
    ((array)[GET_CELL_OFFSET((x))]) &= ~(1 << GET_CELL_BIT((x))); \
    ((array)[GET_CELL_OFFSET((x))]) |= ((state) << GET_CELL_BIT((x))); \
    }
#define SUM_ROW(x, array) ( \
    GET_CELL((x)-1, (array)) + \
    GET_CELL((x)  , (array)) + \
    GET_CELL((x)+1, (array)) \
    )

uint8_t d_cells[NUM_ROWS][COL_BYTES] = {{ CELL_DEAD }};
uint8_t t_cells[2][COL_BYTES] = {{ CELL_DEAD }};
uint8_t game_state[NUM_ROWS][COL_BYTES] = {{ CELL_DEAD }};

static void finish(int sig);

static inline void output_cell(int y, int x, int state) {
    mvaddch(y, x, (state == CELL_ALIVE) ? 'x' : ' ');
}

static inline void delay_period() {
    sleep(1);
}

void discard_line(FILE * fp) {
    int c;

    do {
        c = fgetc(fp);
        if (c == '\n') break;
    } while (c != EOF);
}

int parse_val(FILE * fp) {
    // Input is in the form 0x## so use 4 bytes
    char hex_string[] = "0x00";
    int c;
    //char * ptr;

    for (int i = 0; i < (sizeof(hex_string) - 1); ++i) {
        c = fgetc(fp);
        hex_string[i] = c;
    }

    return (int)strtol(hex_string, NULL, 16);
}

int write_file() {
    FILE * fp;

    fp = fopen(SAVE_FILE_NAME, "w");

    if (!fp) return -1;

    fprintf(fp, "/*******************\n");
    fprintf(fp, " * CGOL Game Array *\n");
    fprintf(fp, " *******************/\n\n");

    fprintf(fp, "uint8_t d_cells[NUM_ROWS][COL_BYTES] =\n");
    fprintf(fp, "{");
    for (int y = 0; y < NUM_ROWS; ++y) {
        for (int x = 0; x < COL_BYTES; ++x) {

            if (x == 0) fprintf(fp, "{");

            fprintf(fp, "0x%02hhx", (unsigned char)d_cells[y][x]);

            if (x == (COL_BYTES - 1)) {
                if (y == (NUM_ROWS - 1)) fprintf(fp, "}");
                else fprintf(fp, "},");
            } else {
                fprintf(fp, ",");
            }
        }
    }
    fprintf(fp, "};\n");

    return 0;
}

int read_file() {
    // Open file
    FILE * fp;
    int c;
    int cell_val;

    fp = fopen(SAVE_FILE_NAME, "r");
    if (!fp) {
        fprintf(stderr, "ERROR: couldn't open file\n");
        goto EXIT_ERROR;
    }

    // Discard preamble and array declaration
    for (int i = 0; i < 5; ++i) {
        discard_line(fp);
    }

    // Discard initial opening brace
    c = fgetc(fp);fprintf(stderr, "%c", c);
    c = fgetc(fp);fprintf(stderr, "%c", c);
    if (c != '{') {
        fprintf(stderr, "ERROR: outer opening brace missing\n");
        goto EXIT_ERROR;
    }

    for (int y = 0; y < NUM_ROWS; ++y) {

        // Each row starts with an opening brace
        c = fgetc(fp);
        if (c != '{') {
            fprintf(stderr, "ERROR: inner opening brace missing\n");
            goto EXIT_ERROR;
        }

        for (int x = 0; x < COL_BYTES; ++x) {
            // Get integer eq of hex string
            cell_val = parse_val(fp);

            // Check that next character is either a comma or close brace
            c = fgetc(fp);
            // Return an error for an unexpected character
            if (x != (COL_BYTES - 1)) {
                if (c != ',') {
                    fprintf(stderr, "ERROR: inner comma missing\n");
                    goto EXIT_ERROR;
                }
            } else {
                if (c != '}') {
                    fprintf(stderr, "ERROR: inner closing brace missing\n");
                    goto EXIT_ERROR;
                }
            }

            // Check that a valid value was returned
            if ((cell_val < 0) || (cell_val > 255)) goto EXIT_ERROR;

            d_cells[y][x] = (uint8_t)cell_val;

        }

        c = fgetc(fp);
        // Check to see the correct delimiting character was received
        if (y != (NUM_ROWS - 1)) {
            if (c != ',') {
                fprintf(stderr, "ERROR: outer comma missing\n");
                goto EXIT_ERROR;
            }
        } else {
            if (c != '}') {
                fprintf(stderr, "ERROR: outer closing brace missing\n");
                goto EXIT_ERROR;
            }
        }

    }

    fclose(fp);
    return 0;

EXIT_ERROR:
    if (fp) fclose(fp);
    return -1;
}

#define EN_CURSES

int main(int argc, char * argv[]) {

    int sum;
    int cell_state, next_cell_state;

    uint8_t * working_buffer;
    uint8_t * old_buffer;

    int x_pos = 1;
    int y_pos = 1;

    signal(SIGINT, finish);

#ifdef EN_CURSES
    // Init curses
    initscr();
    // Enable keyboard
    keypad(stdscr, TRUE);
    // Do not use CR/NL
    nonl();
    // Input characters one at a time, rather than one line at a time
    cbreak();

    noecho();

    curs_set(2);

    move(y_pos, x_pos);
#endif

    while (1) {

#ifdef EN_CURSES
        int c = getch();
#else
        int c = getchar();
        putchar(c);
#endif

        switch(c) {
            case KEY_LEFT:
                --x_pos;
                if (x_pos <= 0) x_pos = 1;
                break;
            case KEY_RIGHT:
                ++x_pos;
                if (x_pos >= NUM_COLS) x_pos = NUM_COLS - 1;
                break;
            case KEY_UP:
                --y_pos;
                if (y_pos <= 0) y_pos = 1;
                break;
            case KEY_DOWN:
                ++y_pos;
                if (y_pos >= NUM_ROWS) y_pos = NUM_ROWS - 1;
                break;
            case 'O':
            case 'o':
            case 'X':
            case 'x':

                if (GET_CELL(x_pos, d_cells[y_pos]) == CELL_ALIVE) {
#ifdef EN_CURSES
                    mvaddch(y_pos, x_pos, ' ');
#endif
                    SET_CELL(x_pos, d_cells[y_pos], CELL_DEAD);
                } else {
#ifdef EN_CURSES
                    mvaddch(y_pos, x_pos, 'x');
#endif
                    SET_CELL(x_pos, d_cells[y_pos], CELL_ALIVE);
                }
                //++x_pos;
                //if (x_pos >= NUM_COLS) x_pos = NUM_COLS - 1;
                break;

            case 'R':
            case 'r':
                goto start_game;
                break;
            case 'S':
            case 's':
                if (write_file() != 0) finish(0);
                break;
            case 'L':
            case 'l':
                if (read_file() != 0) finish(0);
                for (int y = 0; y < NUM_ROWS; ++y) {
                    for (int x = 0; x < NUM_COLS; ++x) {
#ifdef EN_CURSES
                        cell_state = GET_CELL(x, d_cells[y]);
                        output_cell(y, x, cell_state);
#endif
                    }
                }
            default:
                break;
        }
#ifdef EN_CURSES
        move(y_pos, x_pos);
        refresh();
#endif
    }

start_game:

#ifndef EN_CURSES
    finish(0);
#endif

    curs_set(0);

    while (1) {
        for (int y = 1; y < NUM_ROWS - 1; ++y) {
            // Swap buffers
            old_buffer     = t_cells[  y  & 1];
            working_buffer = t_cells[(~y) & 1];

            for (int x = 1; x < NUM_COLS - 1; ++x) {

                sum = SUM_ROW(x, d_cells[y-1]) +
                      SUM_ROW(x, d_cells[y  ]) +
                      SUM_ROW(x, d_cells[y+1]);

                cell_state = GET_CELL(x, d_cells[y]);

                if (sum == 3) {
                    // Cell lives
                    next_cell_state = CELL_ALIVE;
                } else if (sum == 4) {
                    // Cell stays the same
                    next_cell_state = cell_state;
                } else {
                    // Cell dies
                    next_cell_state = CELL_DEAD;
                }
                SET_CELL(x, working_buffer, next_cell_state);

                if (next_cell_state != cell_state) {
                    output_cell(y, x, next_cell_state);
                }
            }

            // Write old temp array to display buffer
            memcpy(d_cells[y-1], old_buffer, COL_BYTES);

        }

        refresh();

        delay_period();

    }

    finish(0);
}

static void finish(int sig) {

#ifdef EN_CURSES
    endwin();
#endif

    exit(0);
}