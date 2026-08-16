---
name: codex-session-controller
description: Control a persistent OpenAI Codex coding session from Claude Code (or similar supervisor). Use when the supervisor must start, resume, take over, steer, interrupt, delegate work to, or request an independent review from Codex while preserving the exact thread, repository state, permissions, and verification trail. Prefer this skill over ad-hoc shell polling or transcript scraping.
version: 2.0
---

# Codex Session Controller (v2)

Operate Codex as a persistent coding agent under explicit supervision. Preserve the exact Codex thread and shared workspace. Never imply that the supervisor’s hidden context is automatically available to Codex.

## Required control surface

Map the configured Codex app-server bridge to these canonical operations:

| Operation              | Purpose                                      | Required for          |
|------------------------|----------------------------------------------|-----------------------|
| `thread/start`         | Create new thread                            | Full control          |
| `thread/resume`        | Resume exact existing thread                 | Full control          |
| `thread/read`          | Inspect thread state / history               | Recovery              |
| `thread/list`          | Discover candidate threads                   | Safe resume           |
| `thread/fork`          | Branch a thread (rare)                       | Advanced              |
| `thread/goal/set`      | Set / update high-level goal                 | Preferred             |
| `turn/start`           | Begin a new turn                             | Full control          |
| `turn/steer`           | Inject guidance into active turn             | Live steering         |
| `turn/interrupt`       | Stop active turn                             | Live control          |
| `review/start`         | Detached read-only review                    | Independent review    |
| Streamed turn/item events | Real-time progress                        | Monitoring            |
| Approval request/response | Permission handling                       | Safety                |

**Pre-flight capability check** (run once per session or after bridge restart):

1. Attempt a lightweight discovery call (or inspect bridge metadata).
2. Record which of the above operations are actually available.
3. If `thread/start` or `thread/resume` **and** `turn/start` are missing → stop and report the gap. Do not simulate control.
4. If `turn/steer` or `turn/interrupt` are missing → you may still start/resume, but you must not claim live steering capability.
5. Never fall back to `codex exec --last`, transcript scraping, or direct reads from Codex session storage as a substitute for the control surface.

## State model

Track these values **only** from tool responses (never invent or derive them):

- `threadId` – exact Codex thread being controlled (opaque)
- `sessionId` – root session identifier (opaque, never infer from threadId)
- `turnId` – active or most recently completed turn
- `threadStatus` – `notLoaded` | `idle` | `active` | error state
- `turnStatus` – `inProgress` | `completed` | `failed` | `interrupted` | reported equivalent
- `cwd` – canonical absolute working directory
- `repoRoot` – canonical Git repository root
- `branch` + `head` – branch name and baseline commit
- `dirtyBaseline` – pre-existing modified / staged / untracked paths
- `objective` – current user-approved outcome
- `scope` – owned files/directories or read-only review target
- `permissions` – active sandbox, approval policy, writable roots, network policy
- `pendingApproval` – approval request tied to exact thread + turn + item
- `acceptanceChecks` – commands or observations required before completion
- `bridgeCapabilities` – set of available operations discovered at pre-flight

Treat every identifier as opaque. Never reuse an identifier from another repository or task.

## Control loop

1. Classify the request: `start` | `resume` | `takeover` | `delegate` | `steer` | `interrupt` | `review` | `status` | `release`.
2. Resolve exact repository, working directory, thread, and active turn.
3. Capture Git + permission baseline **without modifying the workspace**.
4. Establish exclusive write ownership for the requested scope (or confirm read-only).
5. Build a compact handoff containing **only** state Codex cannot recover from the workspace or its own thread.
6. Start or resume the thread, set the objective when supported, begin one turn.
7. Stream events until the turn reaches a terminal state. Process approvals and user questions explicitly.
8. Steer the active turn only when new user input or observed deviation materially changes execution.
9. Interrupt when continuing would violate scope, permissions, user intent, or repository safety.
10. Inspect the resulting diff and run the agreed checks **independently** of Codex’s self-report.
11. Report the verified outcome together with exact thread and turn status.

**Hard rule:** Never start a second turn on the same thread while another turn is still active.

## Resolve the session safely

### Start

Start a new thread only when the user requests a new Codex session **or** no prior thread exists for the task.  
Pass: canonical `cwd`, least-privileged sandbox that can complete the work, existing project instructions.  
Record `threadId` and `sessionId` immediately.

### Resume

Resume **only** an exact recorded `threadId`.  
If none is available:

