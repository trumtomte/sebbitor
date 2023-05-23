#ifndef TERM_H_
#define TERM_H_

int editorReadKey(void);
void enableRawMode(void);
int getWindowSize(int *rows, int *cols);
void die(const char *reason);

#endif // TERM_H_
