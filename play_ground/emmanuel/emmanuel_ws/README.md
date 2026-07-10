# C++ Subscriber Node — `my_cpp_package`

**Branch:** `cpp_node`

Steps for setting up the C++ subscriber node that listens on `hello_topic`, matching the Python publisher node built earlier.

---

## 1. Prerequisites

- ROS2 Jazzy sourced in your terminal
- Workspace already exists at `~/ros2_ws/src/`


---

## 2. Create the Package

Generate a new C++ package inside the workspace `src` folder using `ament_cmake` as the build type, with `rclcpp` and `std_msgs` as dependencies.

```bash
cd ~/ros2_ws/src
ros2 pkg create --build-type ament_cmake my_cpp_package --dependencies rclcpp std_msgs
```

This sets up the standard structure: a `CMakeLists.txt`, a `package.xml`, an `include/` folder, and a `src/` folder.

---

## 3. Write the Subscriber Node

Create a new source file inside the package's `src/` folder for the subscriber node.

```bash
touch ~/ros2_ws/src/my_cpp_package/src/subscriber_node.cpp
```

The node should:
- Be a class that inherits from the ROS2 Node base
- Subscribe to `hello_topic`
- Log each incoming message to the terminal when it's received

---

## 4. Configure `CMakeLists.txt`

Open the package's `CMakeLists.txt` and confirm it includes:

- A `find_package` call for `rclcpp`
- A `find_package` call for `std_msgs` — **this one is easy to miss and the build will fail without it**
- An executable definition pointing at your subscriber source file
- A dependency link between that executable and `rclcpp` / `std_msgs`
- An install rule so the built executable gets placed correctly

---

## 5. Configure `package.xml`

Open the package's `package.xml` and check the dependency tags.

- There should be one dependency entry for `rclcpp` and one for `std_msgs`
- **No duplicates** — if a dependency is listed twice, the build can break

---

## 6. Build the Package

From the workspace root, build only this package using `colcon build`, targeting `my_cpp_package` specifically.

```bash
cd ~/ros2_ws
colcon build --packages-select my_cpp_package
```

Once the build finishes, source the workspace's install setup file so the new node is available to run.

```bash
source install/setup.bash
```

---

## 7. Run and Verify

Open two terminals, sourcing the workspace in both.

```bash
source ~/ros2_ws/install/setup.bash
```

- In the first terminal, run the Python publisher node:

```bash
ros2 run my_robot_package <publisher_node_name>
```

- In the second terminal, run the new C++ subscriber node:

```bash
ros2 run my_cpp_package subscriber_node
```

If everything is wired correctly, the subscriber terminal should continuously print each message it receives from the publisher, confirming both nodes are communicating over `hello_topic`.

---

## 8. Common Errors & Fixes

| Issue | Cause | Fix |
|---|---|---|
| Build can't find `std_msgs` | Missing `find_package` entry in `CMakeLists.txt` | Add the missing line, rebuild |
| Build fails reading `package.xml` | Duplicate dependency tags | Remove the duplicate, keep one entry per dependency |
| Package pushes as a Git submodule instead of a normal folder | A leftover nested `.git` folder inside the copied package | Delete the nested `.git` folder before staging the files |
| Code pushed to `master` instead of the expected branch | Git defaulted to `master` | Rename the branch to match convention, or push explicitly to the correct branch name |
| Duplicate package name conflict | Package exists in both the internship folder and `src/` at the same time | Develop only in `src/`, copy into the internship folder just before staging for push |

---

## 9. Pushing to GitHub

1. Move into your folder inside the internship repo (`emmanuelmumo/`)
2. Copy the built package folder into that location
3. Delete any nested `.git` folder inside the copied package, if one exists
4. Stage, commit, and push the folder to the `cpp_node` branch

```bash
cd ~/internship2026/emmanuelmumo
cp -r ~/ros2_ws/src/my_cpp_package .
rm -rf my_cpp_package/.git
git add my_cpp_package
git commit -m "Add C++ subscriber node (my_cpp_package) for Day 3"
git push origin cpp_node
```

---

## Summary

- `my_cpp_package` is a C++ package built with `ament_cmake`, depending on `rclcpp` and `std_msgs`
- Contains a subscriber node that listens on `hello_topic`
- Verified working alongside the Python publisher node from earlier in Day 3
- Watch for: missing `std_msgs` in `CMakeLists.txt`, duplicate tags in `package.xml`, nested `.git` folders before pushing, and correct branch naming