-- =====================================================================
-- 002_scoring.sql
-- Gate/route decision, task+bonus scoring, video, penalties
-- =====================================================================

-- ---------------------------------------------------------------------
-- 7. Route decision (rulebook section 7c: dynamic gate at the junction)
-- ---------------------------------------------------------------------

CREATE TABLE run_gate_decision (
    id                  SERIAL PRIMARY KEY,
    run_id              INTEGER NOT NULL REFERENCES runs(run_id) ON DELETE CASCADE UNIQUE,
    gate_status_at_arrival gate_status_enum NOT NULL,
    path_chosen         path_enum NOT NULL,
    wait_time_seconds   NUMERIC(6,2),           -- how long the robot waited before deciding
    decision_result     gate_decision_result_enum NOT NULL,
    -- points_awarded mirrors section 7c's flat tiers (correct=10, inefficient=5, failed=0).
    -- NOTE: section 7a instead splits Route Decision into
    -- "gate decision (6) + time efficiency (4)". Both appear in the
    -- rulebook; confirm with organizers which one is authoritative
    -- before the live event. This schema defaults to the 7c tiers.
    points_awarded      NUMERIC(4,2) NOT NULL CHECK (points_awarded BETWEEN 0 AND 10)
);

-- ---------------------------------------------------------------------
-- 8. Task + bonus scoring
-- ---------------------------------------------------------------------

CREATE TABLE run_scores (
    score_id       SERIAL PRIMARY KEY,
    run_id         INTEGER NOT NULL REFERENCES runs(run_id) ON DELETE CASCADE,
    criteria_id    INTEGER NOT NULL REFERENCES scoring_criteria(criteria_id),
    judge_id       INTEGER REFERENCES judges(judge_id),
    points_awarded NUMERIC(5,2) NOT NULL,
    comments       TEXT,
    scored_at      TIMESTAMPTZ NOT NULL DEFAULT now(),
    UNIQUE (run_id, criteria_id, judge_id)   -- one score per judge per criteria per run
);

-- Enforce points_awarded <= the criteria's max_points
CREATE OR REPLACE FUNCTION trg_check_score_within_max() RETURNS TRIGGER AS $$
DECLARE
    v_max NUMERIC(5,2);
BEGIN
    SELECT max_points INTO v_max FROM scoring_criteria WHERE criteria_id = NEW.criteria_id;
    IF NEW.points_awarded < 0 OR NEW.points_awarded > v_max THEN
        RAISE EXCEPTION 'points_awarded (%) must be between 0 and % for criteria %',
            NEW.points_awarded, v_max, NEW.criteria_id;
    END IF;
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER check_score_within_max
    BEFORE INSERT OR UPDATE ON run_scores
    FOR EACH ROW
    EXECUTE FUNCTION trg_check_score_within_max();

-- Finish Time Performance (section 7b) — tiered, not additive sub-criteria.
CREATE TABLE run_finish_performance (
    run_id       INTEGER PRIMARY KEY REFERENCES runs(run_id) ON DELETE CASCADE,
    total_time_seconds NUMERIC(6,2),
    tier         performance_tier_enum NOT NULL,
    points_awarded NUMERIC(3,1) NOT NULL CHECK (points_awarded BETWEEN 0 AND 3)
);

-- ---------------------------------------------------------------------
-- 9. Documentation video + engagement bonus (section 7d, 7e)
-- ---------------------------------------------------------------------

CREATE TABLE team_videos (
    video_id       SERIAL PRIMARY KEY,
    team_id        INTEGER NOT NULL REFERENCES teams(team_id) ON DELETE CASCADE UNIQUE,
    video_url      TEXT NOT NULL,
    duration_seconds SMALLINT CHECK (duration_seconds BETWEEN 180 AND 300), -- rulebook: 3-5 min
    submitted_at   TIMESTAMPTZ NOT NULL DEFAULT now()
);

CREATE TABLE video_scores (
    id             SERIAL PRIMARY KEY,
    video_id       INTEGER NOT NULL REFERENCES team_videos(video_id) ON DELETE CASCADE,
    criteria_id    INTEGER NOT NULL REFERENCES scoring_criteria(criteria_id),
    judge_id       INTEGER REFERENCES judges(judge_id),
    points_awarded NUMERIC(3,1) NOT NULL CHECK (points_awarded BETWEEN 0 AND 1),
    UNIQUE (video_id, criteria_id, judge_id)
);

CREATE TABLE video_engagement (
    id               SERIAL PRIMARY KEY,
    video_id         INTEGER NOT NULL REFERENCES team_videos(video_id) ON DELETE CASCADE UNIQUE,
    reposts          INTEGER NOT NULL DEFAULT 0,
    likes            INTEGER NOT NULL DEFAULT 0,
    comments         INTEGER NOT NULL DEFAULT 0,
    measured_at      TIMESTAMPTZ,               -- fixed date/time announced by organizers
    bonus_points_awarded NUMERIC(3,1) NOT NULL DEFAULT 0 CHECK (bonus_points_awarded BETWEEN 0 AND 5)
);

-- ---------------------------------------------------------------------
-- 10. Penalties (section 8: disqualification / infractions)
-- ---------------------------------------------------------------------

CREATE TABLE penalties (
    penalty_id     SERIAL PRIMARY KEY,
    run_id         INTEGER NOT NULL REFERENCES runs(run_id) ON DELETE CASCADE,
    reason         TEXT NOT NULL,
    points_deducted NUMERIC(5,2) NOT NULL CHECK (points_deducted >= 0),
    issued_by      INTEGER REFERENCES judges(judge_id),
    issued_at      TIMESTAMPTZ NOT NULL DEFAULT now()
);

-- ---------------------------------------------------------------------
-- 11. Team size guard (rulebook: 3-5 members per team)
-- ---------------------------------------------------------------------

CREATE OR REPLACE FUNCTION trg_check_team_size() RETURNS TRIGGER AS $$
DECLARE
    v_count INTEGER;
BEGIN
    SELECT COUNT(*) INTO v_count FROM team_members WHERE team_id = NEW.team_id;
    IF v_count > 5 THEN
        RAISE EXCEPTION 'Team % already has 5 members (max per rulebook)', NEW.team_id;
    END IF;
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER check_team_size
    AFTER INSERT ON team_members
    FOR EACH ROW
    EXECUTE FUNCTION trg_check_team_size();
