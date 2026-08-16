# Testing Guide

## Pre-Flight Validation

```bash
# 1. Codex CLI present
codex --version

# 2. Auth valid
codex login status

# 3. codex_capture builds
gcc -o codex_capture src/codex_capture.c -lutil

# 4. Smoke test
./codex_capture /tmp/test.txt
test -f /tmp/test.txt && echo "✓ Transport works"
```

## Workflow Simulation

See `FEEDBACK.md` section "Testing & Validation" for complete 8-phase workflow test including:
- Thread initialization
- Handoff contract
- Event monitoring
- Live steering
- Validation gate
- Ownership release

## Integration Testing

When Codex App Server is available:

```bash
# 1. Start session
threadId=$(codex_api thread/start | jq -r .threadId)

# 2. Save IDs
echo $threadId > .codex_thread_id

# 3. Send handoff
codex_api turn/start --objective "..." --scope "..."

# 4. Monitor events (parallel process)
codex_api events --threadId $threadId &

# 5. Steer if needed
codex_api turn/steer --threadId $threadId --turnId $(cat .codex_turn_id) \
  --instruction "correction"

# 6. Validate independently
git diff --stat
python3 -m pytest

# 7. Release
codex_api turn/complete --threadId $threadId
```

