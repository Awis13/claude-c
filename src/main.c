// claude-c — полноценный AI CLI клиент на чистом C
// точка входа

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "http.h"
#include "stream.h"
#include "cJSON.h"

// ---------- SSE callback для тестового запроса ----------

// обработка SSE события — парсим JSON, извлекаем delta.content
static void on_sse_event(sse_event_type_t type, const char *data,
                          size_t len, void *userdata) {
    (void)userdata;

    switch (type) {
    case SSE_EVENT_DATA: {
        // парсим JSON чанк
        cJSON *root = cJSON_ParseWithLength(data, len);
        if (!root) return;

        // choices[0].delta.content
        cJSON *choices = cJSON_GetObjectItem(root, "choices");
        if (choices && cJSON_IsArray(choices)) {
            cJSON *first = cJSON_GetArrayItem(choices, 0);
            if (first) {
                cJSON *delta = cJSON_GetObjectItem(first, "delta");
                if (delta) {
                    cJSON *content = cJSON_GetObjectItem(delta, "content");
                    if (content && cJSON_IsString(content) && content->valuestring) {
                        printf("%s", content->valuestring);
                        fflush(stdout);
                    }
                }
            }
        }
        cJSON_Delete(root);
        break;
    }
    case SSE_EVENT_DONE:
        printf("\n");
        break;
    case SSE_EVENT_ERROR:
        fprintf(stderr, "\n[SSE ошибка: %.*s]\n", (int)len, data);
        break;
    }
}

// callback-обёртка: curl чанки → SSE парсер
static void on_stream_data(const char *data, size_t len, void *userdata) {
    sse_parser_t *parser = (sse_parser_t *)userdata;
    sse_parser_feed(parser, data, len);
}

// ---------- тестовый запрос ----------

static void test_stream(const config_t *cfg) {
    // формируем URL: endpoint + /v1/chat/completions
    char url[1024];
    snprintf(url, sizeof(url), "%s/v1/chat/completions", cfg->endpoint);

    // формируем JSON тело запроса
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "model", cfg->model);
    cJSON_AddBoolToObject(root, "stream", 1);
    cJSON_AddNumberToObject(root, "max_tokens", 50);

    cJSON *messages = cJSON_AddArrayToObject(root, "messages");
    cJSON *msg = cJSON_CreateObject();
    cJSON_AddStringToObject(msg, "role", "user");
    cJSON_AddStringToObject(msg, "content", "say hello in one word");
    cJSON_AddItemToArray(messages, msg);

    char *body = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    if (!body) {
        fprintf(stderr, "ошибка формирования JSON\n");
        return;
    }

    printf("→ POST %s\n", url);
    printf("→ модель: %s\n\n", cfg->model);

    // заголовки (API ключ если есть)
    const char *headers[3] = { NULL, NULL, NULL };
    char auth_header[512];
    int h = 0;
    if (cfg->api_key[0] != '\0') {
        snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", cfg->api_key);
        headers[h++] = auth_header;
    }

    // SSE парсер
    sse_parser_t parser;
    sse_parser_init(&parser, on_sse_event, NULL);

    // отправляем запрос
    long status = http_post_stream(url, body, headers, on_stream_data, &parser);

    if (status == 0) {
        fprintf(stderr, "\nОшибка соединения — сервер недоступен?\n");
    } else if (status != 200) {
        fprintf(stderr, "\nHTTP %ld\n", status);
    }

    free(body);
}

// ---------- main ----------

int main(int argc, char *argv[]) {
    config_t cfg;

    // загрузка конфига: дефолты → файл → env → CLI
    config_init(&cfg);
    config_load_file(&cfg);
    config_load_env(&cfg);

    int rc = config_parse_args(&cfg, argc, argv);
    if (rc != 0) {
        // 1 = help/version (уже напечатано), -1 = ошибка
        return rc < 0 ? 1 : 0;
    }

    // показать конфиг
    config_dump(&cfg);

    // тестовый стриминг (если не programmatic режим)
    if (!cfg.programmatic) {
        printf("\n--- тест стриминга ---\n");
        http_init();
        test_stream(&cfg);
        http_cleanup();
    }

    return 0;
}
