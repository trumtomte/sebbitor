#ifndef OPS_H_
#define OPS_H_

#include "editor.h"

int editorRowRxToCx(erow *row, int rx);
int editorRowCxToRx(erow *row, int cx);
void editorInsertRow(int at, char *s, size_t len);
void editorInsertChar(int c);
void editorDeleteChar(void);
void editorInsertNewline(void);

#endif // OPS_H_
