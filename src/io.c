#include "io.h"
#include "editor.h"
#include "ops.h"
#include "search.h"
#include "syntax.h"
#include "term.h"
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// FIXME: might wanna rename this to screen buffer?
// append buffer
typedef struct abuf {
  char *buf;
  size_t len;
} abuf;

void abAppend(abuf *ab, const char *s, size_t len) {
  char *new = realloc(ab->buf, ab->len + len);

  if (new == NULL)
    return;

  // Append the new data, and put it back into the original buffer
  memcpy(&new[ab->len], s, len);
  ab->buf = new;
  ab->len += len;

  // BAD
  // ab->buf = realloc(ab->buf, ab->len + len);
  // memcpy(&ab->buf[ab->len], s, len);
  // ab->len += len;
}

void abFree(abuf *ab) { free(ab->buf); }

/* File I/O */

void editorOpen(char *filename) {
  free(E.filename);
  E.filename = strdup(filename);

  editorSelectSyntaxHighlight();

  FILE *fp = fopen(filename, "r");

  if (!fp)
    die("fopen");

  char *line = NULL;
  size_t linecap = 0;
  ssize_t linelen;

  while ((linelen = getline(&line, &linecap, fp)) != -1) {
    while (linelen > 0 &&
           (line[linelen - 1] == '\n' || line[linelen - 1] == '\r')) {
      linelen--;
    }

    editorInsertRow(E.num_rows, line, linelen);
  }

  free(line);
  fclose(fp);
  E.dirty = 0;
}

char *editorRowsToString(int *buflen) {
  int total_len = 0;
  int j = 0;
  for (j = 0; j < E.num_rows; j++)
    total_len += E.row[j].size + 1;

  *buflen = total_len;

  char *buf = malloc(total_len);
  char *p = buf;

  for (j = 0; j < E.num_rows; j++) {
    memcpy(p, E.row[j].chars, E.row[j].size);
    p += E.row[j].size;
    *p = '\n';
    p++;
  }

  return buf;
}

void editorSave(void) {
  if (E.filename == NULL) {
    E.filename = editorPrompt(" Save as: %s  (Press ESC to cancel)", NULL);
    if (E.filename == NULL) {
      editorSetStatusMessage(" Save aborted");
      return;
    }

    editorSelectSyntaxHighlight();
  }

  int len;
  char *buf = editorRowsToString(&len);

  int fd = open(E.filename, O_RDWR | O_CREAT, 0644);

  if (fd == -1) {
    free(buf);
    editorSetStatusMessage(" Can't save! I/O error: %s", strerror(errno));
    return;
  }

  if (ftruncate(fd, len) == -1) {
    close(fd);
    free(buf);
    return;
  }

  if (write(fd, buf, len) != len) {
    close(fd);
    free(buf);
    return;
  }

  close(fd);
  free(buf);
  editorSetStatusMessage(" %d bytes written to disk", len);
  E.dirty = 0;
}

/* Output */

void printWelcomeMessage(abuf *ab) {
  char msg[80];

  int len =
      snprintf(msg, sizeof(msg), "The Sebbitor Editor (%s)", SEBBITOR_VERSION);

  // Truncate
  if (len > E.screen_cols)
    len = E.screen_cols;

  int padding = (E.screen_cols - len) / 2;

  if (padding) {
    abAppend(ab, "~", 1);
    padding--;
  }

  while (padding--)
    abAppend(ab, " ", 1);

  abAppend(ab, msg, len);
}

void editorSetStatusMessage(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  vsnprintf(E.status_msg, sizeof(E.status_msg), fmt, args);
  va_end(args);
  E.status_msg_time = time(NULL);
}

void editorScroll(void) {
  E.rx = 0;

  if (E.cy < E.num_rows) {
    E.rx = editorRowCxToRx(&E.row[E.cy], E.cx);
  }

  if (E.cy < E.offset_row) {
    E.offset_row = E.cy;
  }

  if (E.cy >= E.offset_row + E.screen_rows) {
    E.offset_row = E.cy - E.screen_rows + 1;
  }

  if (E.rx < E.offset_col) {
    E.offset_col = E.rx;
  }

  // NOTE: do not scroll the line numbers (left/right)
  if (E.rx >= E.offset_col + (E.screen_cols - E.offset_line_number)) {
    E.offset_col = E.rx - (E.screen_cols - E.offset_line_number) + 1;
  }
}

void editorDrawLineNumber(abuf *ab, int filerow) {
  char *line_number = malloc(E.offset_line_number);

  int len = snprintf(line_number, E.offset_line_number + 1, "%d ", filerow + 1);
  int pad = E.offset_line_number - len + 1;

  if (E.cy == filerow) {
    abAppend(ab, "\x1b[38;5;179m", 11);
    abAppend(ab, "\x1b[4m", 4);
  } else {
    abAppend(ab, "\x1b[38;5;243m", 11);
  }

  while (pad--)
    abAppend(ab, " ", 1);

  abAppend(ab, line_number, len);
  abAppend(ab, "\x1b[24m", 5);
  abAppend(ab, "\x1b[39m", 5);
}

