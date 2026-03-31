// api_anthropic.h — Anthropic Messages API клиент

#ifndef API_ANTHROPIC_H
#define API_ANTHROPIC_H

#include "api.h"
#include "config.h"

// отправить запрос к Anthropic Messages API
// стримит ответ через text_callback, вызывает done_callback по завершении
// возвращает 0 при успехе, -1 при ошибке
int api_anthropic_chat(const config_t *cfg,
                       const message_list_t *messages,
                       text_callback_t on_text,
                       done_callback_t on_done,
                       void *userdata);

#endif // API_ANTHROPIC_H
