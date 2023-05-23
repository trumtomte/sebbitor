#include "editor.h"
#include "syntax.h"
#include <stdlib.h>
#include <string.h>

/**
 * Row operations
 * =================
 **/

int editorRowCxToRx(erow *row, int cx) {
  int rx = 0;
  int j;
  for (j = 0; j < cx; j++) {
    // make the cursor 'jump' the width of a tab
    if (row->chars[j] == '\t')
      rx += (SEBBITOR_TAB_STOP - 1) - (rx % SEBBITOR_TAB_STOP);
    rx++;
  }

  return rx;
}

int editorRowRxToCx(erow *row, int rx) {
  int curr_rx = 0;
  int cx;
  for (cx = 0; cx < row->size; cx++) {
    // make the cursor 'jump' the width of a tab
    if (row->chars[cx] == '\t')
      curr_rx += (SEBBITOR_TAB_STOP - 1) - (curr_rx % SEBBITOR_TAB_STOP);
    curr_rx++;

    if (curr_rx > rx)
      return cx;
  }

  return cx;
}

void editorUpdateRow(erow *row) {
  int j;
  int tabs = 0;
  for (j = 0; j < row->size; j++)
    if (row->chars[j] == '\t')
      tabs++;

  free(row->render);
  row->render = malloc(row->size + (tabs * (SEBBITOR_TAB_STOP - 1)) + 1);

  int col = 0;
  for (j = 0; j < row->size; j++) {
    if (row->chars[j] == '\t') {
      row->render[col++] = ' ';
      // Append spaces until the next tab stop (ie. col divisible by 8)
      while (col % SEBBITOR_TAB_STOP != 0)
        row->render[col++] = ' ';
    } else {
      row->render[col++] = row->chars[j];
    }
  }
  row->render[col] = '\0';
  row->rsize = col;

  editorUpdateSyntax(row);
}

void editorInsertRow(int at, char *s, size_t len) {
  if (at < 0 || at > E.num_rows)
    return;

  E.row = realloc(E.row, sizeof(erow) * (E.num_rows + 1));
  memmove(&E.row[at + 1], &E.row[at], sizeof(erow) * (E.num_rows - at));

  E.row[at].idx = at;
  E.row[at].size = len;
  E.row[at].chars = malloc(len + 1);
  memcpy(E.row[at].chars, s, len);
  E.row[at].chars[len] = '\0';

  E.row[at].rsize = 0;
  E.row[at].render = NULL;
  E.row[at].hl = NULL;
  E.row[at].hl_open_comment = 0;
  editorUpdateRow(&E.row[at]);

  E.num_rows++;
  E.dirty++;
}

void editorFreeRow(erow *row) {
  free(row->chars);
  free(row->render);
  free(row->hl);
}

void editorRowInsertChar(erow *row, int at, int c) {
  if (at < 0 || at > row->size)
    at = row->size;

  row->chars = realloc(row->chars, row->size + 2);
  memmove(&row->chars[at + 1], &row->chars[at], row->size - at + 1);

  row->size++;
  row->chars[at] = c;

  editorUpdateRow(row);
  E.dirty++;
}

void editorRowAppendString(erow *row, char *s, size_t len) {
  row->chars = realloc(row->chars, row->size + len + 1);
  memcpy(&row->chars[row->size], s, len);
  row->size += len;
  row->chars[row->size] = '\0';
  editorUpdateRow(row);
  E.dirty++;
}

void editorRowDeleteChar(erow *row, int at) {
  if (at < 0 || at > row->size)
    at = row->size;

  memmove(&row->chars[at], &row->chars[at + 1], row->size - at);
  row->size--;
  editorUpdateRow(row);
  E.dirty++;
}

void editorDeleteRow(int at) {
  if (at < 0 || at >= E.num_rows)
    return;

  editorFreeRow(&E.row[at]);
  memmove(&E.row[at], &E.row[at + 1], sizeof(erow) * (E.num_rows - at - 1));

  for (int j = at; j < E.num_rows - 1; j++)
    E.row[j].idx--;

  E.num_rows--;
  E.dirty++;
}

/**
 * Editor operations
 * =================
 **/

void editorInsertChar(int c) {
  if (E.cy == E.num_rows)
    editorInsertRow(E.num_rows, "", 0);

  editorRowInsertChar(&E.row[E.cy], E.cx, c);
  E.cx++;
}

void editorDeleteChar(void) {
  if (E.cy == E.num_rows)
    return;
  if (E.cx == 0 && E.cy == 0)
    return;

  erow *row = &E.row[E.cy];
  if (E.cx > 0) {
    editorRowDeleteChar(row, E.cx - 1);
    E.cx--;
  } else {
    E.cx = E.row[E.cy - 1].size;
    editorRowAppendString(&E.row[E.cy - 1], row->chars, row->size);
    editorDeleteRow(E.cy);
    E.cy--;
  }
}

void editorInsertNewline(void) {
  if (E.cx == 0) {
    editorInsertRow(E.cy, "", 0);
  } else {
    erow *row = &E.row[E.cy];
    editorInsertRow(E.cy + 1, &row->chars[E.cx], row->size - E.cx);
    row = &E.row[E.cy];
    row->size = E.cx;
    row->chars[row->size] = '\0';
    editorUpdateRow(row);
  }
  E.cy++;
  E.cx = 0;
}
