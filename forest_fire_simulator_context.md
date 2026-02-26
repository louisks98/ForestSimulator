# Forest Fire Simulator — Project Context for Claude Code

## Project Goal

Help build a **forest fire simulator in Unreal Engine 5**, grounded in two academic papers on physically-based wildfire and wood combustion simulation. The simulator should model individual tree combustion and fire propagation across a forest at interactive rates.

---

## Source Papers

### Paper 1: *Interactive Wood Combustion for Botanical Tree Models*
- **Authors:** Pirk, Jarząbek, Hädrich, Michels, Palubicki (2017) — ACM Transactions on Graphics
- **Focus:** Per-tree, per-branch combustion simulation at fine granularity
- **Key contributions:**
  - Trees represented as skeletal graphs converted to Cosserat rod physics
  - Triangulated surface mesh generated from rods (generalized cylinders per branch chain)
  - Per-triangle simulation state: temperature, mass, char layer thickness, virgin wood thickness, moisture
  - Pyrolysis model: wood decomposes into char + flammable gases; char layer insulates inner wood → realistic tipping point in mass loss rate
  - Heat diffuses along mesh surface via discrete Laplacian; cross-junction adjacency handled via nearest-vertex lookup
  - Branch vertices deform inward along normals as char grows (contraction)
  - Branches bend and break when strain exceeds threshold (Cosserat physics)
  - Leaves are separate 2D billboard quads attached to branch particles; burn without char insulation
  - Animation via Linear Blend Skinning — vertices bound to two rod-element particles
  - Visual combustion: char parameter (darkening) + glow parameter (emissive) blended in material

### Paper 2: *Fire in Paradise: Mesoscale Simulation of Wildfires*
- **Authors:** Hädrich, Banuti, Pałubicki, Pirk, Michels (2021) — ACM Transactions on Graphics
- **Focus:** Forest-scale wildfire propagation at interactive rates (100K+ trees)
- **Key contributions:**
  - Module-based tree representation: each tree is a directed graph of branch modules (sub-tree clusters), modules are instanced/reused across the forest
  - Combustion operates at module level (aggregate mass + temperature per module) — coarser than Paper 1 but scalable
  - Modules distinguished as *internal nodes* (belong to one module) and *connection nodes* (shared between modules — each module stores its own radius there, rendered as average)
  - Inter-tree fire spread via volumetric Eulerian fluid grid: burning modules raise ambient temperature → ignites adjacent modules/trees
  - Wind modeled via Navier-Stokes; wind decreases total burned area by directing fire rather than spreading isotropically
  - Terrain slope has exponential effect on fire propagation speed (uphill = faster)
  - Forest density tipping point: ~58% tree cover threshold below which fire self-limits
  - Wildfire management: interactive fire retardant, firebreak cutting
  - Flammagenitus clouds: fire-induced water vapor condensation modeled via extended Kessler model; can produce rainfall that extinguishes fire or lightning that starts new ignition points
  - Combustion and fire simulation are **decoupled**: tree detail (module count) and fluid grid resolution can vary independently

---

## Proposed Tree Actor Architecture (UE5)

### Overview

A tree is composed of three layers:
1. **Skeletal Graph** — topology and physics geometry
2. **Simulation State** — combustion data living on edges and modules
3. **Procedural Mesh + Material** — visual representation driven by simulation state

---

### Data Structures

#### Node (Particle)
Represents a branch junction or tip. Stores physics/geometry data only.
```cpp
struct FTreeNode
{
    int32       ParentIndex;      // -1 for root
    FVector     Position;         // World-space position
    FQuat       Orientation;      // Cosserat material frame (quaternion)
    float       Radius;           // Branch thickness at this node
    float       Stiffness;        // Decreases as mass is lost (affects bending/breaking)
};
```

#### Edge (Rod Element)
Represents a single branch segment between two nodes. **This is where simulation state lives.**
```cpp
struct FBranchEdge
{
    int32   NodeStart;
    int32   NodeEnd;
    float   Length;

    // Combustion state
    float   Temperature;          // Surface temperature
    float   Mass;                 // Remaining wood mass
    float   CharThickness;        // Thickness of char insulation layer
    float   VirginWoodThickness;  // Remaining unburned wood thickness
    float   Moisture;             // 0=dry, 1=saturated; slows/prevents ignition
};
```

#### Module
A named cluster of edges forming a sub-tree section. Used as the unit of inter-tree fire propagation.
```cpp
struct FBranchModule
{
    TArray<int32>   EdgeIndices;      // Edges belonging to this module
    float           Temperature;      // Aggregate module temperature
    float           Mass;             // Aggregate module mass
    int32           ParentModuleIndex;// For module-level fire propagation graph
};
```

#### Leaf
Lightweight billboard quad attached to a branch node.
```cpp
struct FLeafInstance
{
    int32   AttachedNodeIndex;
    float   Mass;
    float   Area;               // Shrinks proportionally to mass loss
};
```

---

### Tree Actor Components

```
ATreeActor
│
├── TArray<FTreeNode>       Nodes
├── TArray<FBranchEdge>     Edges
├── TArray<FBranchModule>   Modules
├── TArray<FLeafInstance>   Leaves
│
├── TArray<TArray<int32>>   AdjacencyBuffer     // Precomputed per-triangle neighbor indices
│                                               // Handles cross-junction nearest-vertex lookup
│
├── UProceduralMeshComponent*   BranchMesh      // One mesh section per Edge (generalized cylinder)
│                                               // Vertex custom data channels: char, glow
│
├── UInstancedStaticMeshComponent*  LeafMesh    // Billboarded quads, one per FLeafInstance
│
└── UMaterialInstanceDynamic*   BranchMaterial  // ONE shared DMI for all branch sections
                                                // Reads char + glow from vertex custom data
```

