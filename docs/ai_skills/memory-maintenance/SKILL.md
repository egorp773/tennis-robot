---
name: memory-maintenance
description: Use when updating project memory, decisions, roadmap, Obsidian wiki links, test logs, or Codex skill documentation after important architecture or testing decisions.
---

# Memory Maintenance

## Purpose

Keep the project memory usable for future Codex sessions and human review.

## Files To Maintain

- `AGENTS.md`: short repository operating rules for Codex.
- `MEMORY.md`: current long-term state and project rules.
- `DECISIONS.md`: accepted decisions that should not be rediscovered.
- `AUTONOMY_ROADMAP.md`: implementation roadmap.
- `TEST_LOG.md`: physical/integration test records.
- `docs/ai_skills/*/SKILL.md`: focused Codex Skills.
- `docs/wiki/raw/`: raw notes, chats, logs, dumps.
- `docs/wiki/compiled/`: cleaned stable wiki knowledge.

## Update Rule

When architecture changes, update:

- `MEMORY.md`
- `DECISIONS.md`
- `AUTONOMY_ROADMAP.md`
- The relevant `SKILL.md`

When physical tests are run, append to:

- `TEST_LOG.md`

When old rough notes are superseded:

- Preserve them in `docs/wiki/raw/`.
- Summarize stable knowledge in `docs/wiki/compiled/` or relevant skills.

## Obsidian Links

Use Obsidian-style links for important concepts and files:

- `[[MEMORY]]`
- `[[DECISIONS]]`
- `[[AUTONOMY_ROADMAP]]`
- `[[TEST_LOG]]`
- `[[uwb-two-anchor-diagonal]]`

These links are for graph navigation. They do not need to be filesystem-perfect.
