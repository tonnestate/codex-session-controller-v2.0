# Session Controller for OpenAI Codex

An independent supervisor specification and Linux helper for maintaining controlled, persistent Codex coding sessions from Claude Code or another orchestration environment.

The project focuses on explicit session identity, bounded delegation, live steering, approval handling, single-writer coordination, recovery after interruption, and independent validation of the resulting repository changes.

> **Project status:** v2.0 operational beta. Review the integration and security notes before using it on production repositories.

## What this repository contains

| Component | Purpose |
| --- | --- |
| `SKILL.md` | The authoritative supervisor workflow for starting, resuming, steering, interrupting, reviewing, and validating Codex work. |
| `src/codex_capture.c` | A small Linux PTY helper that runs the configured `codex result` command and captures its terminal output to a file. |
| `docs/CODEX_CAPTURE_README.md` | Additional build and runtime notes for the PTY helper. |
| `docs/TESTING.md` | Suggested checks for the controller and helper. |
| `FEEDBACK.md` | Review history and design considerations. |

The C helper is a transport utility only. It does **not** implement the Codex app-server protocol, persist thread state, grant approvals, or validate repository changes. Those responsibilities remain with the supervisor and the control surface described in `SKILL.md`.

## Core guarantees

When the configured Codex bridge exposes the required operations, the controller is designed to provide:

- exact `threadId`, `sessionId`, and `turnId` tracking from tool responses;
- continuation of the intended thread instead of an ambiguous “last session”;
- live steering and interruption of the active turn;
- approval decisions bound to the relevant thread, turn, and item;
- a single-writer rule for overlapping repository paths;
- independent inspection of diffs, tests, builds, and other acceptance checks;
- explicit degraded-mode reporting when persistent control is unavailable.

It does not transfer hidden Claude context automatically. Every handoff must state the objective, authorized scope, constraints, evidence, acceptance checks, and expected return format.

## Requirements

- Linux for `codex_capture` (`openpty()` and `libutil`)
- OpenAI Codex CLI installed and authenticated by the local user
- A Codex version exposing the app-server methods required by `SKILL.md`
- Claude Code or another supervisor capable of using the configured bridge
- GCC and the PTY development headers to build the optional helper
- A working `codex result` command or local compatibility command if `codex_capture` is used

Codex authentication remains local. Do not copy authentication files, API keys, or session credentials into this repository, prompts, or capture files.

## Installation

```bash
git clone https://github.com/TonnEstate/codex-session-controller.git
cd codex-session-controller

mkdir -p "$HOME/.claude/skills/codex-session-controller"
cp SKILL.md "$HOME/.claude/skills/codex-session-controller/SKILL.md"

gcc -Wall -Wextra -Wpedantic -O2 \
  -o codex_capture src/codex_capture.c -lutil
install -m 0755 codex_capture "$HOME/.local/bin/codex_capture"
```

Confirm that the required commands are available before the first controlled session:

```bash
codex --version
codex app-server --help
codex result
command -v codex_capture
```

If `codex result` is not provided by your Codex installation or local integration, the PTY helper cannot be used unchanged. The app-server control workflow in `SKILL.md` remains the preferred integration path.

## Control model

The supervisor communicates with the documented Codex app server over JSON-RPC. The important operations include:

| Operation | Purpose |
| --- | --- |
| `thread/start` | Create a new persistent Codex thread. |
| `thread/resume` | Resume an exact recorded thread. |
| `thread/read` / `thread/list` | Inspect or safely resolve stored thread state. |
| `thread/goal/set` | Persist the high-level objective when supported. |
| `turn/start` | Begin one turn on the selected thread. |
| `turn/steer` | Add guidance to the active turn using its expected ID. |
| `turn/interrupt` | Request cancellation of the active turn. |
| `review/start` | Run an inline or detached Codex review. |
| streamed events | Observe turn, item, approval, and completion state. |

