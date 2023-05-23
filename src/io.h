#ifndef IO_H_
#define IO_H_

// 00011111 (i.e. 31, the control character)
#define CTRL_KEY(c) ((c)&0x1f)

typedef enum {
  BACKSPACE = 127,
  ARROW_LEFT = 1000,
  ARROW_DOWN,
  ARROW_UP,
  ARROW_RIGHT,
  DEL_KEY,
  HOME_KEY,
  END_KEY,
  PAGE_UP,
  PAGE_DOWN,
} editorKey;

void editorSetStatusMessage(const char *fmt, ...);
char *editorPrompt(char *prompt, void (*callback)(char *, int));
void editorOpen(char *filename);
void editorRefreshScreen(void);
void editorProcessKeypress(void);

#endif // IO_H_
