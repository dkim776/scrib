/*** includes ***/

#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
//#include <time.h>
#include <sys/time.h>

/*** defines ***/

#define SCRIB_VERSION "0.1.0"
#define TAB_WIDTH 2
#define CTRL_KEY(k) ((k) & 0x1f)

enum editorKey {
  BACKSPACE = 127,
  ARROW_LEFT = 1000,
  ARROW_RIGHT,
  ARROW_UP,
  ARROW_DOWN,
  DEL_KEY,
  HOME_KEY,
  END_KEY,
  PAGE_UP,
  PAGE_DOWN
};

/*** data ***/
struct editorConfig {
  int cx, cy;
  int rowoff;
  int coloff;
  int screenrows;
  int screencols;
  int numrows;
  int dirty;
  int *rowNums;
  char *filename;
  char statusmsg[80];
  time_t statusmsg_time;
  struct termios orig_termios;
};

struct editorConfig E;

char *original;
char *added;
int bufferIndex;
int bufferSize;

typedef struct list {
  int linecount;
  int *lines;
  int offset;
  int start;
  int end;
  char *buffer;
  int bufferType;
  struct list *next;
  struct list *prev;
} node;

node *head = NULL;
node *tail = NULL;

// node *getNewNode (char *text, int start, int end, int bufferType);
// void insertChar (char *c, int line, int pos, int bufferType);
// void deleteNode (int line, int pos);
// void debugList (int opt);
// void printList ();
// void printlines(int topLine, int botLine, struct abuf *ab);
void editorSetStatusMessage(const char *fmt, ...);
void editorRefreshScreen();
char *editorPrompt(char *prompt);

void debugNode(node* curr) {
  printf ("[(%d) %d,%d,%d]\r\n", curr->bufferType, curr->start, curr->end, curr->linecount);;
}

node *getNewNode (char *text, int start, int end, int bufferType){
  int* a = (int *)calloc(1, sizeof(int));
  int count = 0;
  int offset = 0;
  for (int i = start; i <= end; i++){
    offset++;
    if (text[i] == '\n') {
        a[count++] = offset;
        offset = 0;
        a = (int *)realloc(a, (count + 1) * sizeof(int));
    }
  }

  node *tmp = (node *) malloc (sizeof (node));
  tmp->linecount = count;
  tmp->lines = a;
  tmp->start = start;
  tmp->end = end;
  tmp->buffer = text;
  tmp->bufferType = bufferType;
  //free(a);
  return tmp;
}

// print the list in forward order
void debugList (int opt){
  node *curr = head;
  while (curr != NULL && opt == 0) {
    debugNode(curr);
    curr = curr->next;
  }

  while (curr != NULL && opt == 1) {
    printf ("[ ");
    for (int i = 0; i < curr->linecount; i++) {
  	  printf ("%d ", *(curr->lines + i));
  	}
    printf ("]");
    curr = curr->next;
  }

  printf ("[done]\n");
}

void printList () {
  node *curr = head;
  while (curr != NULL) {
    if (curr->bufferType == 0) {
      printf("%.*s", curr->end - curr->start, curr->start + original);
    } else {
      printf("%.*s", curr->end - curr->start, curr->start + added);
    }
    curr = curr->next;
  }
  printf ("\n[done]");
}

void splitNode(node *curr, int splitIndex) {
  node *front = curr->prev;
  node *back = curr->next;
  node *leftSplit = getNewNode(curr->buffer, curr->start, curr->start + splitIndex - 1, curr->bufferType);
  node *newNode = getNewNode(added, bufferIndex, bufferIndex, 1);
  node *rightSplit = getNewNode(curr->buffer, curr->start + splitIndex, curr->end, curr->bufferType);

  leftSplit->next = newNode;
  leftSplit->prev = front;
  newNode->next = rightSplit;
  newNode->prev = leftSplit;
  rightSplit->next = back;
  rightSplit->prev = newNode;

  front->next = leftSplit;
  back->prev = rightSplit;
}

