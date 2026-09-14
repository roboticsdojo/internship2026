-- =====================================================================
-- 003_seed_data.sql
-- Reference data taken directly from the Robotics Dojo 2026 rulebook.
-- Image paths are placeholders -- point them at wherever the actual
-- photos (Figures 4-6) end up in the repo, e.g.
-- gamefield_software/media/waste_items/<name>.jpg
-- =====================================================================

-- ---------------------------------------------------------------------
-- Waste items shown on the detection screen (Figures 4, 5, 6)
-- ---------------------------------------------------------------------
INSERT INTO waste_items (waste_type, name, image_url, description) VALUES
    ('organic', 'Watermelon rind', 'media/waste_items/watermelon_rind.jpg', 'Figure 4 in rulebook'),
    ('plastic', 'Plastic bottles',  'media/waste_items/plastic_bottles.jpg', 'Figure 5 in rulebook'),
    ('metal',   'Fabric',           'media/waste_items/fabric.jpg', 'Figure 6 -- listed as example item; confirm with organizers, "fabric" is not metal by common definition, flag for clarification');

-- NOTE: the rulebook labels Figure 6 "Fabric" under what reads like a metal
-- example slot, which does not match typical waste categorization (fabric
-- is usually organic/textile waste, not metal). This is almost certainly
-- a rulebook typo or a placeholder image. Confirm the real metal example
-- item with the organizing committee before the mini competition and
-- correct this row.

-- ---------------------------------------------------------------------
-- Scoring categories (rulebook section 7a: Task Scoring, 90-pt subtotal)
-- ---------------------------------------------------------------------
INSERT INTO scoring_categories (name, max_points, is_task_score, display_order) VALUES
    ('Mapping',          10, TRUE, 1),
    ('Waste Detection',  15, TRUE, 2),
    ('Terrain',          15, TRUE, 3),
    ('Obstacles',        15, TRUE, 4),
    ('Loading',          10, TRUE, 5),
    ('Route Decision',   10, TRUE, 6),
    ('Depositing',       15, TRUE, 7);

-- Bonus / technical evaluation categories (sections 7b, 7d-7g)
INSERT INTO scoring_categories (name, max_points, is_task_score, display_order) VALUES
    ('Finish Time Performance',      3, FALSE, 8),
    ('Innovation Bonus',            10, FALSE, 9),
    ('CAD Design & Presentation',   10, FALSE, 10),
    ('Virtual SLAM',                10, FALSE, 11),
    ('Poster Presentation',         10, FALSE, 12),
    ('Technical Report',            10, FALSE, 13),
    ('Industrial Scalability',      10, FALSE, 14),
    ('Documentation Video',          5, FALSE, 15),
    ('Video Engagement Bonus',       5, FALSE, 16);

-- ---------------------------------------------------------------------
-- Sub-criteria (only where the rulebook explicitly breaks points down;
-- categories with no breakdown get a single criteria matching the total)
-- ---------------------------------------------------------------------

-- Mapping (10): Completeness (5), Accuracy and usability (5)
INSERT INTO scoring_criteria (category_id, name, max_points, display_order)
SELECT category_id, c.name, c.max_points, c.ord
FROM scoring_categories, (VALUES
    ('Completeness of area coverage', 5, 1),
    ('Accuracy and usability of map', 5, 2)
) AS c(name, max_points, ord)
WHERE scoring_categories.name = 'Mapping';

-- Waste Detection (15): Classification accuracy (6), detection speed (6), navigation to screen (3)
INSERT INTO scoring_criteria (category_id, name, max_points, display_order)
SELECT category_id, c.name, c.max_points, c.ord
FROM scoring_categories, (VALUES
    ('Classification accuracy', 6, 1),
    ('Detection speed', 6, 2),
    ('Navigation to display screen', 3, 3)
) AS c(name, max_points, ord)
WHERE scoring_categories.name = 'Waste Detection';

-- Terrain (15): rulebook only lists terrain types, no point breakdown given.
-- Modeled as a single criteria; log per-terrain-segment notes in run_scores.comments.
INSERT INTO scoring_criteria (category_id, name, max_points, display_order)
SELECT category_id, 'Terrain traversal (gravel, sand, ramps, grass, grater)', 15, 1
FROM scoring_categories WHERE name = 'Terrain';

-- Obstacles (15): Detection (6), avoidance/response (6), path efficiency (3)
INSERT INTO scoring_criteria (category_id, name, max_points, display_order)
SELECT category_id, c.name, c.max_points, c.ord
FROM scoring_categories, (VALUES
    ('Obstacle detection', 6, 1),
    ('Avoidance / response', 6, 2),
    ('Path efficiency', 3, 3)
) AS c(name, max_points, ord)
WHERE scoring_categories.name = 'Obstacles';

-- Loading (10): Station selection (4), positioning (3), item securement (3)
INSERT INTO scoring_criteria (category_id, name, max_points, display_order)
SELECT category_id, c.name, c.max_points, c.ord
FROM scoring_categories, (VALUES
    ('Station selection', 4, 1),
    ('Positioning', 3, 2),
    ('Item securement', 3, 3)
) AS c(name, max_points, ord)
WHERE scoring_categories.name = 'Loading';

