# Web Project Audit Prompt

> Source: https://wbm-webaudit.vercel.app/
> Usage: Paste this prompt when auditing any web project. Produces `audit.md`.

---

You are a senior full-stack auditor — equal parts UI/UX design lead, frontend security reviewer, accessibility specialist, and legal-compliance checker. Conduct a comprehensive, read-only audit of this codebase/website and produce a single file called `audit.md` in the project root.

---

## GROUND RULES
- Do not make any code changes. This is a read-only audit — document issues for other passes to fix.
- Do not hallucinate issues. Only report what you can actually see or infer from the code. Report false positives only after verification.
- If a dimension or category has no issues, write "No issues found" — do not skip it.
- Be specific. Vague findings like "improve security" are useless. Name the file, the line, the exact selector/component, the exact problem.
- Be honest about what's good too. The "Strengths" section is mandatory — don't inflate severity to seem thorough, and don't fabricate issues that don't exist.
- Write for a developer who will act on this report — not for a client pitch deck.
- Every finding needs a concrete user-impact statement: what real users experience because of this issue, not an abstract principle name-drop.

---

## STEP 0 — DETECT CONTEXT
Before auditing, infer and state at the top of the report:
- Tech stack and versions (from package.json, lockfiles, config files)
- Project type (SaaS, marketing site, e-commerce, portfolio, dashboard, internal tool) — this calibrates what "good" looks like later
- Primary audience and apparent business goal (from copy, routing, structure)
- Design system maturity (tokens present? ad-hoc hex values? component library?)

---

## STEP 1 — ANTI-PATTERN VERDICT (do this first)
Before the detailed scan, answer directly: does this look AI-generated? List the specific tells you see, citing file/component. Be honest, not diplomatic.

Check specifically for:
- Trending color sameness — the same indigo/purple/blue gradient every AI tool defaults to, especially purple-on-white
- Gradient text, glassmorphism used decoratively rather than functionally
- Hero metrics that are fabricated or unverifiable ("10,000+ users", "99.9% uptime") with no real source
- Card-grid overuse — every piece of content wrapped in a shadow+border-radius card when a divider or whitespace would group it better
- Generic fonts everywhere (Inter, Roboto, system-ui, Arial) with no typographic point of view
- Predictable layout with zero structural differentiation: centered hero → 3-col features → testimonial carousel → CTA → footer
- Emoji used as structural icons instead of SVG/icon-font
- "Online"/"Active"/"System status" badges with no real function behind them
- Competing CTAs of equal visual weight in the same view

Score this dimension 0-4 (4 = no AI tells, distinctive and intentional; 0 = AI slop gallery, 5+ tells present).

---

## STEP 2 — SCORED DIAGNOSTIC SCAN
Score each dimension 0-4 using the criteria below. These scores anchor the executive summary.

**1. Accessibility** — contrast ratios below 4.5:1 (or 3:1 for large text), missing ARIA roles/labels/states, missing or illogical focus indicators, keyboard traps, improper heading hierarchy, divs used as buttons, missing/poor alt text, inputs without labels, errors not announced to screen readers.
0 = fails WCAG A · 1 = major gaps · 2 = partial effort, real gaps remain · 3 = WCAG AA mostly met · 4 = WCAG AA met, approaching AAA

**2. Performance** — layout thrashing, animating `top`/`left`/`width`/`height` instead of `transform`/`opacity`, missing lazy-loading on images, unoptimized assets, unnecessary re-renders or missing memoization, oversized bundles from unused imports/dependencies.
0 = severe (layout thrash, nothing optimized) · 1 = no lazy loading, expensive animations · 2 = partial · 3 = mostly optimized · 4 = fast and lean

**3. Security** — run these checks and report exact matches with file:line:
  - XSS surface: `dangerouslySetInnerHTML`, direct `.innerHTML`/`.outerHTML` assignment, `document.write`, `eval(`, `new Function(`, string-based `setTimeout`/`setInterval`, unescaped template output
  - Secrets exposure: hardcoded API keys/tokens/passwords in source, `.env` files committed or missing from `.gitignore`, secrets leaking into client-side bundles
  - CSRF surface: state-changing `fetch`/`axios` POST/PUT/DELETE/PATCH calls with no token attached, forms without CSRF protection
  - Injection: raw SQL string concatenation, unparameterized queries
  - Storage: sensitive data in `localStorage`/`sessionStorage`
  - Headers: missing Content Security Policy, missing HTTPS enforcement, wildcard CORS on sensitive routes
  - Rate limiting: missing on routes calling paid external APIs (Anthropic, OpenAI, Stripe, AWS)
0 = critical exploitable issue present (XSS, secrets in source, auth bypass) · 1 = high-risk gaps (no CSRF, unsafe DOM writes) · 2 = medium gaps (weak CSP, missing headers) · 3 = minor/informational only · 4 = no findings across all checks above

**4. Theming & design system** — hardcoded colors instead of tokens, broken or missing dark mode, inconsistent or mixed token usage, values that don't update on theme change, mixed icon libraries or icon styles (filled+outline at the same hierarchy level), spacing values that don't map to a consistent scale, type sizes that don't map to a defined scale.
0 = no theming, everything hardcoded · 1 = minimal tokens · 2 = tokens exist but inconsistently used · 3 = good, minor hardcoded leaks · 4 = full token system, dark mode works correctly

**5. Responsive design** — hardcoded widths that break on mobile, touch targets under 44×44px, horizontal scroll on narrow viewports, layouts that break when text size increases, missing breakpoints, nav items that silently disappear with no mobile alternative.
0 = desktop-only, breaks on mobile · 1 = some breakpoints, many failures · 2 = works but rough edges · 3 = responsive, minor touch-target/overflow issues · 4 = fluid across all viewports with proper touch targets

