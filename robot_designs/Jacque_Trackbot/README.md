# Robotics Dojo Competition Robot Designs

## Overview

This repository contains the mechanical designs and CAD assemblies for an all-terrain reconnaissance and logistics robot developed for the Robotics Dojo Competition.

The project focuses on creating a compact, modular, and robust robotic platform capable of operating on challenging terrain while protecting critical components and simplifying maintenance. The design prioritises reliability, traction, obstacle traversal, and ease of manufacture over maximum speed.



# Design Objectives

- Develop a compact all-terrain robotic platform.
- Protect critical electronic components.
- Improve mobility across rough and uneven terrain.
- Create a modular design that is easy to maintain and upgrade.
- Integrate a simple and reliable payload handling mechanism.
- Provide a foundation for future autonomous operation.



# Key Design Features

## Compact Chassis

The robot is built around a compact central chassis that houses all major components. The enclosed design protects internal hardware while keeping the overall size suitable for competition requirements.

### Benefits

- Protects internal components from impacts.
- Lowers the centre of gravity.
- Reduces overall footprint.
- Improves structural rigidity.



## Internal Electronics Compartments

Dedicated compartments are provided for major electronics including:

- Raspberry Pi
- Battery / Power Bank
- Motor Drivers
- Control Electronics

Separating components improves organisation, simplifies maintenance, and allows individual systems to be upgraded without redesigning the entire robot.

### Benefits

- Cleaner wiring layout.
- Easier maintenance.
- Better protection for electronics.
- Modular component replacement.



## Protected Motor Layout

The drive motors are positioned within the track assemblies instead of extending outside the robot.

This protects the motors from collisions, reduces the chance of damage during obstacle traversal, and creates a cleaner overall design.

### Benefits

- Improved motor protection.
- Better weight distribution.
- Reduced exposed components.
- Increased durability.



## Independently Articulated Tracked Modules

Each corner of the robot uses an independently articulated tracked module that allows the tracks to better follow uneven terrain while maintaining traction.

The tracked configuration was selected to maximise grip, climbing ability, and stability on loose or irregular surfaces where conventional wheeled robots may lose traction.

### Benefits

- Increased traction.
- Improved obstacle climbing.
- Better terrain adaptability.
- Stable movement across uneven surfaces.



## Sensor Platform

The top section of the chassis provides a dedicated mounting location for sensors including:

- RPLIDAR A1
- Camera Module

Mounting sensors above the chassis improves visibility while reducing obstruction from the robot body.

### Benefits

- Improved sensor field of view.
- Reduced interference from the chassis.
- Easy sensor replacement and upgrades.



## Vertical Payload Handling Mechanism

The robot incorporates a compact vertical payload handling mechanism integrated into the centre of the chassis. The system is designed to securely transport and release cargo using a simple mechanical design with minimal moving parts.

### Loading

The payload is placed onto the loading platform manually before deployment.

A lifting mechanism raises the platform vertically along a guided shaft until it reaches the transport position. Once fully raised, a locking pin secures the platform and safely supports the payload during movement.

### Transport

While travelling, the locking pin prevents the platform from moving, allowing the robot to traverse rough terrain without accidental payload release. The guide shaft keeps the platform aligned throughout its travel.

### Unloading

At the delivery location, the locking pin is removed.

The payload platform then travels downward along the guide shaft under the weight of the load. As the platform reaches its lowest position, the payload is deposited onto the ground without requiring an additional motor or actuator.

### Reset

After unloading, the return spring provides sufficient force to raise the empty platform back to its original position. The locking pin is then reinserted, preparing the mechanism for the next loading cycle.

### Benefits

- Simple mechanical operation.
- Minimal moving parts.
- Low maintenance.
- Reduced power consumption.
- Reliable payload retention during transport.
- Automatic spring-assisted reset.
- Compact integration within the chassis.



## Modular Construction

The robot is designed as a collection of independent assemblies.

Individual components and subsystems can be modified or replaced without redesigning the entire robot, making future development significantly easier.

### Benefits

- Faster design iterations.
- Easier manufacturing.
- Simplified maintenance.
- Improved scalability.


# Repository Contents

Current repository contents include:

- Robot CAD Assembly
- Mechanical Components
- Design Revisions

Future additions may include:

- STEP Files
- STL Files
- Technical Drawings
- Bill of Materials (BOM)
- Assembly Instructions



# Design Philosophy

Rather than designing the fastest robot, this project focuses on developing a capable and practical all-terrain platform.

Every major design decision was made to improve reliability, protection, serviceability, and terrain performance. The enclosed chassis protects critical electronics, the recessed motor layout reduces exposure to impacts, the articulated tracked modules improve mobility over difficult terrain, and the vertical payload mechanism provides a simple, dependable method for transporting and delivering cargo.

The result is a modular design that can be continuously refined while serving as a strong foundation for future autonomous reconnaissance and logistics applications.



# Author

**Jacque Kyalo**

Robotics Dojo Internship
