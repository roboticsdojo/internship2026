-- =====================================================================
-- 004_views.sql
-- Read-only views for the judges' screen. A display app (web dashboard,
-- kiosk script, whatever renders the monitor at the field) should poll
-- these views rather than writing raw joins itself, so the display
-- logic stays in one place.
-- =====================================================================

-- ---------------------------------------------------------------------
-- A. Live "now running" card -- team, robot, assigned waste item + image,
--    and a running clock, meant to be polled every 1-2s during a run.
-- ---------------------------------------------------------------------
CREATE VIEW v_current_run_display AS
SELECT
    r.run_id,
    t.team_name,
    rb.robot_name,
    cr.label                                   AS round_label,
    wi.name                                    AS assigned_item_name,
    wi.image_url                               AS assigned_item_image_url,
    wi.waste_type                              AS assigned_item_true_type,
    rd.detected_waste_type,
    rd.is_correct                              AS detection_correct,
    ev_start.event_time                        AS autonomous_start_time,
    EXTRACT(EPOCH FROM (now() - ev_start.event_time)) AS elapsed_seconds,
    r.restart_used,
    r.disqualified
FROM runs r
JOIN teams t   ON t.team_id = r.team_id
JOIN robots rb ON rb.robot_id = r.robot_id
JOIN competition_rounds cr ON cr.round_id = r.round_id
LEFT JOIN waste_items wi ON wi.item_id = r.assigned_item_id
LEFT JOIN run_waste_detection rd ON rd.run_id = r.run_id
LEFT JOIN run_events ev_start ON ev_start.run_id = r.run_id AND ev_start.event_type = 'autonomous_start';

COMMENT ON VIEW v_current_run_display IS
    'Poll this for the "currently on field" card: team, robot, waste item image, live elapsed time.';

-- ---------------------------------------------------------------------
-- B. Full waste-item catalogue with images, for a pre-run "what the
--    robot might see" reference panel on the screen.
-- ---------------------------------------------------------------------
CREATE VIEW v_waste_item_gallery AS
SELECT item_id, waste_type, name, image_url, description
FROM waste_items
ORDER BY waste_type, name;

-- ---------------------------------------------------------------------
-- C. Per-run score breakdown -- every criteria, its max, and what was
--    awarded (averaged across judges if more than one scored it).
-- ---------------------------------------------------------------------
CREATE VIEW v_run_score_breakdown AS
SELECT
    r.run_id,
    t.team_name,
    sc.name          AS category_name,
    scr.name         AS criteria_name,
    scr.max_points,
    ROUND(AVG(rs.points_awarded), 2) AS avg_points_awarded,
    COUNT(rs.judge_id)               AS judges_scored
FROM runs r
JOIN teams t ON t.team_id = r.team_id
JOIN scoring_categories sc ON TRUE
JOIN scoring_criteria scr ON scr.category_id = sc.category_id
LEFT JOIN run_scores rs ON rs.run_id = r.run_id AND rs.criteria_id = scr.criteria_id
GROUP BY r.run_id, t.team_name, sc.name, scr.name, scr.max_points, sc.display_order, scr.display_order
ORDER BY r.run_id, sc.display_order, scr.display_order;

-- ---------------------------------------------------------------------
-- D. Run total -- sums task-score criteria + route decision + finish
--    time tier + bonus categories, minus penalties. This is the number
--    the leaderboard screen shows.
-- ---------------------------------------------------------------------
CREATE VIEW v_run_totals AS
WITH criteria_totals AS (
    SELECT run_id, SUM(avg_points_awarded) AS criteria_points
    FROM v_run_score_breakdown
    GROUP BY run_id
),
gate_points AS (
    SELECT run_id, points_awarded AS gate_points
    FROM run_gate_decision
),
finish_points AS (
    SELECT run_id, points_awarded AS finish_points
    FROM run_finish_performance
),
penalty_totals AS (
    SELECT run_id, SUM(points_deducted) AS penalty_points
    FROM penalties
    GROUP BY run_id
)
SELECT
    r.run_id,
    t.team_id,
    t.team_name,
    rb.robot_name,
    cr.label AS round_label,
    COALESCE(ct.criteria_points, 0)
        + COALESCE(gp.gate_points, 0)
        + COALESCE(fp.finish_points, 0)
        - COALESCE(pt.penalty_points, 0) AS total_score,
    r.disqualified
FROM runs r
JOIN teams t ON t.team_id = r.team_id
JOIN robots rb ON rb.robot_id = r.robot_id
JOIN competition_rounds cr ON cr.round_id = r.round_id
LEFT JOIN criteria_totals ct ON ct.run_id = r.run_id
LEFT JOIN gate_points gp ON gp.run_id = r.run_id
LEFT JOIN finish_points fp ON fp.run_id = r.run_id
LEFT JOIN penalty_totals pt ON pt.run_id = r.run_id;

-- ---------------------------------------------------------------------
-- E. Team totals across task runs + bonus/technical categories that are
--    scored once per team rather than per run (CAD, poster, SLAM,
--    report, scalability, video, engagement) -- the actual leaderboard.
-- ---------------------------------------------------------------------
CREATE VIEW v_leaderboard AS
WITH run_side AS (
    SELECT team_id, SUM(total_score) AS run_points
    FROM v_run_totals
    WHERE NOT disqualified
    GROUP BY team_id
),
team_criteria AS (
    -- Bonus/technical categories that are not tied to a specific run
    -- (CAD, SLAM, Poster, Report, Scalability) are scored via run_scores
    -- against the team's single representative run per round; adjust
    -- the join if your event scores these independently of a run.
    SELECT r.team_id, SUM(vb.avg_points_awarded) AS bonus_points
    FROM v_run_score_breakdown vb
    JOIN runs r ON r.run_id = vb.run_id
    WHERE vb.category_name IN (
        'Innovation Bonus', 'CAD Design & Presentation', 'Virtual SLAM',
        'Poster Presentation', 'Technical Report', 'Industrial Scalability'
    )
    GROUP BY r.team_id
),
video_side AS (
    SELECT tv.team_id,
           COALESCE(SUM(vs.points_awarded), 0) + COALESCE(MAX(ve.bonus_points_awarded), 0) AS video_points
    FROM team_videos tv
    LEFT JOIN video_scores vs ON vs.video_id = tv.video_id
    LEFT JOIN video_engagement ve ON ve.video_id = tv.video_id
    GROUP BY tv.team_id
)
SELECT
    t.team_id,
    t.team_name,
    COALESCE(rs.run_points, 0)      AS task_and_route_points,
    COALESCE(tc.bonus_points, 0)    AS technical_bonus_points,
    COALESCE(vs2.video_points, 0)   AS video_points,
    COALESCE(rs.run_points, 0) + COALESCE(tc.bonus_points, 0) + COALESCE(vs2.video_points, 0) AS grand_total,
    RANK() OVER (ORDER BY COALESCE(rs.run_points, 0) + COALESCE(tc.bonus_points, 0) + COALESCE(vs2.video_points, 0) DESC) AS rank
FROM teams t
LEFT JOIN run_side rs ON rs.team_id = t.team_id
LEFT JOIN team_criteria tc ON tc.team_id = t.team_id
LEFT JOIN video_side vs2 ON vs2.team_id = t.team_id
ORDER BY rank;

COMMENT ON VIEW v_leaderboard IS
    'The main scoreboard: what the judges'' big screen shows between runs.';
