// config.c — загрузка конфигурации: дефолты, файл, env, CLI

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <ctype.h>

// версия (одно место на весь проект)
#define CLAUDE_C_VERSION "0.1.0"

// дефолты
#define DEFAULT_ENDPOINT    "http://localhost:11434"
#define DEFAULT_MODEL       "qwen3.5:9b"
#define DEFAULT_MAX_TOKENS  4096
#define DEFAULT_TEMPERATURE -1.0

// ---------- config_init ----------

void config_init(config_t *cfg) {
    memset(cfg, 0, sizeof(*cfg));
    snprintf(cfg->endpoint, sizeof(cfg->endpoint), "%s", DEFAULT_ENDPOINT);
    snprintf(cfg->model, sizeof(cfg->model), "%s", DEFAULT_MODEL);
    cfg->api_key[0] = '\0';
    cfg->api_type = API_TYPE_OPENAI;
    cfg->max_tokens = DEFAULT_MAX_TOKENS;
    cfg->temperature = DEFAULT_TEMPERATURE;
    cfg->programmatic = 0;
    cfg->prompt_text[0] = '\0';
}

// ---------- config_load_file ----------

// обрезать пробелы в начале и конце строки (in-place, возвращает указатель)
static char *trim(char *s) {
    while (isspace((unsigned char)*s)) s++;
    if (*s == '\0') return s;
    char *end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end)) *end-- = '\0';
    return s;
}

// распарсить api_type из строки
static api_type_t parse_api_type(const char *s) {
    if (strcasecmp(s, "anthropic") == 0) return API_TYPE_ANTHROPIC;
    return API_TYPE_OPENAI;
}

int config_load_file(config_t *cfg) {
    const char *home = getenv("HOME");
    if (!home) return -1;

    char path[1024];
    snprintf(path, sizeof(path), "%s/.claude-c/config", home);

    FILE *f = fopen(path, "r");
    if (!f) return -1;

    char line[1024];
    while (fgets(line, sizeof(line), f)) {
        char *p = trim(line);

        // пропустить пустые строки и комментарии
        if (*p == '\0' || *p == '#') continue;

        // найти разделитель '='
        char *eq = strchr(p, '=');
        if (!eq) continue;

        *eq = '\0';
        char *key = trim(p);
        char *val = trim(eq + 1);

        if (strcmp(key, "endpoint") == 0) {
            snprintf(cfg->endpoint, sizeof(cfg->endpoint), "%s", val);
        } else if (strcmp(key, "model") == 0) {
            snprintf(cfg->model, sizeof(cfg->model), "%s", val);
        } else if (strcmp(key, "api_key") == 0) {
            snprintf(cfg->api_key, sizeof(cfg->api_key), "%s", val);
        } else if (strcmp(key, "api_type") == 0) {
            cfg->api_type = parse_api_type(val);
        } else if (strcmp(key, "max_tokens") == 0) {
            cfg->max_tokens = atoi(val);
        } else if (strcmp(key, "temperature") == 0) {
            cfg->temperature = atof(val);
        }
    }

    fclose(f);
    return 0;
}

// ---------- config_load_env ----------

void config_load_env(config_t *cfg) {
    const char *v;

    if ((v = getenv("CLAUDE_C_ENDPOINT")) != NULL) {
        snprintf(cfg->endpoint, sizeof(cfg->endpoint), "%s", v);
    }
    if ((v = getenv("CLAUDE_C_MODEL")) != NULL) {
        snprintf(cfg->model, sizeof(cfg->model), "%s", v);
    }
    if ((v = getenv("CLAUDE_C_API_KEY")) != NULL) {
        snprintf(cfg->api_key, sizeof(cfg->api_key), "%s", v);
    }
    if ((v = getenv("CLAUDE_C_API_TYPE")) != NULL) {
        cfg->api_type = parse_api_type(v);
    }

    // ANTHROPIC_API_KEY — если api_key ещё пуст, использовать + переключить тип
    if (cfg->api_key[0] == '\0') {
        if ((v = getenv("ANTHROPIC_API_KEY")) != NULL) {
            snprintf(cfg->api_key, sizeof(cfg->api_key), "%s", v);
            cfg->api_type = API_TYPE_ANTHROPIC;
        }
    }
}

// ---------- config_parse_args ----------