void insertChar (int line, int pos){
  node *curr = head->next;
  int currLine = 1;
  while (curr != tail && currLine < line) {
    if (currLine + curr->linecount >= line) break;
    currLine += curr->linecount;
    curr = curr->next;
  }

  int offset = 0;
  for (int i = 0; i < line - currLine; i++) {
    offset += curr->lines[i];
  }

  if (currLine + curr->linecount == line) {
    int posLeft = pos - (curr->end - curr->start + 1 - offset);
    if (posLeft > 0) curr = curr->next;
    int splitIndex = posLeft + curr->end - curr->start + 1;

    while (curr != tail && 0 < posLeft) {
      posLeft -= (curr->linecount == 0) ? curr->end - curr->start + 1 : curr->lines[0];
      if (posLeft > 0) curr = curr->next;
      splitIndex = (curr->linecount == 0) ? posLeft + curr->end - curr->start + 1 : curr->lines[0] + posLeft;
    }
    if (posLeft == 0 && curr->bufferType == 1) {
      if (curr->buffer[bufferIndex] == '\n') {
        //printf("err!\n");
        int newLineSum = 0;
        for (int i = 0; i < curr->linecount; i++) newLineSum += curr->lines[i];
        curr->linecount++;
        curr->end++;
        curr->lines = (int *) realloc(curr->lines, (curr->linecount + 1) * sizeof(int));
        curr->lines[curr->linecount - 1] = curr->end - curr->start + 1 - newLineSum;
      } else {
        curr->end++;
      }
    } else if (posLeft == 0) {
      node *newNode = getNewNode(added, bufferIndex, bufferIndex, 1);
      newNode->next = curr->next;
      newNode->prev = curr;
      curr->next->prev = newNode;
      curr->next = newNode;
    } else {
      splitNode(curr, splitIndex);
    }
  } else if (offset == 0 && pos == 0) {
    node *newNode = getNewNode(added, bufferIndex, bufferIndex, 1);
    newNode->next = curr;
    newNode->prev = curr->prev;
    curr->prev->next = newNode;
    curr->prev = newNode;
  } else {
    offset += pos;
    splitNode(curr, offset);
  }
  bufferIndex++;
}

void splitNodeDelete(node *curr, int splitIndex) {
  //printf ("[(%d),%d,%d,%d]\n", curr->bufferType, curr->start, curr->end, curr->linecount);
  node *front = curr->prev;
  node *back = curr->next;
  node *leftSplit = getNewNode(curr->buffer, curr->start, curr->start + splitIndex - 2, curr->bufferType);
  //node *newNode = getNewNode(added, bufferIndex, bufferIndex, 1);
  node *rightSplit = getNewNode(curr->buffer, curr->start + splitIndex, curr->end, curr->bufferType);
  if (curr->buffer[curr->start + splitIndex - 1] == '\n') {
    if (E.cy > 0) E.cy--;
    E.numrows--;
    E.cx = E.rowNums[E.cy - E.rowoff] > 1 ? E.rowNums[E.cy - E.rowoff] : 0;
  }

  leftSplit->next = rightSplit;
  leftSplit->prev = front;
  rightSplit->next = back;
  rightSplit->prev = leftSplit;

  front->next = leftSplit;
  back->prev = rightSplit;
}

