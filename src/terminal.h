// terminal.h — ANSI вывод и простой markdown rendering для стриминга

#ifndef TERMINAL_H
#define TERMINAL_H

// ANSI коды
#define ANSI_RESET    "\033[0m"
#define ANSI_BOLD     "\033[1m"
#define ANSI_DIM      "\033[2m"
#define ANSI_RED      "\033[31m"
#define ANSI_GREEN    "\033[32m"
#define ANSI_YELLOW   "\033[33m"
#define ANSI_BLUE     "\033[34m"
#define ANSI_MAGENTA  "\033[35m"
#define ANSI_CYAN     "\033[36m"
#define ANSI_GRAY     "\033[90m"
#define ANSI_BG_GRAY  "\033[48;5;236m"

// проверить поддержку цветов (isatty + TERM)
int terminal_supports_color(void);

// промпт и метки
void term_print_user_prompt(void);      // "you> " зелёным
void term_print_assistant_label(void);  // "assistant> " синим
void term_print_system(const char *msg); // системные сообщения серым
void term_print_error(const char *msg);  // ошибки красным

// состояние stateful markdown парсера
typedef struct {
    int in_code_block;      // внутри ``` блока
    int in_inline_code;     // внутри `code`
    int in_bold;            // внутри **bold**
    int line_start;         // начало строки (для заголовков, буллетов)
    int color_enabled;      // цвета включены
    char pending;           // буфер для незавершённых токенов (*, `)
} term_state_t;

void term_state_init(term_state_t *s);

// обработать и напечатать чанк текста с markdown подсветкой
// чанки могут разрывать markdown — stateful парсер
void term_print_chunk(term_state_t *s, const char *text);

// сбросить состояние после ответа (закрыть открытые стили)
void term_print_end(term_state_t *s);

#endif
