# Gamefield simulation package (Gazebo Harmonic / ROS 2 Jazzy)

## Why boxes weren't used

I pulled the STL apart before building anything. It isn't a simple box +
ramp field — it's a maze-style layout with **189 distinct wall faces**
(interior partitions, not just an outer perimeter). Hand-authoring that many
`<box>` elements with correct pose/size would take a long time and is easy
to get subtly wrong (a wall 2mm off can leave a robot clipping through a
corner).

The actual root cause matches what you described — Gazebo's physics engines
(DART/ODE/Bullet) only compute correct contacts against **convex** shapes —
but the fix doesn't have to be manual boxes. `gz-physics` will happily use a
mesh for collision as long as it's convex, so the real fix is: decompose the
concave mesh into a small set of convex pieces automatically, and use
*those* as the collision geometry. That's what this package does:

- **Visual**: the original mesh, unmodified — `meshes/gamefield_visual.stl`
- **Collision**: 26 convex hulls generated from that same mesh via
  CoACD (convex decomposition) — `meshes/collision/hull_00.obj` … `hull_25.obj`,
  each referenced as its own `<collision>` in `model.sdf`

No Blender, no Inventor, no manually measured dimensions — the hulls were
computed straight from your STL, so they match the actual field geometry
(walls, corners, whatever ramps/steps exist) instead of an approximation.

The mesh was also re-centered so the origin sits at the middle of the floor,
with the floor at Z=0 (field footprint ≈ 6.03m × 3.06m × 0.6m tall).

## Folder layout

```
sim_package/
├── models/
│   └── gamefield/
│       ├── model.config
│       ├── model.sdf
│       └── meshes/
│           ├── gamefield_visual.stl      (visual only)
│           └── collision/
│               ├── hull_00.obj ... hull_25.obj   (26 convex collision pieces)
└── worlds/
    └── gamefield.world
```

## Running it

1. Copy `models/gamefield` into a directory Gazebo can find, and point
   `GZ_SIM_RESOURCE_PATH` at its parent, e.g.:

   ```bash
   export GZ_SIM_RESOURCE_PATH=$HOME/sim_package/models:$GZ_SIM_RESOURCE_PATH
   ```

2. Launch with ROS 2 Jazzy's `ros_gz_sim`:

   ```bash
   ros2 launch ros_gz_sim gz_sim.launch.py \
       gz_args:="-r $HOME/sim_package/worlds/gamefield.world"
   ```

   or directly with `gz sim`:

   ```bash
   gz sim -r $HOME/sim_package/worlds/gamefield.world
   ```

3. Spawn your robot into the world as usual (e.g. `ros_gz_sim create` or a
   `<include>` in the world file) — it will now collide correctly with the
   walls and interior partitions instead of falling through them.

## If you want fewer/more collision pieces

The hull count (26) is controlled by the CoACD `threshold` parameter — lower
threshold = tighter fit = more hulls; higher threshold = fewer, looser
hulls. If you ever need to regenerate this from an updated STL, I can rerun
the decomposition and hand you a fresh `model.sdf`.
