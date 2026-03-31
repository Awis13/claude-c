// http.h — HTTP клиент на libcurl: POST запросы, SSE стриминг

#ifndef HTTP_H
#define HTTP_H

#include <stddef.h>

// callback для получения данных от стрима
typedef void (*stream_callback_t)(const char *data, size_t len, void *userdata);

// HTTP ответ (для не-стриминговых запросов)
typedef struct {
    char *body;         // тело ответа (malloc, нужно free)
    size_t body_len;    // длина тела
    long status_code;   // HTTP код
} http_response_t;

// инициализация/очистка curl
void http_init(void);
void http_cleanup(void);

// POST с SSE стримингом — вызывает callback на каждый чанк данных
// headers: NULL-terminated массив строк "Key: Value"
// interrupt_flag: если не NULL и *interrupt_flag != 0 — прервать transfer
// возвращает HTTP status code, 0 при ошибке
long http_post_stream(const char *url, const char *body,
                      const char **headers,
                      stream_callback_t callback, void *userdata,
                      volatile int *interrupt_flag);

// POST без стриминга — возвращает весь ответ
// caller должен вызвать http_response_free()
int http_post(const char *url, const char *body,
              const char **headers, http_response_t *resp);

void http_response_free(http_response_t *resp);

#endif // HTTP_H
