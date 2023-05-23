#ifndef EDITOR_H_
#define EDITOR_H_

#include <termios.h>
#include <time.h>

#define SEBBITOR_VERSION "0.1.0"
#define SEBBITOR_TAB_STOP 8
#define SEBBITOR_QUIT_TIMES 2

// editor row
typedef struct erow {
  int idx;
  int size;
  int rsize;
  char *chars;
  char *render;
  unsigned char *hl;
  int hl_open_comment;
} erow;

struct editorConfig {
  int cx, cy;
  int rx;
  // cursor x placement within the [p]rompt
  int px;
  int screen_rows, screen_cols;
  int offset_row, offset_col;
  int offset_line_number;
  int num_rows;
  erow *row;
  int dirty;
  char *filename;
  char status_msg[80];
  time_t status_msg_time;
  struct termios original_termios;
  struct editorSyntax *syntax;
};

extern struct editorConfig E;

#endif // EDITOR_H_
