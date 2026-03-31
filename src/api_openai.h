// api_openai.h — OpenAI-compatible API клиент (Ollama, vLLM, LM Studio, etc.)

#ifndef API_OPENAI_H
#define API_OPENAI_H

#include "api.h"
#include "config.h"

// отправить запрос к OpenAI-compatible API
// стримит ответ через text_callback, вызывает done_callback по завершении
// возвращает 0 при успехе, -1 при ошибке
int api_openai_chat(const config_t *cfg,
                    const message_list_t *messages,
                    text_callback_t on_text,
                    done_callback_t on_done,
                    void *userdata,
                    volatile int *interrupt_flag);

#endif // API_OPENAI_H
