# Robotics Dojo 2026 — Judge Scoring & Display Database

PostgreSQL schema for the waste-management arena: registration, per-run telemetry,
waste-detection results, task/bonus scoring, and views the judges' screen can poll
directly. Tested end-to-end on Postgres 16 with sample data — see
`schema/optional_sample_data.sql`.

## Where this lives

You asked for it under `robot_software/`, but heads-up: `robot_software/README.md`
defines that folder strictly as two ROS2 workspaces (`robot_1_ws`, `robot_2_ws`)
with per-robot package naming conventions. A Postgres schema isn't a ROS2 package
and doesn't belong inside either workspace. Two reasonable options:

1. Drop this whole `dojo_scoring_db/` folder in at the **repo root**, alongside
   `robot_software/`, `robot_designs/`, and `play_ground/` — it's infrastructure
   for the competition as a whole, not something either robot's workspace depends on.
2. If it must sit under `robot_software/`, put it in a clearly-separate
   subfolder like `robot_software/dojo_scoring_db/` and note in that folder's
   README that it's *not* a ROS2 package, so nobody tries to `colcon build` it.

Either way, per `CONTRIBUTING.md` this needs its own Jira ticket (e.g.
`chore/RDI-XXX-add-judge-scoring-db`) and PR — it's a config/infra change.

## Setup

```bash
createdb dojo_scoring
psql -d dojo_scoring -f schema/001_schema.sql
psql -d dojo_scoring -f schema/002_scoring.sql
psql -d dojo_scoring -f schema/003_seed_data.sql
psql -d dojo_scoring -f schema/004_views.sql

# optional: sample rows to try the views
psql -d dojo_scoring -f schema/optional_sample_data.sql
```

## Design overview

**Reference data** (`waste_items`, `scoring_categories`, `scoring_criteria`) is
seeded straight from the rulebook — every point value in section 7 is in
`003_seed_data.sql` as an actual row, not a hardcoded number in application code.
If the organizers revise the rulebook, you update rows, not code.

**Registration** (`teams`, `team_members`, `robots`) enforces the rules that are
checkable at insert time: max 5 members per team (trigger), 30×30×30cm and 24V
limits (CHECK constraints).

**Per-run data:**
- `runs` — one row per robot attempt
- `run_events` — the exact fields listed in rulebook section 8 ("Required Data"):
  autonomous start, waste detected, loading/terrain/depositing complete, gate
  decision/wait, finish. Elapsed times and the leaderboard are all derived from
  this table rather than stored redundantly.
- `run_waste_detection` — what the robot reported vs. the item's true type,
  auto-graded by a trigger (`is_correct`)
- `run_gate_decision` — the dynamic-barrier decision (open/closed, path chosen,
  correct/inefficient/failed)
- `run_scores` — one row per (run, criteria, judge); a trigger rejects any score
  above that criteria's max_points
- `run_finish_performance` — the maximum/average/minimum/failed tiers from
  section 7b

**Team-level bonus data:** `team_videos`, `video_scores`, `video_engagement` for
the documentation video (5 pts) and engagement bonus (5 pts).

**Judge display:** four views in `004_views.sql`, meant to be polled directly by
whatever renders the field-side screen — a small web dashboard, a kiosk script,
whatever you build next:

| View | What it's for |
|---|---|
| `v_current_run_display` | The "on field right now" card — team, robot, the waste item **image** the robot needs to identify, live elapsed time |
| `v_waste_item_gallery` | All waste items with images, for a reference panel |
| `v_run_score_breakdown` | Every criteria for a run, max points vs. awarded |
| `v_run_totals` | Per-run score (criteria + gate decision + finish time − penalties) |
| `v_leaderboard` | The scoreboard — ranked team totals across runs + bonus categories |

Rather than the display app re-deriving totals from raw tables (and risking
drift from how judges actually score), it just reads these views.

## Rulebook inconsistencies flagged during design

I built this straight off the PDF you sent, but two things don't add up and
should get confirmed with the organizing committee before the live event —
I left comments in the SQL at both spots rather than silently picking one
interpretation:

1. **Max possible score**: section 7a's category table sums to **163** points,
   but the text immediately after says "Maximum possible score: 140 points."
2. **Route Decision scoring**: section 7a splits it as gate decision (6) +
   time efficiency (4) = 10. Section 7c instead gives flat tiers — correct=10,
   inefficient=5, failed=0. The schema uses the 7c tiers (`run_gate_decision.points_awarded`)
   since that section is more specific about the actual gate mechanics.
3. Minor: Figure 6 is captioned "Fabric" but appears to be the example for the
   **metal** category, which doesn't match typical waste categorization —
   flagged in a comment on the `waste_items` seed row, worth double-checking
   against the actual reference images.

## Enhancements worth considering next

- **Live screen refresh without polling**: use Postgres `LISTEN/NOTIFY` —
  fire a `NOTIFY run_update` from the `run_events`/`run_scores` triggers, and
  have the display app subscribe instead of polling every N seconds.
- **Read-only judge role**: `CREATE ROLE judge_readonly; GRANT SELECT ON ALL
  TABLES IN SCHEMA public TO judge_readonly;` so anyone building a display
  client can't accidentally write to scoring tables.
- **Photo capture per run**: add a `run_photos` table (run_id, image_url,
  caption) if you want the judges' screen to also show a live camera still of
  the robot depositing, not just the reference waste-item image.
- **Multi-round history**: `runs` already supports `run_number` and multiple
  `competition_rounds`, so a team's mini-competition run and final run both
  live in the same schema — useful for a "progress" view showing improvement
  round over round.
- **CSV/BOM export**: rulebook section 9 mentions BOM submission via Excel —
  a `bill_of_materials` table (robot_id, part_name, quantity, unit_cost,
  supplier) would let you generate that export from the same DB instead of a
  separate spreadsheet that can drift out of sync.
- **Backups during the live event**: `pg_dump` on a cron every few minutes
  during the actual competition day — a screen crash mid-final shouldn't lose
  score data.

## Suggested next step

If you want, I can build the actual display app next — a small dashboard
(HTML/JS or a lightweight Python service) that polls `v_current_run_display`
and `v_leaderboard` and renders them on the field screen. Just say the word.
