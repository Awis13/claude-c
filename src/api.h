// api.h — общий интерфейс для API провайдеров: сообщения, callbacks

#ifndef API_H
#define API_H

#include "config.h"

// роль сообщения
typedef enum {
    MSG_ROLE_SYSTEM,
    MSG_ROLE_USER,
    MSG_ROLE_ASSISTANT
} msg_role_t;

// сообщение в истории
typedef struct {
    msg_role_t role;
    char *content;      // malloc, нужно free
} message_t;

// история сообщений (динамический массив)
typedef struct {
    message_t *items;
    int count;
    int capacity;
} message_list_t;

void message_list_init(message_list_t *list);
void message_list_add(message_list_t *list, msg_role_t role, const char *content);
void message_list_free(message_list_t *list);

// callback для стриминга текста (content или reasoning)
typedef void (*text_callback_t)(const char *text, int is_reasoning, void *userdata);

// callback для завершения ответа
typedef void (*done_callback_t)(void *userdata);

#endif // API_H
