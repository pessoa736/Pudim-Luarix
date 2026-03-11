#ifndef KBOOTLOG_H
#define KBOOTLOG_H

void kbootlog_title(const char* title);
void kbootlog_write(const char* msg);
void kbootlog_writeln(const char* msg);
void kbootlog_line(const char* title, const char* msg);

#endif