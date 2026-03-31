// config.h — конфигурация: структура, загрузка, приоритеты
// приоритет: CLI > env > file > дефолты

#ifndef CONFIG_H
#define CONFIG_H

typedef enum {
    API_TYPE_OPENAI,    // по умолчанию — для Ollama и совместимых
    API_TYPE_ANTHROPIC
} api_type_t;

typedef struct {
    char endpoint[512];     // URL API сервера
    char model[128];        // имя модели
    char api_key[256];      // API ключ
    api_type_t api_type;    // тип API
    int max_tokens;         // лимит токенов (0 = дефолт сервера)
    double temperature;     // температура (-1 = дефолт сервера)
    int programmatic;       // -p режим
    char prompt_text[8192]; // текст для -p режима
} config_t;

// заполнить дефолтами
void config_init(config_t *cfg);

// прочитать ~/.claude-c/config (0 = ок, -1 = нет файла/ошибка)
int config_load_file(config_t *cfg);

// прочитать env vars
void config_load_env(config_t *cfg);

// парсинг CLI (0 = ок, 1 = выход (help/version), -1 = ошибка)
int config_parse_args(config_t *cfg, int argc, char **argv);

// вывести текущий конфиг (для отладки)
void config_dump(const config_t *cfg);

#endif // CONFIG_H