---

### Mesh Generation Pipeline (at spawn time, one-time)

```
Skeletal Graph (Nodes + Edges)
        │
        ▼
Cosserat Rod Elements
  └─ Each Edge → two endpoint particles + orientation quaternion
  └─ Quaternion computed by aligning parent director d3 with current edge direction
        │
        ▼
Procedural Mesh (UProceduralMeshComponent)
  └─ One cylinder mesh section per Edge
  └─ Generalized cylinder: start circle = Node[EdgeStart] radius/orientation
                           end circle   = Node[EdgeEnd]   radius/orientation
  └─ Uniform tessellation (e.g. 6–8 sides for thin branches, more for trunk)
  └─ At branch junctions: child mesh sections are DISCONNECTED from parent
        │
        ▼
Adjacency Index Buffer (precomputed at spawn)
  └─ Per triangle: list of neighboring triangle indices for heat diffusion
  └─ At disconnected junctions: use nearest vertex on nearest branch as neighbor
        │
        ▼
Vertex Custom Data channels set per vertex:
  └─ Channel 0: CharRatio   (0→1, drives darkening in material)
  └─ Channel 1: GlowIntensity (0→1, drives emissive in material)
```

---

### Per-Tick Simulation Update

Each tick, for each active (burning or near-fire) tree:

1. **Heat input from environment** — Forest Manager writes ambient temperature into the tree's root module
2. **Module temperature propagation** — heat walks down the module graph to adjacent modules
3. **Edge-level combustion** (for edges above ignition threshold):
   - Moisture drains: `dW/dt = -w_T * T_s`
   - Mass loss via pyrolysis: `dM/dt = -k * A * (1 - charInsulation)`
   - Char layer grows: `dH_c/dt = proportional to mass loss`
   - Virgin wood shrinks accordingly
4. **Surface heat diffusion** — temperature diffuses across adjacency buffer (discrete Laplacian)
5. **Air heat transfer** — burning edges raise ambient temperature proportional to mass loss rate (feeds back to Forest Manager grid)
6. **Vertex deformation** — vertices offset inward along normals by `H_off = H0 - H - H_c`
7. **LBS update** — vertices repositioned based on particle (node) position changes
8. **Break check** — if edge strain > threshold, remove edge, spawn detached branch rigid body
9. **Material update** — write char and glow values to vertex custom data channels
10. **Leaf update** — shrink leaf area proportional to mass loss; remove at zero mass

---

### Forest Manager (inter-tree fire spread)

```
AForestManager
│
├── Spatial grid over terrain (e.g. 1m–5m cells)
│   └─ Each cell stores: AmbientTemperature, accumulated heat from burning modules
│
├── TArray<ATreeActor*>  Trees
│
└── Per-tick:
    ├── For each burning module: raise temperature in nearby grid cells
    ├── For each tree: if root module temperature > ignition threshold → activate combustion
    └── Wind vector → applied to fluid grid, biases heat transport direction
```

---

## LOD Strategy for Forest Scale

| Distance / State     | Module Count | Simulation Detail          |
|----------------------|--------------|----------------------------|
| Active burn (close)  | 80–100       | Per-edge combustion + LBS deform |
| About to ignite      | 10–20        | Module-level temperature only |
| Distant / unignited  | 1–3          | Single temperature scalar, no mesh deform |

The Procedural Mesh and simulation detail can be swapped independently of render LOD (Nanite handles visual geometry LOD separately).

---

## Key Physical Behaviors to Implement

- **Char insulation tipping point** — mass loss rate slows after char layer forms; critical for realism, must not be skipped
- **Moisture resistance** — wet wood delays/prevents ignition; dry wood burns rapidly
- **Thin branches first** — small branches reach ignition temp faster than thick trunk (radius inversely affects ignition speed)
- **Uphill acceleration** — fire propagation speed increases exponentially with positive terrain slope
- **Wind directionality** — wind reduces total burned area by focusing fire in one direction
- **Forest density threshold** — below ~58% canopy cover, fire tends to self-extinguish
- **Firebreaks** — gaps in the module graph block inter-module heat propagation

---

## Known Limitations / Out of Scope (from papers)

- Ground fire spread (grass, litter, undergrowth) — not modeled in either paper
- Flying sparks / embers — noted as future work
- Tree resin effects — not modeled
- Leaf role in combustion — simplified/ignored in Paper 2
- Turbulence from tree-wind interaction at sub-grid scale — not fully captured

---

## UE5 Implementation Notes

- **Procedural Mesh:** `UProceduralMeshComponent` for dynamic vertex updates; consider `RuntimeMeshComponent` plugin for better performance
- **Physics:** UE5 Chaos Physics for detached branch rigid bodies
- **GPU compute:** Edge-level temperature diffusion is highly parallel — candidate for Niagara Simulation Stage or RDG compute shader
- **Niagara:** Fire/smoke/ember VFX driven by edge temperature and mass loss rate as Niagara parameters
- **Material:** Single Dynamic Material Instance per tree; char and glow driven by vertex custom data (not per-instance parameters, to avoid thousands of parameter updates)
- **Spatial grid:** UE5 built-in spatial hashing or a custom flat grid texture (readable by both CPU and GPU) for Forest Manager ambient temperature field
