// chat.h — интерактивный REPL: чат-цикл с историей сообщений

#ifndef CHAT_H
#define CHAT_H

#include "config.h"
#include "api.h"

// запустить интерактивный REPL
// возвращает 0 при нормальном выходе
int chat_repl(const config_t *cfg);

#endif // CHAT_H