1. Call `thread/list`.
2. Filter by canonical `cwd`, repository identity, task objective, and recency.
3. If more than one candidate remains → ask the user to choose.
4. Never use a global “last session” shortcut.

After resume, confirm the returned thread belongs to the intended repository and inspect its status before starting a turn.  
A successful resume preserves Codex history; it does **not** transfer the supervisor’s private reasoning or unrecorded instructions.

### Recovery after restart or compaction

1. Call `thread/resume` with the recorded `threadId`.
2. If needed, use `thread/read` to reconstruct only:
   - last confirmed objective
   - completed turn
   - pending work
   - relevant outputs
3. Do **not** replay the full supervisor transcript.
4. If resume fails → preserve the identifier and error, then ask before creating a replacement thread (a new thread changes session identity).

## Repository ownership

Enforce a single-writer rule for overlapping paths:

- **Takeover** → Codex owns the agreed write scope; supervisor remains verifier.
- **Delegation** → concurrent work only on explicitly disjoint paths.
- **Review** → Codex is read-only unless the user separately authorizes fixes.
- Before handing ownership back: wait for the turn to finish (or interrupt it), inspect the diff, record final status.

Capture `git status --short`, branch, and `HEAD` **before** Codex writes.  
Treat every pre-existing change as user-owned. Never discard, reset, overwrite, stage, or commit unrelated changes.  
If the working tree changes unexpectedly while Codex is active → pause the turn and determine whether another writer is operating.

Ownership is a coordination invariant, not proof that other processes are absent.

## Handoff contract

Send Codex one compact, explicit task envelope:

```text
OBJECTIVE
<single verifiable outcome>

SCOPE
<owned paths and allowed read-only paths>

CURRENT STATE
<confirmed implementation state, baseline commit, relevant dirty files>

AUTHORITATIVE CONSTRAINTS
<user instructions, project rules, architecture decisions, permission limits>

NON-GOALS
<work Codex must not perform>

EVIDENCE
<exact files, symbols, errors, tests, or prior turn results>

ACCEPTANCE CHECKS
<commands and observable conditions that must pass>

NEXT ACTION
<the first concrete action Codex should take>

RETURN CONTRACT
<summary, changed files, checks, blockers, residual risks, thread status>
```

Do not paste information Codex can read directly from the shared repository.  
Do not transfer speculation as fact. Label unresolved assumptions and require Codex to verify them before editing.

### Example – Takeover

```text
OBJECTIVE
Implement the missing rate-limiter middleware and wire it into the existing Express router so that /api/* endpoints are limited to 100 req/min per IP.

SCOPE
src/middleware/rateLimit.ts (create)
src/app.ts (modify – only the middleware registration)
tests/rateLimit.test.ts (create)

CURRENT STATE
Baseline: main @ a1b2c3d
No existing rate-limit code.
Express app already mounts routers under /api.

AUTHORITATIVE CONSTRAINTS
- Use the `express-rate-limit` package already in package.json.
- Keep the existing error-handling middleware order.
- Do not change any other middleware.

NON-GOALS
- Do not touch authentication or logging middleware.
- Do not add Redis or external store.

EVIDENCE
package.json already lists "express-rate-limit": "^7.x"
src/app.ts lines 34-48 show current middleware stack.

ACCEPTANCE CHECKS
- npm test -- rateLimit
- curl -I http://localhost:3000/api/health (should include rate-limit headers after start)
- git diff --stat shows only the three files above

NEXT ACTION
Create src/middleware/rateLimit.ts with a standard IP-based limiter, then register it in src/app.ts before the routers.

RETURN CONTRACT
summary + list of changed files + test output + any remaining blockers
```

### Example – Bounded Delegation

```text
OBJECTIVE
Add TypeScript types for the existing untyped `parseQuery` helper and update its three call sites.

SCOPE
src/utils/query.ts (modify)
src/routes/search.ts (modify – call sites only)
src/routes/filter.ts (modify – call sites only)

CURRENT STATE
parseQuery currently returns `any`. Call sites are in the two route files listed.

AUTHORITATIVE CONSTRAINTS
- Keep runtime behavior identical.
- Prefer exact types over `unknown` where the shape is already known from usage.

NON-GOALS
- Do not refactor the helper itself beyond adding types.
- Do not touch other utils.

ACCEPTANCE CHECKS
- tsc --noEmit
- existing unit tests for the routes still pass

NEXT ACTION
Inspect current usages, define a precise return type, apply it, and fix the call sites.
```

