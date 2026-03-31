// claude-c — полноценный AI CLI клиент на чистом C
// точка входа

#include <stdio.h>
#include <stdlib.h>

#include "config.h"
#include "http.h"
#include "api.h"
#include "api_openai.h"

// ---------- callbacks для стриминга ----------

// вывод текста (content или reasoning)
static void on_text(const char *text, int is_reasoning, void *userdata) {
    (void)is_reasoning;
    (void)userdata;
    printf("%s", text);
    fflush(stdout);
}

// завершение ответа
static void on_done(void *userdata) {
    (void)userdata;
    printf("\n");
}

// ---------- main ----------

int main(int argc, char *argv[]) {
    config_t cfg;

    // загрузка конфига: дефолты -> файл -> env -> CLI
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

        // формируем сообщения
        message_list_t messages;
        message_list_init(&messages);
        message_list_add(&messages, MSG_ROLE_USER, "say hello in one word");

        // отправляем запрос
        printf("-> POST %s/v1/chat/completions\n", cfg.endpoint);
        printf("-> модель: %s\n\n", cfg.model);

        int result = api_openai_chat(&cfg, &messages, on_text, on_done, NULL);
        if (result != 0) {
            fprintf(stderr, "\nОшибка запроса к API\n");
        }

        message_list_free(&messages);
        http_cleanup();
    }

    return 0;
}
