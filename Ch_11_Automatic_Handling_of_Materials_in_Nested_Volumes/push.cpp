void Push_and_Load(
    // Results
    int &incident_material_idx, int &outgoing_material_idx, bool &leaving_material,
    // Material assigned to intersected geometry
    const int material_idx,
    // Stack state
    volume_stack_element stack[STACK_SIZE], int &stack_pos)
{
    bool odd_parity = true;
    int prev_same;
    // Go down the stack and search a previous instance of the new material
    // (to check for parity and unset its topmost flag).
    for (prev_same = stack_pos; prev_same >= 0; --prev_same)
        if (material[material_idx] == material[prev_same]) {
            // Note: must have been topmost before.
            stack[prev_same].topmost = false;
            odd_parity = !stack[prev_same].odd_parity;
            break;
        }

    // Find the topmost previously entered material (occurs an odd number
    // of times, is marked topmost, and is not the new material).
    int idx;
    // idx will always be >= 0 due to camera volume.
    for (idx = stack_pos; idx >= 0; --idx)
        if ((material[stack[idx].material_idx] != material[material_idx]) &&
            (stack[idx].odd_parity && stack[idx].topmost))
            break;

    // Now push the new material idx onto the stack.
    if (stack_pos < STACK_SIZE - 1) // If too many nested volumes, do not crash.
        ++stack_pos;
    stack[stack_pos].material_idx = material_idx;
    stack[stack_pos].odd_parity = odd_parity;
    stack[stack_pos].topmost = true;

    if (odd_parity) { // Assume that we are entering the pushed material.
        incident_material_idx = stack[idx].material_idx;
        outgoing_material_idx = material_idx;
    } else { // Assume that we are exiting the pushed material.
        outgoing_material_idx = stack[idx].material_idx;
        if(idx < prev_same)
            // Not leaving an overlap,
            // since we have not entered another material yet.
            incident_material_idx = material_idx;
        else
            // Leaving the overlap,
            // indicate that this boundary should be skipped.
            incident_material_idx = outgoing_material_idx;
    }

    leaving_material = !odd_parity;
}
