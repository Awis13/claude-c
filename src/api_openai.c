// api_openai.c — реализация OpenAI-compatible API клиента
// стриминг через SSE, парсинг delta.content / delta.reasoning

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "api_openai.h"
#include "http.h"
#include "stream.h"
#include "cJSON.h"

// контекст для SSE парсера — пробрасывает callbacks наружу
typedef struct {
    text_callback_t on_text;
    done_callback_t on_done;
    void *userdata;
    int error;  // флаг ошибки SSE
} openai_ctx_t;

// SSE callback — парсим JSON чанк, извлекаем delta
static void openai_sse_event(sse_event_type_t type, const char *data,
                              size_t len, void *userdata) {
    openai_ctx_t *ctx = (openai_ctx_t *)userdata;

    switch (type) {
    case SSE_EVENT_DATA: {
        cJSON *root = cJSON_ParseWithLength(data, len);
        if (!root) return;

        // choices[0].delta.content / reasoning
        cJSON *choices = cJSON_GetObjectItem(root, "choices");
        if (choices && cJSON_IsArray(choices)) {
            cJSON *first = cJSON_GetArrayItem(choices, 0);
            if (first) {
                cJSON *delta = cJSON_GetObjectItem(first, "delta");
                if (delta) {
                    // сначала content
                    cJSON *content = cJSON_GetObjectItem(delta, "content");
                    if (content && cJSON_IsString(content) &&
                        content->valuestring && content->valuestring[0]) {
                        ctx->on_text(content->valuestring, 0, ctx->userdata);
                    }

                    // потом reasoning (thinking-модели: qwen3 и т.д.)
                    cJSON *reasoning = cJSON_GetObjectItem(delta, "reasoning");
                    if (reasoning && cJSON_IsString(reasoning) &&
                        reasoning->valuestring && reasoning->valuestring[0]) {
                        ctx->on_text(reasoning->valuestring, 1, ctx->userdata);
                    }
                }
            }
        }
        cJSON_Delete(root);
        break;
    }
    case SSE_EVENT_DONE:
        if (ctx->on_done) {
            ctx->on_done(ctx->userdata);
        }
        break;
    case SSE_EVENT_ERROR:
        ctx->error = 1;
        break;
    }
}

// curl чанки → SSE парсер
static void openai_stream_data(const char *data, size_t len, void *userdata) {
    sse_parser_t *parser = (sse_parser_t *)userdata;
    sse_parser_feed(parser, data, len);
}

int api_openai_chat(const config_t *cfg,
                    const message_list_t *messages,
                    text_callback_t on_text,
                    done_callback_t on_done,
                    void *userdata,
                    volatile int *interrupt_flag) {
    // формируем URL
    char url[1024];
    snprintf(url, sizeof(url), "%s/v1/chat/completions", cfg->endpoint);

    // формируем JSON тело
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "model", cfg->model);
    cJSON_AddBoolToObject(root, "stream", 1);

    if (cfg->max_tokens > 0) {
        cJSON_AddNumberToObject(root, "max_tokens", cfg->max_tokens);
    }

    if (cfg->temperature >= 0) {
        cJSON_AddNumberToObject(root, "temperature", cfg->temperature);
    }

    // массив сообщений
    cJSON *msgs = cJSON_AddArrayToObject(root, "messages");
    for (int i = 0; i < messages->count; i++) {
        cJSON *msg = cJSON_CreateObject();
        cJSON_AddStringToObject(msg, "role", role_to_str(messages->items[i].role));
        cJSON_AddStringToObject(msg, "content", messages->items[i].content);
        cJSON_AddItemToArray(msgs, msg);
    }

    char *body = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    if (!body) return -1;

    // заголовки
    const char *headers[3] = { NULL, NULL, NULL };
    char auth_header[512];
    int h = 0;
    if (cfg->api_key[0] != '\0') {
        snprintf(auth_header, sizeof(auth_header),
                 "Authorization: Bearer %s", cfg->api_key);
        headers[h++] = auth_header;
    }

    // контекст для SSE парсера
    openai_ctx_t ctx = {
        .on_text = on_text,
        .on_done = on_done,
        .userdata = userdata,
        .error = 0
    };

    sse_parser_t parser;
    sse_parser_init(&parser, openai_sse_event, &ctx);

    // отправляем запрос
    long status = http_post_stream(url, body, headers, openai_stream_data, &parser,
                                   interrupt_flag);
    free(body);

    if (status == 0 || status != 200 || ctx.error) {
        return -1;
    }

    return 0;
}
