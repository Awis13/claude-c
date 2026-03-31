// chat.c — интерактивный REPL: readline, история, стриминг ответов

#include "chat.h"
#include "http.h"
#include "api_openai.h"
#include "api_anthropic.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

// readline/libedit — macOS поставляет libedit под видом readline
#include <editline/readline.h>

// ---------- строковый буфер для сбора ответа ----------

typedef struct {
    char *buf;
    size_t len;
    size_t cap;
} strbuf_t;

static void strbuf_init(strbuf_t *sb) {
    sb->cap = 256;
    sb->buf = malloc(sb->cap);
    sb->buf[0] = '\0';
    sb->len = 0;
}

static void strbuf_append(strbuf_t *sb, const char *text) {
    size_t tlen = strlen(text);
    while (sb->len + tlen + 1 > sb->cap) {
        sb->cap *= 2;
        sb->buf = realloc(sb->buf, sb->cap);
    }
    memcpy(sb->buf + sb->len, text, tlen);
    sb->len += tlen;
    sb->buf[sb->len] = '\0';
}

static void strbuf_free(strbuf_t *sb) {
    free(sb->buf);
    sb->buf = NULL;
    sb->len = 0;
    sb->cap = 0;
}

// ---------- Ctrl+C: не убивать процесс ----------

static volatile sig_atomic_t g_interrupted = 0;

static void sigint_handler(int sig) {
    (void)sig;
    g_interrupted = 1;
}

// ---------- callbacks для стриминга ----------

// userdata для callbacks — strbuf для сбора ответа
typedef struct {
    strbuf_t *sb;
} chat_cb_data_t;

// вывод текста + сбор в буфер
static void on_text(const char *text, int is_reasoning, void *userdata) {
    (void)is_reasoning;
    chat_cb_data_t *data = (chat_cb_data_t *)userdata;

    // если пользователь нажал Ctrl+C — не печатаем
    if (g_interrupted) return;

    printf("%s", text);
    fflush(stdout);
    strbuf_append(data->sb, text);
}

// завершение ответа
static void on_done(void *userdata) {
    (void)userdata;
    printf("\n\n");
    fflush(stdout);
}

// ---------- проверка команд выхода ----------

static int is_exit_cmd(const char *input) {
    return (strcmp(input, "exit") == 0 ||
            strcmp(input, "quit") == 0 ||
            strcmp(input, "/exit") == 0 ||
            strcmp(input, "/quit") == 0);
}

// ---------- REPL ----------

int chat_repl(const config_t *cfg) {
    // приветствие
    printf("claude-c v0.1.0\n");
    if (cfg->api_type == API_TYPE_ANTHROPIC) {
        printf("модель: %s @ %s (Anthropic)\n", cfg->model, cfg->endpoint);
    } else {
        printf("модель: %s @ %s\n", cfg->model, cfg->endpoint);
    }
    printf("введите сообщение (exit для выхода)\n\n");

    // инициализация
    http_init();

    message_list_t messages;
    message_list_init(&messages);

    // установить обработчик SIGINT
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sigint_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);

    int exit_code = 0;

    // основной цикл
    for (;;) {
        g_interrupted = 0;

        char *line = readline("you> ");

        // EOF (Ctrl+D) → выход
        if (!line) {
            printf("\n");
            break;
        }

        // пустой ввод → пропустить
        if (line[0] == '\0') {
            free(line);
            continue;
        }

        // команды выхода
        if (is_exit_cmd(line)) {
            free(line);
            break;
        }

        // добавить в историю readline (для стрелок вверх/вниз)
        add_history(line);

        // добавить user message в историю разговора
        message_list_add(&messages, MSG_ROLE_USER, line);
        free(line);

        // подготовить буфер для сбора ответа
        strbuf_t sb;
        strbuf_init(&sb);

        chat_cb_data_t cb_data = { .sb = &sb };

        // вывести маркер ассистента
        printf("\033[1;32massistant>\033[0m ");
        fflush(stdout);

        // отправить запрос
        g_interrupted = 0;
        int result;
        if (cfg->api_type == API_TYPE_ANTHROPIC) {
            result = api_anthropic_chat(cfg, &messages, on_text, on_done, &cb_data);
        } else {
            result = api_openai_chat(cfg, &messages, on_text, on_done, &cb_data);
        }

        if (result != 0 && !g_interrupted) {
            fprintf(stderr, "\n\033[1;31mошибка запроса к API\033[0m\n\n");
        }

        // сохранить ответ ассистента в историю (если что-то получили)
        if (sb.len > 0) {
            message_list_add(&messages, MSG_ROLE_ASSISTANT, sb.buf);
        }

        strbuf_free(&sb);

        // если был Ctrl+C во время стриминга — печатаем перенос и продолжаем
        if (g_interrupted) {
            printf("\n\n");
            g_interrupted = 0;
        }
    }

    // очистка
    message_list_free(&messages);
    http_cleanup();

    return exit_code;
}
