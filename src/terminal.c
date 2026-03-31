// terminal.c — ANSI вывод и простой markdown rendering для стриминга

#include "terminal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// ---------- определение поддержки цветов ----------

int terminal_supports_color(void) {
    // stdout должен быть терминалом
    if (!isatty(STDOUT_FILENO)) return 0;

    // проверить TERM (dumb = без цветов)
    const char *term = getenv("TERM");
    if (!term || strcmp(term, "dumb") == 0) return 0;

    // NO_COLOR convention (https://no-color.org/)
    if (getenv("NO_COLOR")) return 0;

    return 1;
}

// ---------- промпт и метки ----------

void term_print_user_prompt(void) {
    // readline сам выводит prompt, тут не нужны цвета
    // эта функция для получения строки промпта
}

void term_print_assistant_label(void) {
    if (terminal_supports_color()) {
        printf(ANSI_BOLD ANSI_BLUE "assistant>" ANSI_RESET " ");
    } else {
        printf("assistant> ");
    }
    fflush(stdout);
}

void term_print_system(const char *msg) {
    if (terminal_supports_color()) {
        printf(ANSI_GRAY "%s" ANSI_RESET "\n", msg);
    } else {
        printf("%s\n", msg);
    }
}

void term_print_error(const char *msg) {
    if (terminal_supports_color()) {
        fprintf(stderr, ANSI_BOLD ANSI_RED "%s" ANSI_RESET "\n", msg);
    } else {
        fprintf(stderr, "%s\n", msg);
    }
}

// ---------- stateful markdown парсер ----------

void term_state_init(term_state_t *s) {
    memset(s, 0, sizeof(*s));
    s->line_start = 1;  // начинаем с начала строки
    s->color_enabled = terminal_supports_color();
}

// вспомогательная: применить текущий стиль
static void apply_style(term_state_t *s) {
    if (!s->color_enabled) return;

    if (s->in_code_block) {
        printf(ANSI_BG_GRAY ANSI_CYAN);
    } else if (s->in_inline_code) {
        printf(ANSI_CYAN);
    } else if (s->in_bold) {
        printf(ANSI_BOLD);
    }
}

// вспомогательная: сбросить стиль
static void reset_style(term_state_t *s) {
    if (!s->color_enabled) return;
    printf(ANSI_RESET);
}

// вывести один символ с текущим стилем
static void emit_char(term_state_t *s, char c) {
    apply_style(s);
    putchar(c);
    reset_style(s);

    // отслеживать начало строки
    s->line_start = (c == '\n');
}

void term_print_chunk(term_state_t *s, const char *text) {
    if (!text || !*text) return;

    size_t len = strlen(text);

    for (size_t i = 0; i < len; i++) {
        char c = text[i];

        // обработать pending символ
        if (s->pending) {
            char prev = s->pending;
            s->pending = 0;

            if (prev == '`') {
                // проверяем ``` (code block)
                if (c == '`' && i + 1 < len && text[i + 1] == '`') {
                    // тройные бэктики — переключить code block
                    s->in_code_block = !s->in_code_block;
                    // пропустить до конца строки (язык после ```)
                    i += 1; // пропустили второй и третий `
                    // пропустить остаток строки (название языка)
                    while (i + 1 < len && text[i + 1] != '\n') {
                        i++;
                    }
                    if (s->in_code_block && s->color_enabled) {
                        // начало блока — перевод строки уже будет от следующего символа
                    }
                    continue;
                }
                // одиночный ` — переключить inline code
                if (!s->in_code_block) {
                    s->in_inline_code = !s->in_inline_code;
                }
                // текущий символ c обработать как обычный
                // (fall through к обработке c ниже)
            } else if (prev == '*') {
                if (c == '*') {
                    // ** — переключить bold (только вне code block)
                    if (!s->in_code_block && !s->in_inline_code) {
                        s->in_bold = !s->in_bold;
                    } else {
                        emit_char(s, '*');
                        emit_char(s, '*');
                    }
                    continue;
                }
                // одиночная * — просто вывести
                emit_char(s, '*');
                // текущий символ c обработать как обычный (fall through)
            }

            // fall through: обработать текущий символ c
        }

        // внутри code block — всё как есть (кроме закрывающих ```)
        if (s->in_code_block) {
            if (c == '`') {
                // может быть начало ```
                if (i + 2 < len && text[i + 1] == '`' && text[i + 2] == '`') {
                    s->in_code_block = 0;
                    i += 2; // пропустить ```
                    // пропустить остаток строки
                    while (i + 1 < len && text[i + 1] != '\n') {
                        i++;
                    }
                    continue;
                }
                // если на границе чанка — отложить
                if (i + 2 >= len) {
                    s->pending = '`';
                    continue;
                }
            }
            emit_char(s, c);
            continue;
        }

        // начало строки — проверить заголовки и буллеты
        if (s->line_start && c == '#') {
            // заголовок: # текст
            if (s->color_enabled) {
                printf(ANSI_BOLD ANSI_MAGENTA);
            }
            putchar('#');
            // вывести до конца строки
            i++;
            while (i < len && text[i] != '\n') {
                putchar(text[i]);
                i++;
            }
            if (s->color_enabled) {
                printf(ANSI_RESET);
            }
            if (i < len) {
                putchar('\n');
                s->line_start = 1;
            } else {
                s->line_start = 0;
            }
            continue;
        }

        if (s->line_start && c == '-' && i + 1 < len && text[i + 1] == ' ') {
            // буллет: - текст
            if (s->color_enabled) {
                printf(ANSI_YELLOW);
            }
            putchar('-');
            if (s->color_enabled) {
                printf(ANSI_RESET);
            }
            s->line_start = 0;
            continue;
        }

        // backtick — может быть начало ``` или `inline`
        if (c == '`') {
            s->pending = '`';
            continue;
        }

        // звёздочка — может быть **bold**
        if (c == '*') {
            s->pending = '*';
            continue;
        }

        // обычный символ
        emit_char(s, c);
    }
}

void term_print_end(term_state_t *s) {
    // сбросить pending
    if (s->pending) {
        putchar(s->pending);
        s->pending = 0;
    }

    // закрыть открытые стили
    if (s->color_enabled) {
        printf(ANSI_RESET);
    }

    // сбросить состояние
    s->in_code_block = 0;
    s->in_inline_code = 0;
    s->in_bold = 0;
    s->line_start = 1;
}