void editorDrawRow(abuf *ab, int filerow) {
  int len = E.row[filerow].rsize - E.offset_col;

  if (len < 0)
    len = 0;

  // truncate with line numbers
  if (len > E.screen_cols - E.offset_line_number)
    len = E.screen_cols - E.offset_line_number;
  // if (len > E.screen_cols)
  //   len = E.screen_cols;
  //
  // abAppend(ab, &E.row[filerow].render[E.offset_col], len);

  char *c = &E.row[filerow].render[E.offset_col];
  unsigned char *hl = &E.row[filerow].hl[E.offset_col];
  int current_color = -1;
  int j;

  for (j = 0; j < len; j++) {
    if (iscntrl(c[j])) {
      char sym = (c[j] <= 26) ? '@' + c[j] : '?';
      abAppend(ab, "\x1b[7m", 4);
      abAppend(ab, &sym, 1);
      abAppend(ab, "\x1b[m", 3);

      if (current_color != -1) {
        char buf[16];
        int clen = snprintf(buf, sizeof(buf), "\x1b[38;5;%dm", current_color);
        abAppend(ab, buf, clen);
      }
    } else if (hl[j] == HL_NORMAL) {
      // Reset color when the previous was not HL_NORMAL
      if (current_color != -1) {
        abAppend(ab, "\x1b[39m", 5);
        current_color = -1;
      }

      abAppend(ab, &c[j], 1);
    } else {
      int color = editorSyntaxToColor(hl[j]);
      if (color != current_color) {
        current_color = color;
        char buf[16];
        int color_len = snprintf(buf, sizeof(buf), "\x1b[38;5;%dm", color);
        abAppend(ab, buf, color_len);
      }

      abAppend(ab, &c[j], 1);
    }
  }

  abAppend(ab, "\x1b[39m", 5);
}

void updateLineNumberOffset(int lines) {
  E.offset_line_number = 2;
  while ((lines /= 10) > 0)
    E.offset_line_number++;
}

void editorDrawRows(abuf *ab) {
  updateLineNumberOffset(E.num_rows);

  // Draw tildes as the left column
  int y;
  for (y = 0; y < E.screen_rows; y++) {
    int filerow = y + E.offset_row;

    if (filerow >= E.num_rows) {
      if (E.num_rows == 0 && y == E.screen_rows / 3) {
        // Print a welcome message 1/3 down the screen
        printWelcomeMessage(ab);
      } else {
        abAppend(ab, "~", 1);
      }
    } else {
      editorDrawLineNumber(ab, filerow);
      editorDrawRow(ab, filerow);
    }

    // Clear everything to the right of the cursor
    abAppend(ab, "\x1b[K", 3);
    abAppend(ab, "\r\n", 2);
  }
}

void editorDrawStausBar(abuf *ab) {
  // abAppend(ab, "\x1b[7m", 4);

  char status[80], rstatus[80];

  int len = snprintf(status, sizeof(status), " %.20s  %d:%d %s",
                     E.filename ? E.filename : "[Untitled]", E.cy + 1, E.cx,
                     E.dirty ? "(modified)" : "");
  if (len > E.screen_cols)
    len = E.screen_cols;

  abAppend(ab, status, len);

  int pos = E.num_rows > 0 ? (E.cy * 100) / E.num_rows : 0;
  int rlen = snprintf(rstatus, sizeof(rstatus), "%d%% (%d) [%s] ", pos,
                      E.num_rows, E.syntax ? E.syntax->filetype : "unkwn ft");

  while (len < E.screen_cols) {
    // The length of the right message aligns with the end of the columns
    if (E.screen_cols - len == rlen) {
      abAppend(ab, rstatus, rlen);
      break;
    }

    abAppend(ab, " ", 1);
    len++;
  }

  /* abAppend(ab, "\x1b[m", 3); */
  abAppend(ab, "\r\n", 2);
}

void editorDrawMessageBar(abuf *ab) {
  abAppend(ab, "\x1b[K", 3);
  abAppend(ab, "\x1b[38;5;179m", 11);

  int len = strlen(E.status_msg);
  if (len > E.screen_cols)
    len = E.screen_cols;

  if (len && time(NULL) - E.status_msg_time < 5)
    abAppend(ab, E.status_msg, len);

  abAppend(ab, "\x1b[39", 4);
}

void editorDrawCursor(abuf *ab) {
  char buf[32];
  int y;
  int x;

  // Place the cursor within the prompt
  if (E.px > 0) {
    y = E.screen_rows + 2;
    x = E.px;
  } else {
    // +1 since VT100 positions are set by 1..n
    y = (E.cy - E.offset_row) + 1;
    x = E.offset_line_number + (E.rx - E.offset_col) + 2;
  }

  snprintf(buf, sizeof(buf), "\x1b[%d;%dH", y, x);
  abAppend(ab, buf, strlen(buf));
}

