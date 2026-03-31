// claude-c — полноценный AI CLI клиент на чистом C
// точка входа

#include <stdio.h>
#include <stdlib.h>

#include "config.h"
#include "chat.h"

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

    if (cfg.programmatic) {
        return chat_oneshot(&cfg, cfg.prompt_text);
    }

    // интерактивный REPL
    return chat_repl(&cfg);
}
