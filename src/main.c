// claude-c — полноценный AI CLI клиент на чистом C
// точка входа

#include <stdio.h>
#include <stdlib.h>

#include "config.h"

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

    // пока без REPL — просто показать конфиг
    config_dump(&cfg);

    return 0;
}
