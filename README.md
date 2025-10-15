# Agent-C

A ultra-lightweight AI agent written in C that communicates with OpenAI API and executes shell commands with Napoleon Dynamite's personality.

![Agent-C Preview](preview.webp)

## Features

- **Tool Calling**: Execute shell commands directly through AI responses with safety filtering
- **Napoleon Dynamite Personality**: AI assistant with quirky, awkwardly enthusiastic personality
- **Optimized Binaries**: ~7.9KB on macOS (GZEXE), ~16KB on Linux (UPX)
- **Conversation Memory**: Sliding window memory management (20 messages max)
- **Cross-Platform**: macOS and Linux
- **Command Safety**: Automatic filtering of dangerous shell characters
- **Multi-step Task Support**: Chain commands with `&&` operator

## Quick Start

### Prerequisites

- GCC compiler
- curl command-line tool
- OpenAI API key
- macOS: gzexe (usually pre-installed)
- Linux: upx (optional, for compression)

### Build

```bash
make
```

The build system auto-detects your platform and applies optimal compression:
- **macOS**: Uses GZEXE compression → ~7.9KB binary
- **Linux**: Uses UPX compression → ~16KB binary

### Setup

Set your OpenAI API key and configuration:

```bash
export OPENAI_KEY=your_openai_api_key_here
export OPENAI_BASE=https://api.openai.com/v1  # Optional, defaults to OpenAI
export OPENAI_MODEL=gpt-3.5-turbo             # Optional, defaults to configured model
```

### Run

```bash
./agent-c
```

## Configuration

The agent uses the following environment variables:

- `OPENAI_KEY`: Your OpenAI API key (required)
- `OPENAI_BASE`: API base URL (optional, defaults to OpenAI API)
- `OPENAI_MODEL`: Model name (optional, defaults to configured model)

## Usage Examples

Start the agent and interact with it:

```
Agent> Hello
Gosh! Hello there! Sweet!
```

The agent can execute shell commands:

```
Agent> Create a file called test.txt and write "Hello World" to it
$ echo "Hello World" > test.txt
Command output:

```

Multi-step tasks are supported:

```
Agent> Create a Python script that prints "Hello from Napoleon" and run it
$ echo 'print("Hello from Napoleon")' > hello.py && python3 hello.py
Command output:
Hello from Napoleon
```

## Architecture

- **main.c**: Entry point with signal handling and configuration loading
- **agent.c**: Core agent logic, command execution, and personality handling
- **cli.c**: Command-line interface with colored prompts
- **json.c**: JSON parsing and request/response handling
- **utils.c**: HTTP requests and configuration management
- **agent-c.h**: Header with data structures and function declarations

## License

**CC0 - "No Rights Reserved"**