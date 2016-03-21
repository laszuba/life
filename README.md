# life
Simple implementation of Conway's Game of Life

Notes:

 * Uses curses to represent cells in the terminal.
 * Save files are compatible with E Ink design project implementation of Life.
 * Terminal must be big enough for all rows and columns.
 * Code quality is terrible... Oh well.

Compile with:
```bash
clang -std=c11 -lncurses -Wall cgol.c
```

Commands:
```text
s -- save game file, uses 'cgol_array.h'
l -- load stored game
r -- run current game
x -- set cell
```
