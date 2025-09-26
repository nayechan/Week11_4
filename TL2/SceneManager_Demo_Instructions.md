# Scene Manager (World Outliner) Usage Guide

## Overview
The Scene Manager is now integrated into the Outliner Window and provides a comprehensive interface for managing objects in your 3D scene, similar to Unreal Engine's World Outliner.

## Features

### 1. **Object List Display**
- Shows all actors currently in the scene
- Displays actor names and types
- Auto-updates when objects are added or removed
- Hierarchical tree view (expandable/collapsible)

### 2. **Object Creation**
The toolbar now includes buttons to create new objects:
- `+ Cube` - Creates a new cube actor
- `+ Sphere` - Creates a new sphere actor  
- `+ Triangle` - Creates a new triangle actor

### 3. **Object Selection**
- **Click** on any object in the list to select it
- Selected objects will show the gizmo in the 3D viewport
- Selection syncs between Scene Manager and 3D viewport
- When you click an object in the Scene Manager, the gizmo automatically positions itself

### 4. **Context Menu (Right-click)**
Right-click on any object to access:
- **Focus in Viewport** - Centers selection and shows gizmo
- **Reset Transform** - Resets position, rotation, and scale to default
- **Reset Position** - Resets only the position to origin
- **Rename** - Rename the object (coming soon)
- **Duplicate** - Create a copy (coming soon)
- **Delete** - Remove the object from scene

### 5. **Search and Filtering**
- **Search Bar** - Type to filter objects by name
- **Selected Only** checkbox - Show only selected objects
- **Show Hidden** checkbox - Toggle visibility of hidden objects
- **Refresh** button - Manually refresh the object list

### 6. **Visibility Toggle**
- Click the eye icon (👁/🚫) next to each object to toggle visibility
- Hidden objects are shown with a crossed-out icon

## How It Works

### When You Create Objects:
1. Click one of the `+ Object` buttons in the toolbar
2. A new object is automatically created and added to the world
3. The object appears in the Scene Manager list
4. The new object is automatically selected with gizmo shown

### When You Select Objects:
1. **From Scene Manager**: Click any object → gizmo appears in 3D viewport
2. **From 3D Viewport**: Click object in 3D → Scene Manager highlights it
3. Selection state is synchronized between both interfaces

### When You Load Scenes:
1. Scene Manager automatically detects when new objects are loaded
2. The list refreshes to show all objects from the loaded scene
3. All objects become available for selection and manipulation

## Integration with Existing Systems

The Scene Manager works seamlessly with:
- **Gizmo System** - Selected objects show manipulation gizmos
- **Selection Manager** - Centralized selection state
- **Transform Widgets** - Properties panel updates for selected objects
- **Scene Loading/Saving** - Automatically reflects scene changes

## UI Layout
The Scene Manager window includes:
```
[Scene Manager Window]
├── Toolbar (Create buttons + filters)
├── Search Bar
├── Object Count Display  
├── Hierarchical Object List
│   ├── 👁 ObjectName1 (Type)
│   ├── 👁 ObjectName2 (Type)
│   └── 🚫 ObjectName3 (Type) [Hidden]
├── Context Menu (right-click)
└── Status Bar (Selected object info)
```

This provides a professional-grade scene management experience similar to modern game engines!