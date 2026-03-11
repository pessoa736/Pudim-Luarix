---
description: "Use when creating or updating project documentation, README, roadmap, security notes, architecture guides, or markdown docs. Enforces documentation placement and repo layout rules."
name: "Documentation Location Rules"
applyTo: "**/*.md"
---
# Documentation Location Rules

- Place markdown documentation files under docs/.
- Keep .txt files outside docs/ unless explicitly requested.
- Keep LICENSE at repository root unless explicitly requested otherwise.
- When moving docs, update internal links/paths that reference moved files.
- Prefer concise docs focused on practical implementation details for this kernel project.
- Keep `ROADMAP.md` and `.github/instructions/*.instructions.md` aligned with implemented features when behavior/conventions change.
