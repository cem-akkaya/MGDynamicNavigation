# MG Dynamic Navigation Plugin

[![Plugin version number](https://img.shields.io/github/v/release/TO-FILL/MGDynamicNavigation?label=Version)](https://github.com/TO-FILL/MGDynamicNavigation/releases/latest)
[![Unreal Engine Supported Versions](https://img.shields.io/badge/Unreal_Engine-5.4_%7C_5.5_%7C_5.6-9455CE?logo=unrealengine)](https://github.com/TO-FILL/MGDynamicNavigation/releases)
[![License](https://img.shields.io/github/license/TO-FILL/MGDynamicNavigation)](LICENSE)
[![Actively Maintained](https://img.shields.io/badge/Maintenance%20Level-Actively%20Maintained-green.svg)](#)

<img src="https://github.com/cem-akkaya/LightAwareness/blob/prod/Resources/Demo6.gif" alt="plugin-mgdn" width="100%"/>

## Overview

**MG Dynamic Navigation (MGDN)** is an Unreal Engine plugin that gives AI the ability to move reliably on **moving platforms**, such as ships, elevators, floating islands, physics-driven actors, and multi-deck structures.

Instead of relying on the static Recast NavMesh, MGDN dynamically builds a **local 3D voxel navigation grid**, detects walkable areas using raycasts, and runs a custom **A\*** solver to generate paths.  
AI movement is handled through smooth local-space spline motion, unaffected by platform rotation, acceleration, or physics simulation.

Typical use cases:
- AI movement on large ships with multiple decks
- AI walking on rotating/tilting platforms
- Physics-driven moving platforms
- Complex ship layouts with ramps, slopes, and multi-layer geometry

If you encounter any issues, please open a GitHub issue.  
If you'd like to contribute, feel free to create a pull request.

## Features

- Dynamic **3D voxel grid** generation per moving actor
- Works on **any moving/physics actor** (ships, elevators, rigs, etc.)
- Local-space **A\*** pathfinding
- Support for **ramps, slopes, multi-deck layouts**
- Spline-based movement with smoothing and rotation interpolation
- Grid visualization tools
- Fully Blueprint-callable movement functions
- No dependency on UE’s built-in NavMesh

## Examples

Replace with your own images / GIFs:

| <img src="Resources/Example1.gif" width="370"/> | <img src="Resources/Example2.gif" width="370"/> |
|:-----------------------------------------------:|:-----------------------------------------------:|
| AI following a path on a moving ship.           | Multi-deck navigation with vertical transitions. |

| <img src="Resources/Example3.gif" width="790"/> |
|:----------------------------------------------:|
| Dynamic navigation on a physics-driven platform. |

## Installation

Install the plugin like any other Unreal Engine plugin:

- Place the plugin under:  
  `YourProject/Plugins/MGDynamicNavigation`
- Enable the plugin in the Unreal Editor
- Add the **MGDNNavVolume** actor to your moving platform
- Assign or create a **MGDNNavDataAsset**
- Click **Bake Grid**

<img src="Resources/ss1.jpg" width="830"/>

## Quick Start

1. Add **MGDN Nav Volume** component to your moving platform.
2. Adjust the **volume size** to cover walkable areas.
3. Click **Bake Now** to generate the navigation grid.
4. Call the Blueprint function:


Example Blueprint:

<img src="Resources/ss2.jpg" width="830"/>

## Component Details

<img src="Resources/ss3.jpg" width="830"/>

### MGDN Nav Volume Component

- Defines the scanning bounds
- Generates voxel grid from geometry & navmesh
- Stores baked results in **MGDNNavDataAsset**
- Provides debug visualization tools

#### Parameters

- **Cell Size** – Horizontal voxel resolution
- **Cell Height** – Vertical voxel slice resolution
- **Source Asset** – Assigned nav data asset
- **Visualize Grid** – Shows walkable voxels
- **Bake Now** – Regenerates grid

### MGDynamicNavigationSubsystem

<img src="Resources/ss4.jpg" width="830"/>

- Manages all MGDN volumes in the world
- Handles AI movement updates
- Provides async pathfinding interface
- Allows querying paths in C++ or Blueprint

## Movement System

MGDN moves AI using:
- Local-space spline path
- Smooth interpolation
- Directional rotation
- Optional velocity injection for animation playback

Movement remains stable even if the ship tilts or rotates.

## FAQ

<details>
<summary><b>Can MGDN handle multiple decks?</b></summary>

> Yes. The voxel grid supports full 3D navigation with multiple height layers.
</details>

<details>
<summary><b>Does it work with physics-driven platforms?</b></summary>

> Yes. Movement is relative to the platform’s transform every frame.
</details>

<details>
<summary><b>Is Unreal’s NavMesh required?</b></summary>

> No. MGDN works entirely without UE NavMesh.
</details>

<details>
<summary><b>How do I debug walkable nodes?</b></summary>

> Use the **Visualize Grid** button inside MGDN Nav Volume.
</details>

## Known Limitations / Tips

- Extremely thin geometry requires smaller cell size
- Path smoothing depends on grid density
- Physics collisions on AI may affect the platform
- Avoid tiny cell heights unless necessary

## License

This plugin is under the [MIT License](LICENSE).  
Commercial use is allowed as long as the copyright notice is included.

## *Support Me*

If you like the plugin, you can support development with a coffee:

<a href="https://www.buymeacoffee.com/akkayaceq" target="_blank"><img src="https://cdn.buymeacoffee.com/buttons/default-yellow.png" alt="Buy Me A Coffee" height="41" width="174"></a>
