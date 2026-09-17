/* Минимальные системные вызовы newlib: ввод-вывод не используется. */
#include <sys/stat.h>
#include <errno.h>
int _close(int f) { (void)f; return -1; }
int _lseek(int f, int p, int d) { (void)f; (void)p; (void)d; return 0; }
int _read(int f, char *b, int n) { (void)f; (void)b; (void)n; return 0; }
int _write(int f, char *b, int n) { (void)f; (void)b; return n; }
int _fstat(int f, struct stat *s) { (void)f; s->st_mode = S_IFCHR; return 0; }
int _isatty(int f) { (void)f; return 1; }
