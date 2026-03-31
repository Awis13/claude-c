// stream.c — SSE парсер: разбор data: строк из Server-Sent Events

#include "stream.h"

#include <string.h>

// ---------- sse_parser_init ----------

void sse_parser_init(sse_parser_t *p, sse_event_callback_t cb, void *userdata) {
    memset(p->buf, 0, sizeof(p->buf));
    p->buf_len = 0;
    p->callback = cb;
    p->userdata = userdata;
}

// ---------- sse_parser_reset ----------

void sse_parser_reset(sse_parser_t *p) {
    p->buf_len = 0;
}

// ---------- обработка одной строки ----------

// обработать одну полную строку SSE (без \n)
static void process_line(sse_parser_t *p, const char *line, size_t len) {
    // пустая строка — разделитель событий, пропускаем
    if (len == 0) return;

    // комментарий (начинается с ':')
    if (line[0] == ':') return;

    // event:, id:, retry: — пропускаем (не нужны для нашего случая)
    if (len >= 6 && strncmp(line, "event:", 6) == 0) return;
    if (len >= 3 && strncmp(line, "id:", 3) == 0) return;
    if (len >= 6 && strncmp(line, "retry:", 6) == 0) return;

    // data: — основной тип
    if (len >= 5 && strncmp(line, "data:", 5) == 0) {
        const char *data = line + 5;
        size_t data_len = len - 5;

        // пропустить пробел после "data:"
        if (data_len > 0 && data[0] == ' ') {
            data++;
            data_len--;
        }

        // проверить [DONE]
        if (data_len == 6 && strncmp(data, "[DONE]", 6) == 0) {
            p->callback(SSE_EVENT_DONE, data, data_len, p->userdata);
            return;
        }

        // обычные данные
        p->callback(SSE_EVENT_DATA, data, data_len, p->userdata);
        return;
    }

    // неизвестный формат — ошибка
    p->callback(SSE_EVENT_ERROR, line, len, p->userdata);
}

// ---------- sse_parser_feed ----------

void sse_parser_feed(sse_parser_t *p, const char *data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        if (data[i] == '\n') {
            // нашли конец строки — обработать
            // убрать \r если есть (CRLF)
            size_t line_len = p->buf_len;
            if (line_len > 0 && p->buf[line_len - 1] == '\r') {
                line_len--;
            }
            p->buf[line_len] = '\0';
            process_line(p, p->buf, line_len);
            p->buf_len = 0;
        } else {
            // добавить символ в буфер
            if (p->buf_len < sizeof(p->buf) - 1) {
                p->buf[p->buf_len++] = data[i];
            }
            // если буфер переполнен — молча отбрасываем (защита от мусора)
        }
    }
}