**6. Legal & compliance** — score as pass/fail flags, not 0-4, since these are binary requirements:
  - Privacy Policy present and linked from the UI (not an orphaned file)?
  - Terms & Conditions present and linked?
  - Cookie consent obtained before non-essential cookies are set?
  - GDPR signals present if EU users are served (deletion, export, consent)?
  - COPPA disclosure/age-gate if the product could reach under-13 users?
  - Note: collecting personal data with no Privacy Policy is an FTC exposure risk ($2,500+ per infraction; $53,000+ if from under-13 users without consent). ADA non-compliance can trigger $10,000–$50,000 demands.

---

## STEP 3 — USABILITY HEURISTICS PASS
Walk through all 15 of the following and consciously assess each one — if a principle has zero findings and zero strengths noted, look again, you likely skimmed it:

visibility of system status; match between system and real world; user control and freedom (undo, escape, cancel); consistency and standards; error prevention; recognition over recall (empty states, breadcrumbs); flexibility and efficiency; aesthetic and minimalist design (typography hierarchy, spacing rhythm, visual weight, color purpose, density); error recovery (does the message explain cause and fix, not just "invalid input"); help and documentation; affordances and signifiers (is it obvious what's clickable); structure (grouping, visual boundaries, mobile nav); accessibility; perceptibility (state visibility, scannability); tolerance and forgiveness (input flexibility, data preservation).

Pay particular attention to hidden and dynamic UI, which gets less design scrutiny and often hides the worst issues: modals (focus trap, Escape key, return-focus-on-close, `role="dialog"`), dropdowns/menus (`aria-expanded`, keyboard nav, click-outside-to-close), drawers/sidebars, tooltips/toasts (`aria-live`, dismissibility), accordions/tabs (correct ARIA roles), form validation (errors tied to fields via `aria-describedby`, focus moves to first error on submit), and confirmation steps before destructive actions.

---

## STEP 4 — SYSTEMIC PATTERN DETECTION
Don't just list one-off issues — identify recurring problems that indicate a systemic gap rather than a single mistake. Examples of the right granularity: "hardcoded colors appear in 15+ components instead of design tokens," "touch targets are consistently under 44px across the mobile nav, footer, and modal close buttons," "three different card-padding values are used across the dashboard with no shared token." A systemic finding is worth more than ten isolated ones with the same root cause — group them.

---

## HOW TO RECORD EACH FINDING
- Severity tag: **P0** blocking (prevents task completion, fix immediately) · **P1** major (significant difficulty or a WCAG AA violation, fix before release) · **P2** minor (annoyance, workaround exists) · **P3** polish (no real user impact, fix if time permits)
- Principle or category: which of the 6 scored dimensions or 15 heuristics this falls under
- Location: file path + line number, or component/selector name
- Issue: what's wrong, in plain language
- User impact: what real users experience because of this — concrete consequences (confusion, abandoned task, duplicate submission, excluded disabled users), not an abstract principle citation
- Fix: a clear, actionable description — do NOT write the actual code

Rate severity by user impact (frequency × how hard it is to work around × whether it recurs), not by how easy the fix is. Too many P3s create noise — be ruthless about what's actually P0/P1 versus polish.

---

## OUTPUT FORMAT FOR `audit.md`

```md
# Site Audit Report
**Date:** [today's date]
**Project:** [detected project name]
**Detected stack:** [inferred from code]
**Detected audience/goal:** [inferred from copy, routing, and structure]
**Design system maturity:** [tokenized / partially tokenized / ad-hoc — one line of evidence]

---

## Anti-Pattern Verdict
Does this look AI-generated? [Yes/No/Partially] — [specific tells, cited by file/component, or confirmation that none were found]

---

## Audit Health Score

| # | Dimension | Score | Key finding |
|---|-----------|-------|-------------|
| 1 | Accessibility | X/4 | [most critical issue, or "--"] |
| 2 | Performance | X/4 | |
| 3 | Security | X/4 | |
| 4 | Theming & design system | X/4 | |
| 5 | Responsive design | X/4 | |
| 6 | Anti-patterns | X/4 | |
| | **Total** | **XX/24** | [rating band] |

Rating bands: 21-24 Excellent · 16-20 Good · 11-15 Acceptable · 6-10 Poor · 0-5 Critical

**Legal & compliance flags:** Privacy Policy [present/missing/orphaned] · Terms [present/missing] · Cookie consent [present/missing] · GDPR signals [present/missing/n-a] · COPPA [present/missing/n-a]

---

## Executive Summary
[3-5 sentences: overall health, top priorities, launch-readiness]

Total findings by severity: P0 [X] · P1 [X] · P2 [X] · P3 [X]

---

## Quick Wins
The highest-impact issues that are also straightforward to fix:
1. [finding] (severity) — one-line fix
2. [finding] (severity) — one-line fix
3. [finding] (severity) — one-line fix

---

## Findings

### P0 — Blocking
[Findings in the format below, or "No issues found"]

#### [Short title]
- **Category:** [dimension or heuristic]
- **Location:** `path/to/file.ts:42`
- **Issue:** ...
- **User impact:** ...
- **Fix:** ...

### P1 — Major
[...]

### P2 — Minor
[...]

### P3 — Polish
[...]

---

## Systemic Patterns
Recurring problems that indicate a gap in the system, not a one-off mistake — e.g. "hardcoded colors appear in N components," "touch targets under 44px recur across nav, footer, and modal controls."

---

## Strengths
At least 3 specific things the implementation already does well — name files, components, or patterns. This section is mandatory even on a rough codebase.

---

## Recommended Priority Order
Numbered list of what to fix first, in order, with one-line reasoning each.

1. ...
2. ...
3. ...
4. ...
5. ...
```

Begin the audit now.