void deleteChar (int line, int pos){
  if (line == 1 && pos == 0) return;
  node *curr = head->next;
  int currLine = 1;
  while (curr != tail && currLine < line) {
    if (currLine + curr->linecount >= line) break;
    currLine += curr->linecount;
    curr = curr->next;
  }

  int offset = 0;
  for (int i = 0; i < line - currLine; i++) {
    offset += curr->lines[i];
  }

  if (currLine + curr->linecount == line) { // edge case!
    int posLeft = pos - (curr->end - curr->start + 1 - offset);
    if (posLeft > 0) curr = curr->next;
    int splitIndex = posLeft + curr->end - curr->start + 1;

    while (curr != tail && 0 < posLeft) {
      posLeft -= (curr->linecount == 0) ? curr->end - curr->start + 1 : curr->lines[0];
      if (posLeft > 0) curr = curr->next;
      splitIndex = (curr->linecount == 0) ? posLeft + curr->end - curr->start + 1 : curr->lines[0] + posLeft;
    }

    if (posLeft == 0) { // we've reached the end of the node
      if (curr->end - curr->start == 0) {
        if (curr->buffer[curr->end] == '\n') {
          if (E.cy > 0) E.cy--;
          E.numrows--;
          E.cx = E.rowNums[E.cy - E.rowoff] > 1 ? E.rowNums[E.cy - E.rowoff] : 0;
        }
        curr->prev->next = curr->next;
        curr->next->prev = curr->prev;
      } else if (curr->buffer[curr->end] == '\n') {
        if (E.cy > 0) E.cy--;
        E.numrows--;
        E.cx = E.rowNums[E.cy - E.rowoff] > 1 ? E.rowNums[E.cy - E.rowoff] : 0;
        curr->lines = (int *) realloc(curr->lines, (curr->linecount - 1) * sizeof(int));
        curr->linecount--;
        curr->end--;
      } else {
        curr->end--;
      }
    } else { //
      splitNodeDelete(curr, splitIndex);
    }
  } else if (offset == 0 && pos == 0) { // add new node to front of the currentNode
    curr = curr->prev;
    if (curr->end - curr->start == 0) {
      if (curr->buffer[curr->end] == '\n') {
        E.numrows--;
        if (E.cy > 0) E.cy--;
        E.cx = E.rowNums[E.cy - E.rowoff] > 1 ? E.rowNums[E.cy - E.rowoff] : 0;
      }
      curr->prev->next = curr->next;
      curr->next->prev = curr->prev;
    } else if (curr->buffer[curr->end] == '\n') {
      if (E.cy > 0) E.cy--;
      E.numrows--;
      E.cx = E.rowNums[E.cy - E.rowoff] > 1 ? E.rowNums[E.cy - E.rowoff] : 0;
      curr->lines = (int *) realloc(curr->lines, (curr->linecount - 1) * sizeof(int));
      curr->linecount--;
      curr->end--;
    } else {
      curr->end--;
    }
  } else if (offset == 0 && pos == 1) { // add new node to front of the currentNode
    if (curr->buffer[curr->start + pos] == '\n') {
      if (E.cy > 0) E.cy--;
      E.numrows--;
      E.cx = E.rowNums[E.cy - E.rowoff] > 1 ? E.rowNums[E.cy - E.rowoff] : 0;
      curr->lines = (int *) realloc(curr->lines, (curr->linecount - 1) * sizeof(int));
      curr->linecount--;
      curr->start++;
    } else {
      curr->start++;
    }
  } else { // split middle of the node
    offset += pos;
    splitNodeDelete(curr, offset);
  }
}

/*** terminal ***/

void die(const char *s) {
  write(STDOUT_FILENO, "\x1b[2J", 4);
  write(STDOUT_FILENO, "\x1b[H", 3);

  perror(s);
  exit(1);
}

void disableRawMode() {
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1)
    die("tcsetattr");
}

void enableRawMode() {
  if (tcgetattr(STDIN_FILENO, &E.orig_termios) == -1) die("tcgetattr");
  atexit(disableRawMode);

  struct termios raw = E.orig_termios;
  raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
  raw.c_oflag &= ~(OPOST);
  raw.c_cflag |= (CS8);
  raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
  raw.c_cc[VMIN] = 0;
  raw.c_cc[VTIME] = 1;

  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) die("tcsetattr");
}

