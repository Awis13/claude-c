// chat.h — интерактивный REPL: чат-цикл с историей сообщений

#ifndef CHAT_H
#define CHAT_H

#include "config.h"
#include "api.h"

// запустить интерактивный REPL
// возвращает 0 при нормальном выходе
int chat_repl(const config_t *cfg);

// выполнить один запрос и напечатать результат в stdout
// возвращает 0 при успехе
int chat_oneshot(const config_t *cfg, const char *prompt);

#endif // CHAT_H
