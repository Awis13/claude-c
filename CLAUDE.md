# claude-c

Полноценный AI CLI клиент на чистом C. 1:1 порт функционала Claude Code.
Один бинарник, работает на любом железе, от Alpine-контейнера до роутера.

## Сборка

```bash
make          # собрать
make clean    # очистить
make install  # установить в /usr/local/bin
```

## Зависимости

- **libcurl** — HTTP/TLS (системная, линкуется динамически)
- **cJSON** — JSON парсинг (vendored в deps/cJSON/)
- **readline** или **libedit** — ввод с историей (опционально, fallback на свой)

## Структура

```
src/
  main.c          — точка входа, CLI, REPL цикл
  config.c/h      — конфиг: endpoint, model, api_key, приоритеты
  http.c/h        — HTTP клиент (libcurl), SSE стриминг
  stream.c/h      — SSE парсер (data: lines → токены)
  api_anthropic.c/h — Anthropic Messages API
  api_openai.c/h  — OpenAI-compatible API (Ollama, vLLM, etc.)
  chat.c/h        — conversation loop, message history, tool dispatch
  tools.c/h       — tool registry + реализации
  permissions.c/h — система разрешений (allow/deny/ask)
  session.c/h     — JSONL сессии, save/load/resume
  mcp.c/h         — MCP клиент (JSON-RPC over stdio)
  terminal.c/h    — ANSI вывод, markdown rendering
  tui.c/h         — TUI движок (framebuffer, layout, input)
  arena.c/h       — arena allocator
  util.c/h        — строки, файлы, мелочи
deps/
  cJSON/          — vendored cJSON
```

## Правила

- **Чистый C** — C11, без C++, без расширений компилятора
- **Библиотеки ок** — libcurl, cJSON, readline. Но минимально.
- **Один бинарник** — статическая линковка для релизов
- **Кросс-платформа** — Linux (glibc + musl), macOS, FreeBSD
- **Arena allocator** — для per-request памяти, counted strings
- **Комментарии на русском** — код на английском

## API поддержка

Два типа API, автодетект или явный `--api-type`:
- `anthropic` — Anthropic Messages API (x-api-key)
- `openai` — OpenAI-compatible (Bearer token) — Ollama, vLLM, LM Studio, любой сервер

## Конфиг (приоритет)

1. CLI флаги (`--endpoint`, `--model`, `--api-key`)
2. Env (`ANTHROPIC_API_KEY`, `CLAUDE_C_ENDPOINT`, `CLAUDE_C_MODEL`)
3. Файл (`~/.claude-c/config`)

## Режимы работы

- **Интерактивный** — `claude-c` → REPL чат
- **Programmatic** — `claude-c -p "задача"` → результат в stdout
- **Pipe** — `cat file.py | claude-c -p "review this"`

## Канбан

Obsidian → `Projects/claude-c/TODO.md`
