#ifndef TESSERACT_SCENE_H
#define TESSERACT_SCENE_H

#include "tesseract_engine.h"

typedef enum {
    TESS_NODE_SPATIAL_VECTOR = 0,
    TESS_NODE_3D_MESH        = 1,
    TESS_NODE_UI_CANVAS      = 2
} TessNodeType;

typedef struct TessNode {
    uint32_t id;
    TessNodeType type;
    TessMatrix4x4 local_transform;
    TessMatrix4x4 world_transform;
    
    // Bounding Box for Hit-Testing & Raycasting
    float bbox_min[3];
    float bbox_max[3];

    struct TessNode* parent;
    struct TessNode** children;
    uint32_t child_count;
} TessNode;

// Scene Graph APIs
TessNode* tess_scene_create_node(TesseractContext* ctx, TessNodeType type);
int32_t tess_scene_attach_child(TessNode* parent, TessNode* child);
int32_t tess_scene_raycast_hit_test(TesseractContext* ctx, const float ray_origin[3], const float ray_dir[3], uint32_t* out_hit_node_id);

#endif