int editorReadKey() {
  int nread;
  char c;
  while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
    if (nread == -1 && errno != EAGAIN) die("read");
  }

  if (c == '\x1b') {
    char seq[3];

    if (read(STDIN_FILENO, &seq[0], 1) != 1) return '\x1b';
    if (read(STDIN_FILENO, &seq[1], 1) != 1) return '\x1b';

    if (seq[0] == '[') {
      if (seq[1] >= '0' && seq[1] <= '9') {
        if (read(STDIN_FILENO, &seq[2], 1) != 1) return '\x1b';
        if (seq[2] == '~') {
          switch (seq[1]) {
            case '1': return HOME_KEY;
            case '3': return DEL_KEY;
            case '4': return END_KEY;
            case '5': return PAGE_UP;
            case '6': return PAGE_DOWN;
            case '7': return HOME_KEY;
            case '8': return END_KEY;
          }
        }
      } else {
        switch (seq[1]) {
          case 'A': return ARROW_UP;
          case 'B': return ARROW_DOWN;
          case 'C': return ARROW_RIGHT;
          case 'D': return ARROW_LEFT;
          case 'H': return HOME_KEY;
          case 'F': return END_KEY;
        }
      }
    } else if (seq[0] == 'O') {
      switch (seq[1]) {
        case 'H': return HOME_KEY;
        case 'F': return END_KEY;
      }
    }

    return '\x1b';
  } else {
    return c;
  }
}

int getCursorPosition(int *rows, int *cols) {
  char buf[32];
  unsigned int i = 0;

  if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4) return -1;

  while (i < sizeof(buf) - 1) {
    if (read(STDIN_FILENO, &buf[i], 1) != 1) break;
    if (buf[i] == 'R') break;
    i++;
  }
  buf[i] = '\0';

  if (buf[0] != '\x1b' || buf[1] != '[') return -1;
  if (sscanf(&buf[2], "%d;%d", rows, cols) != 2) return -1;

  return 0;
}

int getWindowSize(int *rows, int *cols) {
  struct winsize ws;

  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
    if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12) return -1;
    return getCursorPosition(rows, cols);
  } else {
    *cols = ws.ws_col;
    *rows = ws.ws_row;
    return 0;
  }
}

/*** row operations ***/

/*** file i/o ***/

void editorOpen(char *filename) {
  free(E.filename);
  E.filename = strdup(filename);

  FILE *f = fopen (filename, "rb");
  fseek (f, 0, SEEK_END);
  long fsize = ftell (f);
  fseek (f, 0, SEEK_SET);	/* same as rewind(f); */

  original = malloc (fsize + 1);
  fread (original, 1, fsize, f);

  head = getNewNode ("", -1, -1, 0);
  tail = getNewNode ("", -1, -1, 0);
  head->next = tail;
  tail->prev = head;

  node *curr = head;
  node *newNode = getNewNode (original, 0, fsize - 1, 0);
  newNode->next = curr->next;
  newNode->prev = curr;
  curr->next->prev = newNode;	//newNode will be the previous node of temp->next node
  curr->next = newNode;

  E.numrows = curr->next->linecount;
  fclose(f);
  E.dirty = 0;
}

void editorSave() {
  if (E.filename == NULL) {
    E.filename = editorPrompt("Save as: %s (ESC to cancel)");
    if (E.filename == NULL) {
      editorSetStatusMessage("Save attempt failed");
      return;
    }
  }

  FILE *fp = fopen(E.filename, "w+");
  if (fp != NULL) {
    int fileByteSize = 0;
    node *curr = head;
    while (curr != tail) {
      fprintf(fp, "%.*s", curr->end - curr->start + 1, curr->start + curr->buffer);
      if (curr != head) fileByteSize += curr->end - curr->start + 1;
      curr = curr->next;
    }
    editorSetStatusMessage("%d bytes written to disk", fileByteSize);
    E.dirty = 0;
    return;
  }
  fclose(fp);
  editorSetStatusMessage("Unable to save file! I/O error: %s", strerror(errno));
}