void editorRefreshScreen(void) {
  editorScroll();

  abuf ab = {.buf = NULL, .len = 0};

  abAppend(&ab, "\x1b[?25l", 6);
  abAppend(&ab, "\x1b[H", 3);
  // Gray background
  abAppend(&ab, "\x1b[48;5;235m", 11);

  editorDrawRows(&ab);
  editorDrawStausBar(&ab);
  editorDrawMessageBar(&ab);
  editorDrawCursor(&ab);

  abAppend(&ab, "\x1b[?25h", 6);

  // Flush our buffer to stdout
  write(STDOUT_FILENO, ab.buf, ab.len);
  abFree(&ab);
}

/* Input */

char *editorPrompt(char *prompt, void (*callback)(char *, int)) {
  size_t bufsize = 128;
  char *buf = malloc(bufsize);

  size_t buflen = 0;
  buf[0] = '\0';

  while (1) {
    // NOTE: this works with %s ...
    E.px = strstr(prompt, "%s") - prompt + buflen + 1;
    // E.px = strlen(prompt) + buflen - 1;

    editorSetStatusMessage(prompt, buf);
    editorRefreshScreen();

    int c = editorReadKey();
    if (c == DEL_KEY || c == CTRL_KEY('h') || c == BACKSPACE) {
      if (buflen != 0)
        buf[--buflen] = '\0';
    } else if (c == '\x1b') {
      editorSetStatusMessage("");

      if (callback)
        callback(buf, c);

      free(buf);
      E.px = 0;
      return NULL;
    } else if (c == '\r') {
      if (buflen != 0) {
        editorSetStatusMessage("");

        if (callback)
          callback(buf, c);

        E.px = 0;
        return buf;
      }
    } else if (!iscntrl(c) && c < 128) {
      if (buflen == bufsize - 1) {
        bufsize *= 2;
        buf = realloc(buf, bufsize);
      }

      buf[buflen++] = c;
      buf[buflen] = '\0';
    }

    if (callback)
      callback(buf, c);
  }
}

void editorMoveCursor(int key) {
  erow *row = (E.cy >= E.num_rows) ? NULL : &E.row[E.cy];

  switch (key) {
  case ARROW_LEFT:
    if (E.cx != 0) {
      E.cx--;
    } else if (E.cy > 0) {
      E.cy--;
      E.cx = E.row[E.cy].size;
    }
    break;
  case ARROW_RIGHT:
    if (row && E.cx < row->size) {
      E.cx++;
    } else if (row && E.cx == row->size) {
      E.cy++;
      E.cx = 0;
    }
    break;
  case ARROW_UP:
    if (E.cy != 0) {
      E.cy--;
    }
    break;
  case ARROW_DOWN:
    if (E.cy < E.num_rows) {
      E.cy++;
    }
    break;
  }

  // If we were at the end of a long line, move down to a shorter one we want to
  // be at the end of the shorter one.
  row = (E.cy >= E.num_rows) ? NULL : &E.row[E.cy];
  int rowlen = row ? row->size : 0;
  if (E.cx > rowlen) {
    E.cx = rowlen;
  }
}

void editorProcessKeypress(void) {
  static int quit_times = SEBBITOR_QUIT_TIMES;

  int c = editorReadKey();

  switch (c) {
  case '\r':
    editorInsertNewline();
    break;

  case CTRL_KEY('q'):
    if (E.dirty && quit_times > 0) {
      editorSetStatusMessage(
          "No write since last change. Press CTRL-Q %d more times to Quit",
          quit_times);
      quit_times--;
      return;
    }
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);
    exit(0);
    break;

  case CTRL_KEY('s'):
    editorSave();
    break;

  case HOME_KEY:
    E.cx = 0;
    break;

  case END_KEY:
    if (E.cy < E.num_rows)
      E.cx = E.row[E.cy].size;
    break;

  case CTRL_KEY('f'):
    editorFind();
    break;

  case BACKSPACE:
  case CTRL_KEY('h'):
  case DEL_KEY:
    if (c == DEL_KEY) {
      editorMoveCursor(ARROW_RIGHT);
    }

    editorDeleteChar();
    break;

  case CTRL_KEY('u'):
  case PAGE_UP: {
    E.cy = E.offset_row;
    int times = E.screen_rows;
    while (times--)
      editorMoveCursor(ARROW_UP);

  } break;
  case CTRL_KEY('d'):
  case PAGE_DOWN: {
    // We simulate a full page jump by positioning the cursor at the top or
    // bottom of the screen first
    E.cy = E.offset_row + E.screen_rows - 1;
    if (E.cy > E.num_rows)
      E.cy = E.num_rows;

    int times = E.screen_rows;
    while (times--)
      editorMoveCursor(ARROW_DOWN);

  } break;
  case ARROW_UP:
  case ARROW_DOWN:
  case ARROW_LEFT:
  case ARROW_RIGHT:
    editorMoveCursor(c);
    break;

  case CTRL_KEY('p'):
    editorMoveCursor(ARROW_UP);
    break;
  case CTRL_KEY('n'):
    editorMoveCursor(ARROW_DOWN);
    break;

  case CTRL_KEY('l'):
  case '\x1b':
    break;

  default:
    editorInsertChar(c);
    break;
  }

  quit_times = SEBBITOR_QUIT_TIMES;
}
