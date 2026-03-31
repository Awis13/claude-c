// api_anthropic.c — реализация Anthropic Messages API клиента
// стриминг через SSE, парсинг content_block_delta / message_delta

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "api_anthropic.h"
#include "http.h"
#include "stream.h"
#include "cJSON.h"

// контекст для SSE парсера — пробрасывает callbacks наружу
typedef struct {
    text_callback_t on_text;
    done_callback_t on_done;
    void *userdata;
    int error;  // флаг ошибки SSE
} anthropic_ctx_t;

// SSE callback — парсим JSON чанк, извлекаем текст по полю type
// Anthropic шлёт event: перед data:, но SSE парсер пропускает event: строки,
// поэтому определяем тип события по JSON полю "type"
static void anthropic_sse_event(sse_event_type_t type, const char *data,
                                size_t len, void *userdata) {
    anthropic_ctx_t *ctx = (anthropic_ctx_t *)userdata;

    switch (type) {
    case SSE_EVENT_DATA: {
        cJSON *root = cJSON_ParseWithLength(data, len);
        if (!root) return;

        cJSON *evt_type = cJSON_GetObjectItem(root, "type");
        if (!evt_type || !cJSON_IsString(evt_type)) {
            cJSON_Delete(root);
            return;
        }

        const char *t = evt_type->valuestring;

        if (strcmp(t, "content_block_delta") == 0) {
            // delta.text — основной текст ответа
            cJSON *delta = cJSON_GetObjectItem(root, "delta");
            if (delta) {
                cJSON *text = cJSON_GetObjectItem(delta, "text");
                if (text && cJSON_IsString(text) &&
                    text->valuestring && text->valuestring[0]) {
                    ctx->on_text(text->valuestring, 0, ctx->userdata);
                }
            }
        } else if (strcmp(t, "message_delta") == 0) {
            // delta.stop_reason — конец сообщения
            cJSON *delta = cJSON_GetObjectItem(root, "delta");
            if (delta) {
                cJSON *stop = cJSON_GetObjectItem(delta, "stop_reason");
                if (stop && cJSON_IsString(stop)) {
                    if (ctx->on_done) {
                        ctx->on_done(ctx->userdata);
                    }
                }
            }
        }
        // ping, message_start, content_block_start/stop — пропускаем

        cJSON_Delete(root);
        break;
    }
    case SSE_EVENT_DONE:
        // [DONE] — OpenAI-стиль, Anthropic не шлёт, но на всякий случай
        if (ctx->on_done) {
            ctx->on_done(ctx->userdata);
        }
        break;
    case SSE_EVENT_ERROR:
        ctx->error = 1;
        break;
    }
}

// curl чанки -> SSE парсер
static void anthropic_stream_data(const char *data, size_t len, void *userdata) {
    sse_parser_t *parser = (sse_parser_t *)userdata;
    sse_parser_feed(parser, data, len);
}

int api_anthropic_chat(const config_t *cfg,
                       const message_list_t *messages,
                       text_callback_t on_text,
                       done_callback_t on_done,
                       void *userdata,
                       volatile int *interrupt_flag) {
    // формируем URL
    char url[1024];
    snprintf(url, sizeof(url), "%s/v1/messages", cfg->endpoint);

    // формируем JSON тело
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "model", cfg->model);
    cJSON_AddNumberToObject(root, "max_tokens", cfg->max_tokens > 0 ? cfg->max_tokens : 4096);
    cJSON_AddBoolToObject(root, "stream", 1);

    if (cfg->temperature >= 0) {
        cJSON_AddNumberToObject(root, "temperature", cfg->temperature);
    }

    // system сообщение — отдельным полем, не в messages
    // остальные — в массив messages
    cJSON *msgs = cJSON_AddArrayToObject(root, "messages");
    for (int i = 0; i < messages->count; i++) {
        if (messages->items[i].role == MSG_ROLE_SYSTEM) {
            cJSON_AddStringToObject(root, "system", messages->items[i].content);
            continue;
        }
        cJSON *msg = cJSON_CreateObject();
        cJSON_AddStringToObject(msg, "role", role_to_str(messages->items[i].role));
        cJSON_AddStringToObject(msg, "content", messages->items[i].content);
        cJSON_AddItemToArray(msgs, msg);
    }

    char *body = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    if (!body) return -1;

    // заголовки: x-api-key, anthropic-version (Content-Type ставит http.c)
    char key_header[512];
    const char *headers[3] = { NULL, NULL, NULL };
    int h = 0;

    if (cfg->api_key[0] != '\0') {
        snprintf(key_header, sizeof(key_header), "x-api-key: %s", cfg->api_key);
        headers[h++] = key_header;
    }
    headers[h++] = "anthropic-version: 2023-06-01";

    // контекст для SSE парсера
    anthropic_ctx_t ctx = {
        .on_text = on_text,
        .on_done = on_done,
        .userdata = userdata,
        .error = 0
    };

    sse_parser_t parser;
    sse_parser_init(&parser, anthropic_sse_event, &ctx);

    // отправляем запрос
    long status = http_post_stream(url, body, headers, anthropic_stream_data, &parser,
                                   interrupt_flag);
    free(body);

    if (status == 0 || status != 200 || ctx.error) {
        return -1;
    }

    return 0;
}
