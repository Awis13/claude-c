// http.c — HTTP клиент на libcurl: POST запросы, SSE стриминг

#include "http.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>

// ---------- инициализация ----------

void http_init(void) {
    curl_global_init(CURL_GLOBAL_DEFAULT);
}

void http_cleanup(void) {
    curl_global_cleanup();
}

// ---------- внутренние структуры ----------

// контекст для стриминга
typedef struct {
    stream_callback_t callback;
    void *userdata;
    volatile int *interrupt_flag;
} stream_ctx_t;

// контекст для буферизованного ответа
typedef struct {
    char *data;
    size_t len;
    size_t cap;
} buffer_ctx_t;

// ---------- curl write callbacks ----------

// callback для стриминга — передаёт данные напрямую в пользовательский callback
// возвращает 0 при прерывании — curl прервёт transfer
static size_t stream_write_cb(char *ptr, size_t size, size_t nmemb, void *userdata) {
    size_t total = size * nmemb;
    stream_ctx_t *ctx = (stream_ctx_t *)userdata;

    // проверяем флаг прерывания — прекращаем запись
    if (ctx->interrupt_flag && *ctx->interrupt_flag) {
        return 0;
    }

    ctx->callback(ptr, total, ctx->userdata);
    return total;
}

// callback для буферизованного ответа — накапливает данные
static size_t buffer_write_cb(char *ptr, size_t size, size_t nmemb, void *userdata) {
    size_t total = size * nmemb;
    buffer_ctx_t *ctx = (buffer_ctx_t *)userdata;

    // расширить буфер при необходимости
    if (ctx->len + total + 1 > ctx->cap) {
        size_t new_cap = (ctx->cap == 0) ? 4096 : ctx->cap * 2;
        while (new_cap < ctx->len + total + 1) {
            new_cap *= 2;
        }
        char *tmp = realloc(ctx->data, new_cap);
        if (!tmp) {
            fprintf(stderr, "http: ошибка выделения памяти\n");
            return 0;
        }
        ctx->data = tmp;
        ctx->cap = new_cap;
    }

    memcpy(ctx->data + ctx->len, ptr, total);
    ctx->len += total;
    ctx->data[ctx->len] = '\0';
    return total;
}

// ---------- progress callback для прерывания ----------

// progress callback — проверяет флаг прерывания, return 1 = abort curl
static int progress_cb(void *clientp,
                       curl_off_t dltotal, curl_off_t dlnow,
                       curl_off_t ultotal, curl_off_t ulnow) {
    (void)dltotal; (void)dlnow; (void)ultotal; (void)ulnow;
    volatile int *flag = (volatile int *)clientp;
    if (flag && *flag) {
        return 1;  // прервать transfer
    }
    return 0;
}

// ---------- общая настройка curl ----------

// настроить общие параметры curl handle
static struct curl_slist *setup_curl(CURL *curl, const char *url,
                                      const char *body, const char **headers) {
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 30L);

    // заголовки: всегда Content-Type + пользовательские
    struct curl_slist *slist = NULL;
    slist = curl_slist_append(slist, "Content-Type: application/json");

    if (headers) {
        for (const char **h = headers; *h != NULL; h++) {
            slist = curl_slist_append(slist, *h);
        }
    }

    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, slist);
    return slist;
}

// ---------- http_post_stream ----------

long http_post_stream(const char *url, const char *body,
                      const char **headers,
                      stream_callback_t callback, void *userdata,
                      volatile int *interrupt_flag) {
    CURL *curl = curl_easy_init();
    if (!curl) {
        fprintf(stderr, "http: не удалось создать curl handle\n");
        return 0;
    }

    struct curl_slist *slist = setup_curl(curl, url, body, headers);

    // для стрима — без общего таймаута (может длиться долго)
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 0L);

    // callback для данных — прекращает запись при interrupt
    stream_ctx_t ctx = {
        .callback = callback,
        .userdata = userdata,
        .interrupt_flag = interrupt_flag
    };
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, stream_write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &ctx);

    // progress callback — прерывает curl при interrupt
    if (interrupt_flag) {
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
        curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, progress_cb);
        curl_easy_setopt(curl, CURLOPT_XFERINFODATA, (void *)interrupt_flag);
    }

    CURLcode res = curl_easy_perform(curl);

    long status = 0;
    if (res == CURLE_ABORTED_BY_CALLBACK) {
        // прервано по Ctrl+C — не ошибка, получаем status если успели
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    } else if (res != CURLE_OK) {
        fprintf(stderr, "http: ошибка запроса: %s\n", curl_easy_strerror(res));
    } else {
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    }

    curl_slist_free_all(slist);
    curl_easy_cleanup(curl);
    return status;
}

// ---------- http_post ----------

int http_post(const char *url, const char *body,
              const char **headers, http_response_t *resp) {
    memset(resp, 0, sizeof(*resp));

    CURL *curl = curl_easy_init();
    if (!curl) {
        fprintf(stderr, "http: не удалось создать curl handle\n");
        return -1;
    }

    struct curl_slist *slist = setup_curl(curl, url, body, headers);

    // таймаут 300 секунд для обычных запросов
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 300L);

    // callback для буферизации
    buffer_ctx_t ctx = { .data = NULL, .len = 0, .cap = 0 };
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, buffer_write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &ctx);

    CURLcode res = curl_easy_perform(curl);

    if (res != CURLE_OK) {
        fprintf(stderr, "http: ошибка запроса: %s\n", curl_easy_strerror(res));
        free(ctx.data);
        curl_slist_free_all(slist);
        curl_easy_cleanup(curl);
        return -1;
    }

    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &resp->status_code);
    resp->body = ctx.data;
    resp->body_len = ctx.len;

    curl_slist_free_all(slist);
    curl_easy_cleanup(curl);
    return 0;
}

// ---------- http_response_free ----------

void http_response_free(http_response_t *resp) {
    free(resp->body);
    resp->body = NULL;
    resp->body_len = 0;
    resp->status_code = 0;
}
