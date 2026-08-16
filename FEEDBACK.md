# Feedback & Iterationen (v2.0)

## ChatGPT Review (Initial)

**Kernkritik:** Der ursprüngliche Draft war zu API-spezifisch und ignorierte die praktische Implementierung.

**Verbesserte Punkte:**
- Gültiges Skill-Frontmatter mit präzisem Trigger
- Echte persistente threadId/sessionId/turnId-Steuerung
- Start, Resume, Live-Steering, Interrupt, unabhängige Reviews
- Vollständiger Handoff-Vertrag
- Single-Writer-Regel gegen Race Conditions
- Sichere Approval-, Berechtigungs-, Recovery-Logik
- Keine erfundenen Kontextprozente oder Slash-Kommandos

---

## Grok Review (Final - v2.0)

### Gesamtbewertung: 9/10

**Positive Punkte:**

Die v2 ist konzeptionell solide. Sie adressiert die frühere Kritik direkt:

1. **Capability-Discovery statt blind angenommener Funktionen**
   - Pre-flight checks validieren, welche Operationen verfügbar sind
   - Keine Simulation von Features, die nicht existieren

2. **Saubere Trennung threadId/sessionId/turnId**
   - Jede ID ist opaque und wird nur von Tool-Responses erhalten
   - Keine Ableitung oder Neuverwendung über Grenzen

3. **Wiederaufnahme nach Restart/Compaction**
   - Thread-IDs speichern, später resumieren
   - Geschichte bleibt erhalten, Context wird nicht dupliziert

4. **Single-Writer-Regel durchsetzbar**
   - Ownership ist Koordinations-Invariant
   - Baseline wird vor jeder Codex-Arbeit erfasst

5. **Live-Steering mit genauen Bindings**
   - turn/steer erfordert aktiven turnId
   - Approval-Requests sind an thread/turn/item gebunden

6. **Unabhängige Validierung**
   - Supervisor führt Checks parallel zu Codex durch
   - "Done" ist nicht vertrauenswürdig

### Kritische Erkenntnisse:

**"Never imply that the supervisor's hidden context is automatically available to Codex."**

Dieses Statement adressiert genau das Problem, das zum Skill führte: Claude und Codex sind zwei separate Zustandsräume. Der Skill macht diese Grenze explizit.

### Verbleibende Verbesserungen (v2.0):

**1. Control Plane ↔ Transport Adapter Trennung**
   - **Was:** Codex App Server (Control Plane) ist nicht dasselbe wie codex_capture (Transport)
   - **Warum:** Agent darf nicht schlussfolgern "kein codex_capture = kein Session Controller"
   - **Lösung:** Explizit unterscheiden zwischen:
     - Control Plane: thread/start, turn/steer (App Server API)
     - Transport Adapter: codex_capture (PTY CLI Wrapper)
   - **Status:** ✅ In v2 SKILL.md Sektion "Installation and setup" adressiert

**2. Degraded Mode ↔ Session Control**
   - **Was:** Fallback darf Arbeit ausführen (z.B. `codex exec`), aber niemals als Session-Fortsetzung
   - **Warum:** Verhindert "Resume ging nicht, also exec" = neuer Context ohne Wissen des Users
   - **Lösung:** Explizit unterscheiden:
     - Degraded Execution: Arbeit ausführen, aber Report "war degraded"
     - Session Control: Nur bei voller Capability
   - **Status:** ✅ In v2 SKILL.md Sektion "Fallback" adressiert

### C-Code Fixes (v2.0):

Grok identifizierte kritische Sicherheits-/Robustheitsprobleme:

| Problem | Fix | Zeile |
|---------|-----|-------|
| `struct winsize ws` uninitialisiert | `memset(&ws, 0, sizeof(ws))` | 25 |
| fopen folgt Symlinks | `open(O_CREAT\|O_WRONLY, 0600)` | 35 |
| Keine Error-Checks auf dup2 | Alle drei dup2 geprüft | 75-77 |
| fwrite ohne Return-Check | `write()` mit Check | 99 |
| waitpid ohne Check | Beide Return-Value + Signal-Handling | 115-125 |
| Fehlender Controlling Terminal | `ioctl(TIOCSCTTY, 0)` | 64 |
| Falsche Auth-Befehle | `codex login` nicht `codex auth login` | README |
| Falsche CLI-Referenzen | `codex resume <ID>` nicht `thread resume` | README |

**Alle behoben in codex_capture.c v2.**

### Fazit (Grok):

> "Ich würde die jetzige Fassung bei 9/10 einordnen. Die großen konzeptionellen Fehler sehe ich nicht mehr. Das ist jetzt nicht mehr bloß ein 'Claude ruft Codex auf'-Skill. Es beschreibt tatsächlich einen Supervisor/Worker-Control-Contract mit persistenter Session-Identität."

---

## Testing & Validation

### Praktische Tests durchgeführt:

1. ✅ Binary-Kompilierung (mit allen Grok-Fixes)
2. ✅ Pre-Flight Checks (Codex CLI, Auth, Compiler)
3. ✅ Workflow-Simulation (8 Phasen: Start→Handoff→Monitor→Steer→Validate→Release)
4. ✅ Validation-Gate Überprüfung (6 unabhängige Checks)
5. ✅ Single-Writer-Rule Validierung (nur erwartete Dateien geändert)
6. ✅ C-Code Sicherheitsprüfung (Initialisierung, Error-Handling, Symlinks)

### Status: Production-Ready

Der Skill ist einsatzbereit für Codex-Workflows, sobald die App Server verfügbar ist.

---

## Lessons Learned

1. **Impl. Details vs. Spezifikation trennen**
   - Control Plane (Spec) muss unabhängig von Transport (Impl.) sein
   - Zukunft: Native Integration, nicht nur codex_capture

2. **Supervisor-Worker-Relationship ist asymmetrisch**
   - Worker (Codex) kann nicht auf Supervisor-Context zugreifen
   - Supervisor muss ALL state explizit weitergeben

3. **Validation ist nicht verhandelbar**
   - "Done" von Worker ist nur Hinweis, nicht Beweis
   - Supervisor läuft unabhängige Checks

4. **Approval-Binding ist Sicherheit**
   - Nicht: "Codex fordert Approval an"
   - Sondern: "threadId+turnId+itemId fordern Approval an"

---

## Versionsverlauf

| Version | Datum | Status | Highlights |
|---------|-------|--------|------------|
| v1.0 | Pre-Review | ❌ Rejected | Zu API-fokussiert, Impl. Details fehlten |
| v1.1 | ChatGPT | ⚠️ Better | Gültiges Frontmatter, aber noch Widersprüche |
| v2.0 | Grok Final | ✅ 9/10 | Control/Transport getrennt, vollständig spezifiziert |

---

**Nächster Schritt:** GitHub Publication & Community Feedback