-- Route Decision (10): modeled as a single criteria fed by run_gate_decision
-- (section 7c flat tiers). See comment on that table for the 7a-vs-7c conflict.
INSERT INTO scoring_criteria (category_id, name, max_points, display_order)
SELECT category_id, 'Gate decision outcome', 10, 1
FROM scoring_categories WHERE name = 'Route Decision';

-- Depositing (15): Color detection (5), bin selection (5), placement precision (3), finish position (2)
INSERT INTO scoring_criteria (category_id, name, max_points, display_order)
SELECT category_id, c.name, c.max_points, c.ord
FROM scoring_categories, (VALUES
    ('Colour detection', 5, 1),
    ('Bin selection', 5, 2),
    ('Placement precision', 3, 3),
    ('Finish position', 2, 4)
) AS c(name, max_points, ord)
WHERE scoring_categories.name = 'Depositing';

-- CAD Design & Presentation (10): quality(3), justification(3), presentation(2), STEP+screenshots(2)
INSERT INTO scoring_criteria (category_id, name, max_points, display_order)
SELECT category_id, c.name, c.max_points, c.ord
FROM scoring_categories, (VALUES
    ('CAD quality', 3, 1),
    ('Design justification', 3, 2),
    ('Technical presentation', 2, 3),
    ('STEP file & screenshots', 2, 4)
) AS c(name, max_points, ord)
WHERE scoring_categories.name = 'CAD Design & Presentation';

-- Virtual SLAM (10): map quality(4), accuracy(3), navigation readiness(3)
INSERT INTO scoring_criteria (category_id, name, max_points, display_order)
SELECT category_id, c.name, c.max_points, c.ord
FROM scoring_categories, (VALUES
    ('Map quality', 4, 1),
    ('Accuracy', 3, 2),
    ('Navigation readiness', 3, 3)
) AS c(name, max_points, ord)
WHERE scoring_categories.name = 'Virtual SLAM';

-- Poster Presentation (10): clarity(4), organization(3), framing(3)
INSERT INTO scoring_criteria (category_id, name, max_points, display_order)
SELECT category_id, c.name, c.max_points, c.ord
FROM scoring_categories, (VALUES
    ('Technical clarity', 4, 1),
    ('Visual organization', 3, 2),
    ('Problem framing', 3, 3)
) AS c(name, max_points, ord)
WHERE scoring_categories.name = 'Poster Presentation';

-- Industrial Scalability (10): manufacturability(4), cost-effectiveness(3), deployment feasibility(3)
INSERT INTO scoring_criteria (category_id, name, max_points, display_order)
SELECT category_id, c.name, c.max_points, c.ord
FROM scoring_categories, (VALUES
    ('Manufacturability', 4, 1),
    ('Cost-effectiveness', 3, 2),
    ('Deployment feasibility', 3, 3)
) AS c(name, max_points, ord)
WHERE scoring_categories.name = 'Industrial Scalability';

-- Categories with no breakdown given -> single criteria matching category total
INSERT INTO scoring_criteria (category_id, name, max_points, display_order)
SELECT category_id, name || ' (overall)', max_points, 1
FROM scoring_categories
WHERE name IN ('Innovation Bonus', 'Technical Report');

-- Documentation Video (5): 1 point each per rulebook section 7d
INSERT INTO scoring_criteria (category_id, name, max_points, display_order)
SELECT category_id, c.name, c.max_points, c.ord
FROM scoring_categories, (VALUES
    ('Team roles and work distribution', 1, 1),
    ('Design and development process', 1, 2),
    ('Expected vs. actual results', 1, 3),
    ('Challenges and lessons learned', 1, 4),
    ('Presentation and video quality', 1, 5)
) AS c(name, max_points, ord)
WHERE scoring_categories.name = 'Documentation Video';

-- Video Engagement Bonus (5): reposts/shares(2), likes(2), comments(1)
INSERT INTO scoring_criteria (category_id, name, max_points, display_order)
SELECT category_id, c.name, c.max_points, c.ord
FROM scoring_categories, (VALUES
    ('Reposts / Shares', 2, 1),
    ('Likes', 2, 2),
    ('Comments', 1, 3)
) AS c(name, max_points, ord)
WHERE scoring_categories.name = 'Video Engagement Bonus';

-- Finish Time Performance: tiered, not point-additive -- no scoring_criteria row needed,
-- it's scored directly via run_finish_performance.tier + points_awarded.

-- ---------------------------------------------------------------------
-- Competition rounds (rulebook section 9: schedule)
-- ---------------------------------------------------------------------
INSERT INTO competition_rounds (round_type, round_date, label) VALUES
    ('mini_competition', '2026-10-26', 'Mini Competition -- 2 teams eliminated'),
    ('final',             '2026-11-25', 'Final Competition and live demo');