/*** append buffer ***/

struct abuf {
  char *b;
  int len;
};

#define ABUF_INIT {NULL, 0}

void abAppend(struct abuf *ab, const char *s, int len) {
  char *new = realloc(ab->b, ab->len + len);
  //printf ("[%c, %d]", s[0], len);
  if (new == NULL) return;
  memcpy(&new[ab->len], s, len);
  ab->b = new;
  ab->len += len;
}

void abFree(struct abuf *ab) {
  free(ab->b);
}

void printlines(struct abuf *ab, int topLine, int botLine) {
  node *curr = head->next;
  int currLine = 1;
  while (curr != tail && currLine < topLine) {
    if (currLine + curr->linecount >= topLine) break;
    currLine += curr->linecount;
    curr = curr->next;
  }

  int *rowLimits = (int *)calloc(botLine - topLine + 1, sizeof(int));
  int rowIndex = 0;
  int lineSum = 0;

  while (curr != tail && currLine <= botLine) {
    int topRange = 0;
    int f = topLine - currLine > 0 ? topLine - currLine : 0;
    for (int i = 0; i < f; i++) topRange += curr->lines[i];

    for (int i = topRange + curr->start; i <= curr->end; i++) {
      if (lineSum == botLine - topLine + 1) break;
      char *buf = curr->bufferType == 0 ? original : added;
      rowIndex++;
      //if (rowIndex < E.screencols + E.screencols) {
      if (buf[i] == '\n') {
        abAppend(ab, "\r\n", 2);
        rowLimits[lineSum] = rowIndex;
        lineSum++;
        rowIndex = 0;
      } else {
        if (rowIndex > E.coloff && rowIndex <= E.coloff + E.screencols) {
          if (isdigit(buf[i])) {
            abAppend(ab, "\x1b[31m", 5);
            abAppend(ab, &buf[i], 1);
            abAppend(ab, "\x1b[39m", 5);
          } else {
            abAppend(ab, &buf[i], 1);
          }
        }
      }
    }
    currLine += curr->linecount;
    curr = curr->next;
  }
  //for(int i = 0; i < botLine - topLine + 1; i++) printf("%d ", rowLimits[i]);
  E.rowNums = rowLimits;
}

/*** output ***/

void editorScroll() {
  if (E.cy < E.rowoff) {
    E.rowoff = E.cy;
  }
  if (E.cy >= E.rowoff + E.screenrows) {
    E.rowoff = E.cy - E.screenrows + 1;
  }
  if (E.cx < E.coloff) {
    E.coloff = E.cx;
  }
  if (E.cx >= E.coloff + E.screencols) {
    E.coloff = E.cx - E.screencols + 1;
  }
}

void editorDrawRows(struct abuf *ab) {
  if (E.filename == NULL && E.dirty == 0) {
    int y;
    for (y = 0; y < E.screenrows; y++) {
      int filerow = y + E.rowoff;
      if (filerow >= E.numrows) {
        if (y == E.screenrows / 3) {
          char welcome[80];
          int welcomelen = snprintf(welcome, sizeof(welcome),
            "Scrib Editor -- Version %s", SCRIB_VERSION);
          if (welcomelen > E.screencols) welcomelen = E.screencols;
          int padding = (E.screencols - welcomelen) / 2;
          if (padding) {
            abAppend(ab, "~", 1);
            padding--;
          }
          while (padding--) abAppend(ab, " ", 1);
          abAppend(ab, welcome, welcomelen);
        } else {
          abAppend(ab, "~", 1);
        }
      }
      abAppend(ab, "\r\n", 2);
    }
  } else {
    int last = E.rowoff + E.screenrows > E.numrows? E.numrows : E.rowoff + E.screenrows;
    printlines(ab, E.rowoff + 1, last);
    int i;
    for (i = last; i < E.screenrows; i++) {
      abAppend(ab, "~\r\n", 3);
    }
  }
}

