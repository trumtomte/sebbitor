#include "editor.h"
#include "io.h"
#include "ops.h"
#include "syntax.h"
#include <stdlib.h>
#include <string.h>

void editorFindCallback(char *query, int key) {
  static int last_match = -1;
  static int direction = 1;

  static int prev_hl_line;
  static char *prev_hl = NULL;

  if (prev_hl) {
    memcpy(E.row[prev_hl_line].hl, prev_hl, E.row[prev_hl_line].rsize);
    free(prev_hl);
    prev_hl = NULL;
  }

  if (key == '\r' || key == '\x1b') {
    last_match = -1;
    direction = 1;
    return;
  }

  if (key == ARROW_RIGHT || key == ARROW_DOWN || key == CTRL_KEY('n')) {
    direction = 1;
  } else if (key == ARROW_LEFT || key == ARROW_UP || key == CTRL_KEY('p')) {
    direction = -1;
  } else {
    last_match = -1;
    direction = 1;
  }

  if (last_match == -1)
    direction = 1;

  int current = last_match;
  int i;
  for (i = 0; i < E.num_rows; i++) {
    current += direction;

    if (current == -1) {
      current = E.num_rows - 1;
    } else if (current == E.num_rows) {
      current = 0;
    }

    erow *row = &E.row[current];
    char *match = strstr(row->render, query);

    if (match) {
      last_match = current;
      E.cy = current;
      E.cx = editorRowRxToCx(row, match - row->render);
      E.offset_row = E.num_rows;

      prev_hl_line = current;
      prev_hl = malloc(row->rsize);
      memcpy(prev_hl, row->hl, row->rsize);
      memset(&row->hl[match - row->render], HL_MATCH, strlen(query));
      break;
    }
  }
}

void editorFind(void) {
  int saved_cy = E.cy;
  int saved_cx = E.cx;
  int saved_offset_row = E.offset_row;
  int saved_offset_col = E.offset_col;

  char *query =
      editorPrompt(" Search: %s  (Press ESC to cancel)", editorFindCallback);

  if (query) {
    free(query);
  } else {
    E.cy = saved_cy;
    E.cx = saved_cx;
    E.offset_row = saved_offset_row;
    E.offset_col = saved_offset_col;
  }
}
