// stream.h — SSE парсер: разбор data: строк из Server-Sent Events

#ifndef STREAM_H
#define STREAM_H

#include <stddef.h>

// тип SSE события
typedef enum {
    SSE_EVENT_DATA,     // data: ...
    SSE_EVENT_DONE,     // data: [DONE]
    SSE_EVENT_ERROR     // ошибка парсинга
} sse_event_type_t;

// callback для обработанных SSE событий
typedef void (*sse_event_callback_t)(sse_event_type_t type,
                                      const char *data, size_t len,
                                      void *userdata);

// состояние SSE парсера (буферизация неполных строк)
typedef struct {
    char buf[65536];        // буфер для неполных строк
    size_t buf_len;         // сколько данных в буфере
    sse_event_callback_t callback;
    void *userdata;
} sse_parser_t;

void sse_parser_init(sse_parser_t *p, sse_event_callback_t cb, void *userdata);
void sse_parser_feed(sse_parser_t *p, const char *data, size_t len);
void sse_parser_reset(sse_parser_t *p);

#endif // STREAM_H