void editorDrawStatusBar(struct abuf *ab) {
  abAppend(ab, "\x1b[7m", 4);
  char status[80], rstatus[80];
  int len = snprintf(status, sizeof(status), "%.20s - %d lines %s",
    E.filename ? E.filename : "[No Name]", E.numrows, E.dirty ? "(modified)" : "");
  int rightLen = snprintf(rstatus, sizeof(rstatus), "[%d, %d]", E.cy + 1, E.cx);
  if (len > E.screencols) len = E.screencols;
  abAppend(ab, status, len);
  while (len < E.screencols) {
    if (E.screencols - len == rightLen) {
      abAppend(ab, rstatus, rightLen);
      break;
    } else {
      abAppend(ab, " ", 1);
      len++;
    }
  }
  abAppend(ab, "\x1b[m", 3);
  abAppend(ab, "\r\n", 2);
}

void editorDrawMessageBar(struct abuf *ab) {
  abAppend(ab, "\x1b[K", 3);
  int msglen = strlen(E.statusmsg);
  if (msglen > E.screencols) msglen = E.screencols;
  if (msglen && time(NULL) - E.statusmsg_time < 5)
    abAppend(ab, E.statusmsg, msglen);
}

void editorRefreshScreen() {
  //printf("derp[%d]\r\n", bufferIndex);
//printf("._.");
  if (getWindowSize(&E.screenrows, &E.screencols) == -1) die("getWindowSize");
  E.screenrows -= 2;
  E.rowNums = (int *)realloc(E.rowNums, (E.screenrows + 1) * sizeof(int));
//printf("._.");
  editorScroll();

  struct abuf ab = ABUF_INIT;

  abAppend(&ab, "\x1b[?25l", 6);
  abAppend(&ab, "\x1b[2J", 4);
  abAppend(&ab, "\x1b[H", 3);
  //printf("._.");
  editorDrawRows(&ab);
  //printf("._.");
  editorDrawStatusBar(&ab);
  //printf("._.");
  editorDrawMessageBar(&ab);
//printf("derp[%d]\r\n", bufferIndex);
  char buf[32];
  snprintf(buf, sizeof(buf), "\x1b[%d;%dH", (E.cy - E.rowoff) + 1, (E.cx - E.coloff) + 1);
  abAppend(&ab, buf, strlen(buf));

  abAppend(&ab, "\x1b[?25h", 6);

  write(STDOUT_FILENO, ab.b, ab.len);
  abFree(&ab);
}

void editorSetStatusMessage(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(E.statusmsg, sizeof(E.statusmsg), fmt, ap);
  va_end(ap);
  E.statusmsg_time = time(NULL);
}

/*** input ***/

void editorMoveCursor(int key) {
  switch (key) {
    case ARROW_LEFT:
      if (E.cx != 0) { //limit left scroll
        E.cx--;
      } else if (E.cy > 0) { //moving left at start of a line
        E.cy--;
        E.cx = E.rowNums[E.cy - E.rowoff] > 1 ? E.rowNums[E.cy - E.rowoff] - 1 : 0;
      }
      break;
    case ARROW_RIGHT:
      if (E.cy < E.numrows && E.rowNums[E.cy - E.rowoff] - 1 > E.cx) { //limit right scroll
        E.cx++;
      } else if (E.cy + 1 < E.numrows){
        E.cy++;
        E.cx = 0;
      }
      break;
    case ARROW_UP:
      if (E.cy > 0) {
        E.cy--;
      }
      break;
    case ARROW_DOWN:
      if (E.cy + 1 < E.numrows) {
        E.cy++;
      }
      break;
  }

  // snap cursor to end of line
  if (E.cy < E.numrows && E.rowNums[E.cy - E.rowoff] - 1 < E.cx) {
    E.cx = E.rowNums[E.cy - E.rowoff] > 1 ? E.rowNums[E.cy - E.rowoff] - 1 : 0;
  }
}

