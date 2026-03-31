// api.c — реализация message_list: инициализация, добавление, освобождение

#include <stdlib.h>
#include <string.h>
#include "api.h"

const char *role_to_str(msg_role_t role) {
    switch (role) {
    case MSG_ROLE_SYSTEM:    return "system";
    case MSG_ROLE_USER:      return "user";
    case MSG_ROLE_ASSISTANT: return "assistant";
    }
    return "user";
}

void message_list_init(message_list_t *list) {
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
}

void message_list_add(message_list_t *list, msg_role_t role, const char *content) {
    // расширяем массив при необходимости
    if (list->count >= list->capacity) {
        int new_cap = list->capacity == 0 ? 8 : list->capacity * 2;
        message_t *new_items = realloc(list->items, (size_t)new_cap * sizeof(message_t));
        if (!new_items) return;
        list->items = new_items;
        list->capacity = new_cap;
    }

    message_t *msg = &list->items[list->count];
    msg->role = role;
    msg->content = strdup(content);
    list->count++;
}

void message_list_free(message_list_t *list) {
    for (int i = 0; i < list->count; i++) {
        free(list->items[i].content);
    }
    free(list->items);
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
}
