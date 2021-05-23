## Public Mesh Workflow

* Mesh IDs are specified by the user when calling the `begin_*` API.
* An ID is an `int` ranging from 0 to `MAX_MESHES` (currently fixed as 4096).
* When calling `begin_*` with existing ID, the current content is deleted.
* The mesh type associated with an ID can change, but at most once per frame.

## Internal Mesh Workflow

* Vertex positions and attributes are split into two streams.
* Transient meshes all live in two buffers, while static and dynamic meshes each have its own buffers.
* Transient meshes are unindexed.
* Static and dynamic meshes are indexed before being sent to GPU.

## Mesh Frame Life Cycle

All mesh recorders and draw lists across all active threads (TODO) are cleared before the `draw` callback is called.

### Geometry Definition

1) `begin_*` is called with a user-provided ID.
2) Based on the used function, the mesh type is determined (transient / static / dynamic).
3) If the ID is already in use for a static or dynamic mesh, its associated content is scheduled for deletion (this will be relaxed in case of dynamic meshes).
4) Mesh ID-to-type info is stored.

### Mesh Submission

1) `mesh` is called, with a user-provided ID.
2) The type of the mesh is resolved from the ID.
    - Error occurrs if ID not associated with any existing mesh.
3) The mesh ID and the draw state are stored in the specific draw list.

### Geometry Update & Submission

This is done after the user-provided `draw` callback finished for a given frame.

1) Mesh buffers scheduled for deletion are deleted.
2) Mesh buffers scheduled for upload are uploaded.
    - Transient meshes are batched.
    - Static and dynamic meshes are indexed before the upload.
3) Draw lists are submitted.
