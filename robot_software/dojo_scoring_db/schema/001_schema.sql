-- =====================================================================
-- Robotics Dojo 2026 — Waste Management Challenge
-- Judge Scoring & Display Database
-- =====================================================================
-- Target: PostgreSQL 14+
-- Purpose: store team/robot registration, per-run event timestamps,
--          waste-detection results, task + bonus scoring, and expose
--          views the judges' screen can poll directly.
-- =====================================================================

-- ---------------------------------------------------------------------
-- 0. Extensions
-- ---------------------------------------------------------------------
CREATE EXTENSION IF NOT EXISTS "pgcrypto";  -- gen_random_uuid(), if you want UUID PKs later

-- ---------------------------------------------------------------------
-- 1. Enumerated types
-- ---------------------------------------------------------------------

CREATE TYPE waste_type_enum AS ENUM ('organic', 'plastic', 'metal');

-- Offloading zone colour -> waste type, per rulebook section 5 (Depositing Task)
--   Blue = plastic, White = organic, Black = metal
CREATE TYPE offload_colour_enum AS ENUM ('blue', 'white', 'black');

CREATE TYPE round_type_enum AS ENUM ('mini_competition', 'final');

CREATE TYPE gate_status_enum AS ENUM ('open', 'closed');

CREATE TYPE path_enum AS ENUM ('path_1', 'path_2');

-- Section 7c: gate decision outcome tiers
CREATE TYPE gate_decision_result_enum AS ENUM ('correct', 'inefficient', 'failed');

-- Section 7b: performance grading tiers (used for Finish Time Performance)
CREATE TYPE performance_tier_enum AS ENUM ('maximum', 'average', 'minimum', 'failed');

-- Section 8: required telemetry events, in the order the rulebook lists them
CREATE TYPE run_event_enum AS ENUM (
    'autonomous_start',
    'waste_detected',
    'loading_complete',
    'terrain_complete',
    'obstacle_response',
    'gate_decision',
    'gate_wait_start',
    'gate_wait_end',
    'depositing_complete',
    'finish'
);

-- ---------------------------------------------------------------------
-- 2. Reference data: waste items shown on the detection screen,
--    scoring categories/criteria straight from the rulebook
-- ---------------------------------------------------------------------

-- The physical/simulated waste objects the robot must classify.
-- image_url is what the judges' screen renders during the Waste
-- Detection task (Figures 4-6 in the rulebook: watermelon rind,
-- plastic bottles, fabric).
CREATE TABLE waste_items (
    item_id      SERIAL PRIMARY KEY,
    waste_type   waste_type_enum NOT NULL,
    name         TEXT NOT NULL,                 -- e.g. 'Watermelon rind'
    image_url    TEXT NOT NULL,                  -- path/URL rendered on the display screen
    description  TEXT,
    created_at   TIMESTAMPTZ NOT NULL DEFAULT now()
);

COMMENT ON TABLE waste_items IS
    'Reference catalogue of objects shown on the waste-detection screen during a run.';

-- Top-level scoring categories (rulebook section 7a and 7g).
-- max_points is stored for reference/validation but the *authoritative*
-- total always comes from summing scoring_criteria, so category and
-- criteria totals can be reconciled instead of drifting apart.
CREATE TABLE scoring_categories (
    category_id   SERIAL PRIMARY KEY,
    name          TEXT NOT NULL UNIQUE,          -- e.g. 'Mapping', 'CAD Design & Presentation'
    max_points    NUMERIC(5,2) NOT NULL CHECK (max_points >= 0),
    is_task_score BOOLEAN NOT NULL DEFAULT TRUE, -- TRUE = counts toward the 90-pt task subtotal
    display_order SMALLINT NOT NULL DEFAULT 0
);

-- Sub-criteria within a category (e.g. Mapping -> Completeness (5) + Accuracy/usability (5))
CREATE TABLE scoring_criteria (
    criteria_id   SERIAL PRIMARY KEY,
    category_id   INTEGER NOT NULL REFERENCES scoring_categories(category_id) ON DELETE CASCADE,
    name          TEXT NOT NULL,
    max_points    NUMERIC(5,2) NOT NULL CHECK (max_points >= 0),
    display_order SMALLINT NOT NULL DEFAULT 0,
    UNIQUE (category_id, name)
);

CREATE TABLE judges (
    judge_id   SERIAL PRIMARY KEY,
    full_name  TEXT NOT NULL,
    role       TEXT,               -- e.g. 'Head Judge', 'Technical Judge'
    email      TEXT UNIQUE
);

-- ---------------------------------------------------------------------
-- 3. Teams, members, robots
-- ---------------------------------------------------------------------

CREATE TABLE teams (
    team_id      SERIAL PRIMARY KEY,
    team_name    TEXT NOT NULL UNIQUE,
    institution  TEXT,
    captain_name TEXT NOT NULL,     -- rulebook: one designated captain for official comms
    captain_contact TEXT,
    created_at   TIMESTAMPTZ NOT NULL DEFAULT now()
);

CREATE TABLE team_members (
    member_id   SERIAL PRIMARY KEY,
    team_id     INTEGER NOT NULL REFERENCES teams(team_id) ON DELETE CASCADE,
    full_name   TEXT NOT NULL,
    role        TEXT,               -- e.g. 'Mechanical lead', 'Firmware'
    UNIQUE (team_id, full_name)
);

-- Team size rule (3-5 members) is enforced in application code / a trigger,
-- not at the column level -- see 004_triggers.sql.

