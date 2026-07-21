# [ARCHIVED — SUPERSEDED 2026-07-21] WLTIOS Development Roadmap

> **This document is stale.** It planned a 24-month, week-by-week waterfall
> schedule starting 2026-06-20 (target 1.0: 2028-06-20). Actual velocity has
> diverged significantly from this plan in both directions — several
> Alpha-scoped items (Months 7-12: compositor, window management, taskbar,
> launcher, settings app, package manager) were real and verified within
> about a month, while other items (seL4 multi-partition work) hit real
> walls not anticipated here. The current living roadmap, organized as
> parallel tracks rather than sequential months, is in `docs/PRD.md`.
>
> Kept here for history only — do not treat any week/month target below as
> current.

---

> **Timeline:** 24 months (2 years)
> **Start Date:** 2026-06-20
> **Target 1.0:** 2028-06-20

---

## Overview

```
Phase 1: Foundation     Months 1-6    ->  MVP (boots in QEMU)
Phase 2: Desktop        Months 7-12   ->  Alpha (usable desktop)
Phase 3: Security       Months 13-18  ->  Beta (full anonymity)
Phase 4: Polish + AI    Months 19-24  ->  1.0 (production ready)
```

*(Original week-by-week task breakdown for all four phases, milestone
summary table, resource-hour estimates, and risk mitigation timeline
omitted from this archive for length — the actual sequencing, task
granularity, and hour/LOC estimates below have all been superseded by
observed project velocity. See docs/PRD.md for what's actually been
verified and what's actually queued next.)*

---

*Original roadmap tagline: "WLTIOS — 24 months. 3 million lines. One vision."*
