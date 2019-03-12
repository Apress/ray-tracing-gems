struct volume_stack_element
{
    bool topmost : 1, odd_parity : 1;
    int  material_idx : 30;
};

scene_material *material;