The supervisor must discover the available methods at runtime. It must not claim steering, interruption, review, or persistence when the corresponding operation is unavailable.

## Controlled workflow

1. Resolve the exact repository, working directory, thread, and task.
2. Record the Git baseline, existing dirty files, permissions, and authorized write scope.
3. Establish exclusive write ownership for every overlapping path.
4. Resume the recorded thread or explicitly start a new one.
5. Send a bounded handoff contract and start one turn.
6. Stream events and handle approvals or user questions explicitly.
7. Steer or interrupt only against the exact active turn.
8. Inspect the complete diff and run the agreed acceptance checks independently.
9. Report verified results, blockers, residual risks, and final thread status.

Never begin a second turn on the same thread while another turn is active.

## Validation gate

Codex self-report is not completion evidence. Use checks appropriate to the repository, for example:

```bash
git status --short
git diff --stat
git diff --check
python3 -m pytest
```

Before releasing the task, confirm that only authorized paths changed, pre-existing work remains intact, secrets were not added, and every claimed check actually ran.

## PTY capture helper

```bash
codex_capture /tmp/codex-output.txt
```

The current helper creates or truncates the selected output file with owner-only mode `0600`, starts `codex result` inside a pseudo-terminal, and copies the PTY output into that file.

Important limitations:

- captured data may include ANSI sequences, repository content, prompts, or secrets printed by child processes;
- the helper captures output but does not implement a general interactive input channel;
- the output path must be trusted and controlled by the invoking user;
- the current implementation does not reject a pre-existing symbolic link at the output path;
- a crash or forced termination may leave an incomplete capture;
- the helper is Linux-specific and should be rebuilt locally rather than distributed as an opaque binary.

Treat capture files as sensitive temporary artifacts. Store them outside the repository, restrict access, and remove them when they are no longer needed.

## Recovery rules

| Situation | Required response |
| --- | --- |
| Event stream disconnects | Reconnect to the same recorded thread. |
| Resume fails | Preserve the identifier and error; do not silently create a replacement thread. |
| Approval is unresolved | Wait for an explicit decision; never guess. |
| Codex leaves the authorized scope | Interrupt the turn and inspect partial changes. |
| Validation fails | Return the concrete failure to the same thread for correction. |
| Persistent control is unavailable | Declare degraded mode before using a one-off execution path. |

## Security boundary

This project does not bypass Codex authentication, sandboxing, approval policy, usage limits, or repository permissions. Every user must authenticate through their own supported Codex configuration and remains responsible for the commands, network access, and file changes they authorize.

Do not expose an unauthenticated app-server listener to a network. For remote use, follow the current OpenAI documentation for TLS, authentication, and supported transports.

## Background and related work

This independent project was inspired by the public OpenAI Developer Community announcement [“Introducing Codex Plugin for Claude Code”](https://community.openai.com/t/introducing-codex-plugin-for-claude-code/1378186).

For the official implementation and current protocol documentation, see:

- [OpenAI Codex plugin for Claude Code](https://github.com/openai/codex-plugin-cc)
- [OpenAI Codex app-server documentation](https://developers.openai.com/codex/app-server)
- [OpenAI Codex SDK documentation](https://developers.openai.com/codex/codex-sdk)

## Maintainer

Developed and maintained by [TonnEstate](https://tonnestate.de/).

TonnEstate is an independent project and is not affiliated with, sponsored by, or endorsed by OpenAI or Anthropic.

## Trademarks

OpenAI and Codex are trademarks of OpenAI. Anthropic and Claude are trademarks of Anthropic. All trademarks belong to their respective owners and are used only to describe compatibility. No OpenAI or Anthropic logos are included.

## License

The original material in this repository is licensed under the [MIT License](LICENSE). Third-party software and referenced projects remain subject to their own licenses.

Before publishing, confirm that any material copied or adapted from another repository retains all notices required by its original license.
