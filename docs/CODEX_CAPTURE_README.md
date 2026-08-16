# codex_capture

PTY-based output capture tool for OpenAI Codex. Spawns an interactive pseudo-terminal and captures full output (including ANSI codes) to a file, bypassing "stdin is not a terminal" errors that block automation.

## Usage

```bash
codex_capture <output_file>
```

Runs `codex result` inside a PTY and writes all output (stdin, stdout, stderr) to `<output_file>`.

### Example

```bash
# Capture Codex interactive session to a file
codex_capture /tmp/codex_output.txt

# Later, read the captured output
cat /tmp/codex_output.txt
```

## Installation

The binary is pre-compiled at `/root/bin/codex_capture`. To rebuild:

```bash
gcc -o /root/bin/codex_capture /root/bin/codex_capture.c -lutil
chmod +x /root/bin/codex_capture
```

## Requirements

- **Codex CLI:** v0.147.0 or later (must be in `$PATH`)
- **Codex auth:** `~/.codex/config.json` must be valid
- **Build:** `gcc`, `pty.h`, `libutil` (on some systems: `libutil-dev`)
- **Platform:** Linux (uses `openpty()` from `pty.h`)

## Pre-use checklist

Before calling `codex_capture`:

1. **Verify Codex is installed:** `codex --version` should print a version.
2. **Check auth:** `codex login` or verify `~/.codex/auth.json` exists with valid credentials.
3. **Check output directory:** Ensure the parent directory of `<output_file>` is writable.
4. **Check permissions:** `codex_capture` binary must be executable (`chmod +x /root/bin/codex_capture`).

## Limitations

- **ANSI codes in output:** Full TTY output including escape sequences is captured; strip them with `sed` if needed:
  ```bash
  sed 's/\x1b\[[0-9;]*m//g' /tmp/codex_output.txt
  ```

- **Session persistence:** Each call to `codex_capture` starts a new `codex result` session. Use Codex CLI's native session APIs (`codex resume <SESSION_ID>`, etc.) for multi-turn persistence.

- **Input automation:** This tool captures output of an interactive session; it does not automatically provide input. For automated input, pair with `expect` or pipe commands to the PTY:
  ```bash
  (echo "command1"; echo "command2") | codex_capture /tmp/output.txt
  ```

## Troubleshooting

### "error: exec: openpty: not available"

- **Cause:** Missing `libutil` development headers.
- **Fix (Ubuntu/Debian):** `apt-get install libutil-dev && gcc -o /root/bin/codex_capture /root/bin/codex_capture.c -lutil`
- **Fix (CentOS/RHEL):** `yum install util-linux-devel && gcc -o /root/bin/codex_capture /root/bin/codex_capture.c -lutil`

### "codex: command not found"

- **Cause:** Codex CLI not in `$PATH` or not installed.
- **Fix:** Install Codex (`npm install -g @openai/codex`) or verify installation: `which codex`.

### "stdin is not a terminal" (when not using codex_capture)

- **Cause:** Running `codex result` directly in a non-interactive shell.
- **Fix:** Use `codex_capture <output_file>` to spawn a PTY for Codex.

## Integration with Claude Code

The Codex Session Controller skill (`/root/.claude/skills/cowork/SKILL.md`) uses `codex_capture` to:

1. Capture Codex turn results durably to disk.
2. Parse and forward output to Claude for review.
3. Enable async automation of interactive Codex workflows.

Example Skill usage:

```bash
# Skill internally calls:
codex_capture /tmp/codex_turn_output.txt

# Then reads and parses the file:
cat /tmp/codex_turn_output.txt | grep -i "error\|success"
```
