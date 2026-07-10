
# Robotics Dojo Internship 2026

Robotics Dojo — 2026 monorepo. This repository contains everything produced during the internship programme: robot software, CAD designs, and experimental work.

----------

## Repository structure

```
internship2026/
├── playground/              # Throwaway experiments and personal sandboxes
├── robot_designs/           # CAD files and mechanical design assets
│   ├── robot_1/
│   └── robot_2/
└── robot_software/          # All ROS2 code — read the README inside before touching anything
    ├── robot_1_ws/          # Full ROS2 workspace for robot 1
    └── robot_2_ws/          # Full ROS2 workspace for robot 2

```

----------

## What goes where

### `playground/`

A low-stakes space for trying things out — tutorials, test scripts, algorithm prototypes, or anything that is not production code. Work here does not need to be clean. Work from the personal subfolder with your name if you want a persistent sandbox.

Nothing in `playground/` should ever be imported or depended on by `robot_software/`.

### `robot_designs/`

All mechanical and electrical design files. Organise by robot, then by subsystem (e.g. `robot_1/chassis/`, `robot_1/sensor_mounts/`). Keep source files (`.FCStd`, `.f3d`) alongside exports (`.stl`, `.step`) so others can modify the source, not just print the export.

If a design file is too large for Git (over ~50 MB), store it in the shared Google Drive folder and leave a link in a `README.md` inside the relevant subfolder.

### `robot_software/`

All live robot code lives here. Each robot has its own self-contained ROS2 workspace. Read `robot_software/README.md` before creating any packages — it defines the naming conventions and practices that keep both workspaces consistent and reviewable.

----------

## Ground rules

**Branch, never commit to main.** All work happens on a feature branch. `main` must always contain a working, runnable state of both robots. Nothing merges to `main` without a pull request reviewed by at least one other team member.

**Commit messages describe what changed and why.** `fix slam drift on loop closure` is a good message. `updates` is not. Use the imperative mood: `add`, `fix`, `remove`, `refactor`.

**The `.gitignore` at the root covers the whole repo.** Do not create additional `.gitignore` files inside workspaces unless you have a specific reason — check the root file first to see if your pattern is already covered.

**Do not commit generated files.** `build/`, `install/`, and `log/` are local colcon artefacts. `.pyc` and `__pycache__` are Python runtime artefacts. None of these belong in the repository.

----------

## Quick start

```bash
# Clone the repo
git clone <repo-url> internship2026
cd internship2026

# Build robot 1
cd robot_software/robot_1_ws
rosdep install --from-paths src --ignore-src -r -y
colcon build
source install/setup.bash

# Build robot 2
cd ../robot_2_ws
rosdep install --from-paths src --ignore-src -r -y
colcon build
source install/setup.bash

```

----------

## Team contacts

| Role | Name | Responsibility |
|---|---|---|
| Programme lead | — | Architecture decisions, PR approvals |
| Robot 1 lead | — | `robot_1_ws` ownership |
| Robot 2 lead | — | `robot_2_ws` ownership |


----------

