# claude-c

Full-featured AI CLI client in pure C. One binary, runs anywhere.

Think [Claude Code](https://claude.ai/claude-code), but compiled to a single ~300KB binary that runs on anything from an Alpine container to a router.

## Features

- **Pure C** — compiles everywhere, forks into anything
- **Universal API** — Anthropic + any OpenAI-compatible server (Ollama, vLLM, LM Studio)
- **Full tool use** — Read, Write, Edit, Bash, Grep, Glob, MCP, sub-agents
- **Interactive + Programmatic** — REPL chat or `claude-c -p "task"` for scripts
- **Streaming** — real-time token-by-token output via SSE
- **Tiny** — ~300KB static binary, <5ms startup, 2-4MB RAM

## Quick Start

```bash
# Build
make

# Set API key
export ANTHROPIC_API_KEY=sk-ant-...

# Interactive chat
./claude-c

# Programmatic mode (for scripts)
./claude-c -p "explain this Makefile" < Makefile

# Use with local model (Ollama)
./claude-c --endpoint http://localhost:11434 --model llama3
```

## Build Requirements

- C11 compiler (gcc, clang)
- libcurl (with TLS)
- readline or libedit (optional)

```bash
# Debian/Ubuntu
apt install build-essential libcurl4-openssl-dev libreadline-dev

# macOS (Xcode CLT has everything)
xcode-select --install

# Alpine
apk add build-base curl-dev readline-dev
```

## Configuration

Priority: CLI flags > environment > config file

```bash
# Environment
export ANTHROPIC_API_KEY=sk-ant-...
export CLAUDE_C_ENDPOINT=https://api.anthropic.com
export CLAUDE_C_MODEL=claude-sonnet-4-20250514

# Config file (~/.claude-c/config)
endpoint=https://api.anthropic.com
model=claude-sonnet-4-20250514
api_type=anthropic
```

## License

MIT