## Takeover

1. Stop supervisor-side edits in the transferred scope.
2. Capture repository baseline and unresolved user requirements.
3. Resume the existing Codex thread or start one exact new thread.
4. Send the complete handoff contract and set the thread goal when available.
5. Monitor streamed events; keep the user informed during long work.
6. Forward new user instructions with `turn/steer` using the exact active `turnId`.
7. Retain supervision until validation is complete. Takeover transfers execution, not accountability.

## Delegation

Delegate one bounded result with explicit ownership and acceptance checks.  
Prefer tasks Codex can complete and verify independently.  
Do not delegate an undefined “help with this” request or duplicate work the supervisor is already performing.

When Codex returns: verify the artifact and integrate only confirmed results.  
Feed corrections back through the **same** thread.

## Live steering and interruption

Use `turn/steer` only while a turn is active and always include the expected active `turnId`.  
Steer when:

- the user adds or changes a requirement;
- Codex is operating on the wrong file, layer, or objective;
- new evidence invalidates the current plan;
- an approval decision changes the available path.

Do not steer for cosmetic commentary that can wait until the next turn.

Use `turn/interrupt` when:

- the user asks to stop or replace the active task;
- Codex crosses the declared scope;
- destructive or high-risk work is proposed without authority;
- concurrent edits threaten the same paths;
- continuing would compound a known false assumption.

After interruption: wait for the terminal `interrupted` status and inspect partial changes before starting any replacement turn.

## Approvals and permissions

Preserve the user’s existing sandbox, approval policy, writable roots, and network restrictions.  
Use the least privilege sufficient for the task. Never weaken permissions merely to avoid an approval.

Tie every approval request to its exact `threadId`, `turnId`, and item.  
Show the user the concrete command, file change, destination, or permission requested.  
Never approve destructive actions, broad filesystem access, credential access, or unrestricted network access on the user’s behalf.

Do not expose, copy, or inspect Codex authentication files.  
Never place secrets in prompts, state summaries, logs, repository files, or command-line arguments.

## Independent review

Use the Codex review operation for a read-only review of uncommitted changes, a base branch, a commit, or an exact custom target.  
Prefer detached delivery when independence from the implementation thread matters.

Require findings to include: severity, evidence, affected path/symbol, user impact, and a concrete remediation.  
Triage each finding against the actual code before accepting it.  
Absence of findings is **not** proof that tests pass.

If fixes are authorized, send accepted findings back to the implementation thread with explicit scope and acceptance checks.  
Do not allow the detached reviewer to modify the implementation implicitly.

## Validation gate

Do not accept “done” as evidence. Before releasing ownership:

1. Compare the final working tree against the recorded baseline.
2. Confirm that only authorized paths changed.
3. Inspect the complete diff for accidental deletions, generated noise, secrets, and unrelated formatting.
4. Run the narrowest relevant tests, linters, type checks, builds, or manual checks.
5. Reconcile Codex’s report with observed files and command results.
6. Record blockers and unverified assumptions explicitly.

If validation cannot run, state exactly what remains unverified and why.  
Never convert an incomplete result into a success claim.

## Status reporting

Report evidence instead of fabricated health percentages. A status update must include:

```text
OBJECTIVE
<current objective>

PHASE
<planning | executing | validating | blocked | completed>

CODEX STATE
threadId: <exact>
turnId: <exact>
threadStatus: <status>
turnStatus: <status>

OWNERSHIP
write owner: <Codex | supervisor | none>
scope: <paths>

COMPLETED
<observed changes only>

CHECKS
<command → result>

PENDING
<approval | question | blocker | next action>
```

Do not invent token costs, capability rankings, context percentages, elapsed work, or model strengths.  
If Codex exposes goal or token-budget telemetry, report the returned values as telemetry rather than estimates.

## Failure handling

| Situation                  | Action                                                                 |
|----------------------------|------------------------------------------------------------------------|
| Timeout / disconnected stream | Reconnect to the same thread and inspect status. Do not launch a duplicate turn. |
| Server restart             | Resume exact `threadId` and re-establish event handling before continuing. |
| Resume failure             | Preserve the failure and ask before creating a replacement thread.     |
| Approval timeout           | Keep the turn pending or interrupt it; never guess the user’s decision. |
| Unexpected dirty files     | Preserve them, identify ownership, pause overlapping work.             |
| Test failure               | Return the failure to the same thread with exact command + output.     |
| Context mismatch           | Send a corrected handoff through the existing thread.                  |
| Partial implementation     | Inspect and report the partial diff before retrying or handing back.   |