static void print_usage(void) {
    printf(
        "claude-c v%s — AI CLI клиент на чистом C\n"
        "\n"
        "Использование: claude-c [опции]\n"
        "\n"
        "Опции:\n"
        "  --endpoint URL      URL API сервера (дефолт: %s)\n"
        "  --model NAME        имя модели (дефолт: %s)\n"
        "  --api-key KEY       API ключ\n"
        "  --api-type TYPE     тип API: openai или anthropic (дефолт: openai)\n"
        "  --max-tokens N      лимит токенов (дефолт: %d)\n"
        "  --temperature T     температура (-1 = дефолт сервера)\n"
        "  -p \"текст\"          programmatic режим (один запрос → stdout)\n"
        "  --help              показать справку\n"
        "  --version           показать версию\n"
        "\n"
        "Env vars:\n"
        "  CLAUDE_C_ENDPOINT   URL API сервера\n"
        "  CLAUDE_C_MODEL      имя модели\n"
        "  CLAUDE_C_API_KEY    API ключ\n"
        "  CLAUDE_C_API_TYPE   тип API\n"
        "  ANTHROPIC_API_KEY   ключ Anthropic (авто-переключает api_type)\n"
        "\n"
        "Конфиг файл: ~/.claude-c/config (формат: key=value)\n",
        CLAUDE_C_VERSION,
        DEFAULT_ENDPOINT,
        DEFAULT_MODEL,
        DEFAULT_MAX_TOKENS
    );
}

static void print_version(void) {
    printf("claude-c v%s\n", CLAUDE_C_VERSION);
}

int config_parse_args(config_t *cfg, int argc, char **argv) {
    static struct option long_opts[] = {
        {"endpoint",    required_argument, NULL, 'e'},
        {"model",       required_argument, NULL, 'm'},
        {"api-key",     required_argument, NULL, 'k'},
        {"api-type",    required_argument, NULL, 't'},
        {"max-tokens",  required_argument, NULL, 'n'},
        {"temperature", required_argument, NULL, 'T'},
        {"help",        no_argument,       NULL, 'h'},
        {"version",     no_argument,       NULL, 'v'},
        {NULL, 0, NULL, 0}
    };

    // сбросить getopt
    optind = 1;

    int c;
    while ((c = getopt_long(argc, argv, "p:", long_opts, NULL)) != -1) {
        switch (c) {
        case 'e':
            snprintf(cfg->endpoint, sizeof(cfg->endpoint), "%s", optarg);
            break;
        case 'm':
            snprintf(cfg->model, sizeof(cfg->model), "%s", optarg);
            break;
        case 'k':
            snprintf(cfg->api_key, sizeof(cfg->api_key), "%s", optarg);
            break;
        case 't':
            cfg->api_type = parse_api_type(optarg);
            break;
        case 'n':
            cfg->max_tokens = atoi(optarg);
            break;
        case 'T':
            cfg->temperature = atof(optarg);
            break;
        case 'p':
            cfg->programmatic = 1;
            snprintf(cfg->prompt_text, sizeof(cfg->prompt_text), "%s", optarg);
            break;
        case 'h':
            print_usage();
            return 1;
        case 'v':
            print_version();
            return 1;
        default:
            fprintf(stderr, "Используйте --help для справки\n");
            return -1;
        }
    }

    return 0;
}

// ---------- config_dump ----------

static const char *api_type_str(api_type_t t) {
    switch (t) {
    case API_TYPE_ANTHROPIC: return "anthropic";
    case API_TYPE_OPENAI:    return "openai";
    }
    return "unknown";
}

void config_dump(const config_t *cfg) {
    printf("=== Конфигурация ===\n");
    printf("  endpoint:    %s\n", cfg->endpoint);
    printf("  model:       %s\n", cfg->model);
    printf("  api_key:     %s\n", cfg->api_key[0] ? "***" : "(не задан)");
    printf("  api_type:    %s\n", api_type_str(cfg->api_type));
    printf("  max_tokens:  %d\n", cfg->max_tokens);
    printf("  temperature: ");
    if (cfg->temperature < 0) {
        printf("(дефолт сервера)\n");
    } else {
        printf("%.2f\n", cfg->temperature);
    }
    printf("  programmatic: %s\n", cfg->programmatic ? "да" : "нет");
}
