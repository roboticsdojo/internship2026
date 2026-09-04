-- NOT part of the production schema -- sample rows to sanity-check the
-- views. Safe to skip when setting up the real database.

INSERT INTO teams (team_name, institution, captain_name) VALUES
    ('Team Vulcan', 'JKUAT', 'Joan Ouma');

INSERT INTO team_members (team_id, full_name, role) VALUES
    (1, 'Joan Ouma', 'Software Lead'),
    (1, 'Member Two', 'Mechanical'),
    (1, 'Member Three', 'Electrical');

INSERT INTO robots (team_id, robot_name, length_cm, width_cm, height_cm, max_voltage_v, cad_step_file_url) VALUES
    (1, 'Vulcan Sorter', 29.5, 29.5, 28, 22.2, 'robot_designs/joan_robot_design/Joan''s_robot.step');

INSERT INTO judges (full_name, role) VALUES ('Dr. Aoki', 'Head Judge');

INSERT INTO runs (team_id, robot_id, round_id, assigned_item_id, restart_used)
VALUES (1, 1, 1, 2, FALSE);  -- assigned plastic bottles

-- Telemetry
INSERT INTO run_events (run_id, event_type, offset_seconds) VALUES
    (1, 'autonomous_start', 0),
    (1, 'waste_detected', 12.4),
    (1, 'loading_complete', 30.1),
    (1, 'terrain_complete', 88.7),
    (1, 'obstacle_response', 95.2),
    (1, 'gate_decision', 100.0),
    (1, 'depositing_complete', 150.3),
    (1, 'finish', 160.0);

-- Waste detection result
INSERT INTO run_waste_detection (run_id, item_id, detected_waste_type, detection_time_seconds, screen_shown_at)
VALUES (1, 2, 'plastic', 4.2, now());

-- Gate decision
INSERT INTO run_gate_decision (run_id, gate_status_at_arrival, path_chosen, wait_time_seconds, decision_result, points_awarded)
VALUES (1, 'open', 'path_2', 3.5, 'correct', 10);

-- Finish time performance
INSERT INTO run_finish_performance (run_id, total_time_seconds, tier, points_awarded)
VALUES (1, 160.0, 'maximum', 3);

-- A few criteria scores from the head judge
INSERT INTO run_scores (run_id, criteria_id, judge_id, points_awarded, comments)
SELECT 1, criteria_id, 1,
    CASE
        WHEN name = 'Completeness of area coverage' THEN 4.5
        WHEN name = 'Accuracy and usability of map' THEN 4
        WHEN name = 'Classification accuracy' THEN 6
        WHEN name = 'Detection speed' THEN 5
        WHEN name = 'Navigation to display screen' THEN 3
        ELSE max_points * 0.8
    END,
    NULL
FROM scoring_criteria
WHERE category_id IN (SELECT category_id FROM scoring_categories WHERE name IN ('Mapping','Waste Detection','Terrain','Obstacles','Loading','Depositing'));

-- Sanity checks
\echo '--- v_current_run_display ---'
SELECT * FROM v_current_run_display;

\echo '--- v_run_score_breakdown (first 8 rows) ---'
SELECT * FROM v_run_score_breakdown LIMIT 8;

\echo '--- v_run_totals ---'
SELECT * FROM v_run_totals;

\echo '--- v_leaderboard ---'
SELECT * FROM v_leaderboard;