## Completion contract

Finish with:

```text
RESULT
<verified outcome>

CODEX STATE
threadId: <exact id>
turnId: <exact id>
threadStatus: <status>
turnStatus: <status>

CHANGES
<authorized changed paths and purpose>

VALIDATION
<checks and observed results>

OPEN ITEMS
<blockers, residual risks, or none>

OWNERSHIP
<released to supervisor/user or still held by active Codex turn>
```

Never report completion while the Codex turn is still active, an approval is unresolved, validation is pending, or write ownership is ambiguous.

## Installation and setup

### Architecture: Control Plane vs. Transport Adapter

This skill defines a **control plane** (thread/start, thread/resume, turn/steer, etc.) that is independent of any specific transport or CLI implementation.

**In your current installation:**
- **Control Plane:** Codex App Server (thread/resume, turn/steer, event streaming, approval handling)
- **Transport Adapter:** `codex_capture` C tool (PTY wrapper enabling non-interactive TTY automation for `codex result`)

The control plane may be provided by:
- Codex App Server (native, current)
- Codex MCP server (alternative bridge)
- Future native Claude-Codex integration

**codex_capture is an implementation detail of YOUR transport layer**, not a requirement for the control plane itself. An agent must not conclude “no codex_capture → no session control” — a future setup might bridge the control plane differently.

---

### Pre-flight checks

Run before first use (and after environment changes):

```bash
# 1. Codex CLI present?
codex --version || { echo "Codex CLI missing"; exit 1; }

# 2. Auth present?
test -f ~/.codex/auth.json || { echo "Run: codex login"; exit 1; }

# 3. codex_capture binary?
if [ ! -x /root/bin/codex_capture ]; then
  echo "Building codex_capture..."
  gcc -o /root/bin/codex_capture /root/bin/codex_capture.c -lutil
  chmod +x /root/bin/codex_capture
fi

# 4. Quick smoke test
/root/bin/codex_capture /tmp/codex_capture_smoke.txt "echo ok" || {
  echo "codex_capture smoke test failed"
  exit 1
}
```

### Fallback: Degraded Execution Mode vs. Session Control

If the transport adapter fails (e.g., PTY is broken, codex_capture cannot be built):

**Degraded Execution Mode (permitted):**
- Fall back to non-interactive Codex modes (`codex exec`, etc.) if available.
- Execute one-off tasks independently.
- Verify results by file system state alone.
- **CRITICAL:** Do **not** report this as a continuation of the persistent Codex session or thread.
- Document that the session was suspended and work was executed in degraded mode.

**Session Control (forbidden fallback):**
- Never resume or steer the same `threadId` using a different transport.
- Never claim "resume failed, so I used codex exec instead" — this silently creates a new context.
- If session control fails, stop and ask the user before attempting degraded work on the same thread.

**Supervisor responsibility:**
- Track whether the active turn is using full control or degraded mode.
- After degraded work, reconstruct the thread state from files and ask whether to resume the original thread with the transport adapter fixed or start a new thread.

### Dependencies

- Codex CLI ≥ 0.147.0
- `codex_capture` (openpty-based wrapper)
- Documentation: `/root/bin/CODEX_CAPTURE_README.md`

## Integration notes (Claude Code / supervisor)

- Always invoke `codex_capture <output_file> …` instead of bare `codex result` when interactive output is required.
- Record `threadId` + `sessionId` immediately after `thread/start` or `thread/resume`. Persist them across supervisor compaction.
- Parse streamed `turn` and `item` events; on disconnect, re-attach to the same `threadId` rather than starting a new turn.
- On `pendingApproval`: halt, surface the exact request to the user, and only continue after an explicit decision tied to that `turnId`.
- After supervisor compaction: resume the recorded thread and reconstruct only the last confirmed objective + pending work (do not dump the entire previous conversation).

## Changelog (v2)

- Added explicit capability discovery and pre-flight matrix.
- Added two concrete handoff examples (takeover + delegation).
- Tightened ownership and validation language; removed redundant warnings.
- Expanded failure table and recovery-after-compaction guidance.
- Added structured status-report template.
- Documented fallback path when `codex_capture` is unavailable.
- Clarified that live steering requires both `turn/steer` and `turn/interrupt`.
