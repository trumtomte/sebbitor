#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE

#include "editor.h"
#include "io.h"
#include "term.h"

struct editorConfig E;

void initEditor(void) {
  E.cx = 0;
  E.cy = 0;
  E.rx = 0;
  E.px = 0;
  E.offset_row = 0;
  E.offset_col = 0;
  E.offset_line_number = 2;
  E.num_rows = 0;
  E.row = NULL;
  E.dirty = 0;
  E.filename = NULL;
  E.status_msg[0] = '\0';
  E.status_msg_time = 0;
  E.syntax = NULL;

  if (getWindowSize(&E.screen_rows, &E.screen_cols) == -1)
    die("getWindowSize");

  // Save one row for the status bar and one for the message
  E.screen_rows -= 2;
}

int main(int argc, char *argv[]) {
  enableRawMode();
  initEditor();

  if (argc >= 2) {
    editorOpen(argv[1]);
  }

  editorSetStatusMessage(
      " HELP: CTRL-S = Save | CTRL-Q = Quit | CTRL-F = Find");

  while (1) {
    editorRefreshScreen();
    editorProcessKeypress();
  }

  return 0;
}
