void Pop(
    // The "leaving material" as determined by Push_and_Load()
    const bool leaving_material,
    // Stack state
    volume_stack_element stack[STACK_SIZE], int &stack_pos)
{
    // Pop last entry.
    const scene_material &top = material[stack[stack_pos].material_idx];
    --stack_pos;
    
    // Do we need to pop two entries from the stack?
    if(leaving_material) {
        // Search for the last entry with the same material.
        int idx;
        for (idx = stack_pos; idx >= 0; --idx)
            if (material[stack[idx].material_idx] == top)
                break;

        // Protect against a broken stack
        // (from stack overflow handling in Push_and_Load()).
        if (idx >= 0)
            // Delete the entry from the list by filling the gap.
            for (int i = idx+1; i <= stack_pos; ++i)
                stack[i-1] = stack[i]; 
        --stack_pos;
    }

    // Update the topmost flag of the previous instance of this material.
    for (int i = stack_pos; i >= 0; --i)
        if (material[stack[i].material_idx] == top) {
            stack[i].topmost = true; // Note: must not have been topmost before.
            break;
        }
}