void editorProcessKeypress() {

  int c = editorReadKey();

  switch (c) {
    case CTRL_KEY('q'):
      write(STDOUT_FILENO, "\x1b[2J", 4);
      write(STDOUT_FILENO, "\x1b[H", 3);
      exit(0);
      break;

    case CTRL_KEY('s'):
      editorSave();
      break;

    case 27:
      break;

    case HOME_KEY:
      E.cx = 0;
      break;

    case END_KEY:
      E.cx = E.screencols - 1;
      break;

    case BACKSPACE:
      deleteChar(E.cy + 1, E.cx);
      if (E.cx > 0) E.cx--;
      break;

    case PAGE_UP:
    case PAGE_DOWN:
      {
        int times = E.screenrows;
        while (times--)
          editorMoveCursor(c == PAGE_UP ? ARROW_UP : ARROW_DOWN);
      }
      break;

    case ARROW_UP:
    case ARROW_DOWN:
    case ARROW_LEFT:
    case ARROW_RIGHT:
      editorMoveCursor(c);
      break;

    default:
      c = (c == 13) ? '\n' : c;
      if (bufferIndex >= bufferSize - 1) {
        bufferSize *= 2;
        added = (char *)realloc(added, bufferSize * sizeof(char));
      }
      added[bufferIndex] = c;
      insertChar(E.cy + 1, E.cx);
      E.dirty++;
      if (c == '\n') {
        E.cx = 0;
        E.cy++;
        E.numrows++;
      } else {
        E.cx++;
      }
      break;
  }
}

/*** init ***/
char *editorPrompt(char *prompt) {
  size_t bufsize = 512;
  char *buf = malloc(bufsize);
  size_t buflen = 0;
  buf[0] = '\0';

  while (1) {
    editorSetStatusMessage(prompt, buf);
    editorRefreshScreen();
    int c = editorReadKey();
    if (c == DEL_KEY || c == CTRL_KEY('h') || c == BACKSPACE) {
      if (buflen != 0) buf[--buflen] = '\0';
    } else if (c == 27) {
      editorSetStatusMessage("");
      free(buf);
      return NULL;
    } else if (c == '\r') {
      if (buflen != 0) {
        editorSetStatusMessage("");
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
  }
}

void initEditor() {
  E.cx = 0;
  E.cy = 0;
  E.rowoff = 0;
  E.coloff = 0;
  E.numrows = 0;
  E.filename = NULL;
  E.statusmsg[0] = '\0';
  E.statusmsg_time = 0;
  E.dirty = 0;
  bufferIndex = 0;
  bufferSize = 1000;
  added = (char*) malloc(bufferSize * sizeof (char));

  if (getWindowSize(&E.screenrows, &E.screencols) == -1) die("getWindowSize");
  E.screenrows -= 2;
  E.rowNums = (int *)calloc(E.screenrows + 1, sizeof(int));
}

int main(int argc, char *argv[]) {
  //printf ("[done]");
  enableRawMode();
  initEditor();
  if (argc >= 2) {
    editorOpen(argv[1]);
  } else {
    original = "\n";
    head = getNewNode ("", -1, -1, 0);
    tail = getNewNode ("", -1, -1, 0);
    head->next = tail;
    tail->prev = head;

    node *curr = head;
    node *newNode = getNewNode (original, 0, 0, 0);
    newNode->next = curr->next;
    newNode->prev = curr;
    curr->next->prev = newNode;	//newNode will be the previous node of temp->next node
    curr->next = newNode;

    E.numrows = curr->next->linecount;
  }

  editorSetStatusMessage("HELP: Ctrl-S = save | Ctrl-Q = quit");

  while (1) {
    editorRefreshScreen();
    editorProcessKeypress();
  }

  return 0;
}