CREATE TABLE robots (
    robot_id        SERIAL PRIMARY KEY,
    team_id         INTEGER NOT NULL REFERENCES teams(team_id) ON DELETE CASCADE,
    robot_name      TEXT NOT NULL,
    length_cm       NUMERIC(5,2) NOT NULL CHECK (length_cm <= 30),
    width_cm        NUMERIC(5,2) NOT NULL CHECK (width_cm <= 30),
    height_cm       NUMERIC(5,2) NOT NULL CHECK (height_cm <= 30),
    max_voltage_v   NUMERIC(5,2) NOT NULL CHECK (max_voltage_v <= 24),
    cad_step_file_url TEXT,          -- link to the .step submitted for CAD scoring
    created_at      TIMESTAMPTZ NOT NULL DEFAULT now()
);

COMMENT ON COLUMN robots.length_cm IS 'Rulebook section 6: max 30x30x30 cm, enforced via CHECK.';
COMMENT ON COLUMN robots.max_voltage_v IS 'Rulebook section 6: max system voltage 24V, enforced via CHECK.';

-- ---------------------------------------------------------------------
-- 4. Competition structure: rounds and runs
-- ---------------------------------------------------------------------

CREATE TABLE competition_rounds (
    round_id    SERIAL PRIMARY KEY,
    round_type  round_type_enum NOT NULL,
    round_date  DATE NOT NULL,
    label       TEXT              -- e.g. 'Mini Competition (Oct 26)', 'Final (Nov 25)'
);

-- One row per robot attempt on the field.
CREATE TABLE runs (
    run_id                SERIAL PRIMARY KEY,
    team_id               INTEGER NOT NULL REFERENCES teams(team_id),
    robot_id              INTEGER NOT NULL REFERENCES robots(robot_id),
    round_id              INTEGER NOT NULL REFERENCES competition_rounds(round_id),
    run_number            SMALLINT NOT NULL DEFAULT 1,   -- supports multiple attempts if allowed
    assigned_item_id      INTEGER REFERENCES waste_items(item_id), -- which load this team is assigned to pick
    scheduled_start_time  TIMESTAMPTZ,
    restart_used          BOOLEAN NOT NULL DEFAULT FALSE, -- rulebook: one restart allowed per round
    disqualified          BOOLEAN NOT NULL DEFAULT FALSE,
    disqualification_reason TEXT,
    notes                 TEXT,
    created_at            TIMESTAMPTZ NOT NULL DEFAULT now(),
    UNIQUE (team_id, round_id, run_number)
);

CREATE INDEX idx_runs_round ON runs(round_id);
CREATE INDEX idx_runs_team ON runs(team_id);

-- ---------------------------------------------------------------------
-- 5. Required telemetry (rulebook section 8: "Required Data")
-- ---------------------------------------------------------------------

CREATE TABLE run_events (
    event_id     BIGSERIAL PRIMARY KEY,
    run_id       INTEGER NOT NULL REFERENCES runs(run_id) ON DELETE CASCADE,
    event_type   run_event_enum NOT NULL,
    event_time   TIMESTAMPTZ NOT NULL DEFAULT now(),
    offset_seconds NUMERIC(6,2),   -- seconds since autonomous_start, computed by the robot or backfilled
    UNIQUE (run_id, event_type)     -- each event fires once per run (gate_wait excluded, see below)
);

-- gate_wait_start / gate_wait_end can repeat if the robot re-checks the gate,
-- so relax uniqueness for just those two event types via a partial constraint.
ALTER TABLE run_events DROP CONSTRAINT run_events_run_id_event_type_key;
CREATE UNIQUE INDEX uq_run_events_single ON run_events (run_id, event_type)
    WHERE event_type NOT IN ('gate_wait_start', 'gate_wait_end');

CREATE INDEX idx_run_events_run ON run_events(run_id);

COMMENT ON TABLE run_events IS
    'Timestamp log matching rulebook section 8 required-data fields. The judges'' screen and total run time are both derived from this table.';

-- ---------------------------------------------------------------------
-- 6. Waste detection result per run
-- ---------------------------------------------------------------------

CREATE TABLE run_waste_detection (
    detection_id       SERIAL PRIMARY KEY,
    run_id             INTEGER NOT NULL REFERENCES runs(run_id) ON DELETE CASCADE,
    item_id            INTEGER NOT NULL REFERENCES waste_items(item_id),  -- item shown on screen
    detected_waste_type waste_type_enum,        -- what the robot reported
    is_correct          BOOLEAN,                 -- set by trg_set_detection_correctness below
    detection_time_seconds NUMERIC(6,2),         -- how long it took to classify
    screen_shown_at    TIMESTAMPTZ,
    UNIQUE (run_id)                              -- one detection task per run
);

-- Postgres doesn't allow a subquery inside GENERATED ALWAYS AS, so
-- is_correct is maintained by a trigger instead (compares against
-- the item's true waste_type on insert/update).
CREATE OR REPLACE FUNCTION trg_set_detection_correctness() RETURNS TRIGGER AS $$
BEGIN
    SELECT (NEW.detected_waste_type IS NOT DISTINCT FROM w.waste_type)
    INTO NEW.is_correct
    FROM waste_items w
    WHERE w.item_id = NEW.item_id;
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER set_detection_correctness
    BEFORE INSERT OR UPDATE ON run_waste_detection
    FOR EACH ROW
    EXECUTE FUNCTION trg_set_detection_correctness();
