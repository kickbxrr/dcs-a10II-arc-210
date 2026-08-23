// ARC-210 shallow open-top case
// Units: millimetres
// The XY outline matches the 146 x 133 mm faceplate template; Z is depth.

outer_width = 146;
outer_height = 133;
case_depth = 40;
wall_thickness = 2;
floor_thickness = 3;
outer_corner_radius = 2;

// Through-holes matching the M1-M4 locations in the SVG. They pass through
// the upper mounting pads from the open/top face.
mounting_hole_diameter = 6;  // SVG radius is 3 mm
mounting_feature_depth = 28; // 20 mm taper + 8 mm top pad
mounting_pad_thickness = 8;  // flat top pad; holes pass through this section
mounting_gusset_height = mounting_feature_depth - mounting_pad_thickness;
mounting_holes = [
    [5.0, 14.5],    // M1
    [140.4, 14.5],  // M2
    [5.0, 119.0],   // M3
    [140.4, 119.1]  // M4
];

// The SVG's four black mounting blocks are clipped 26 x 44 mm rounded
// rectangles whose centres sit on the corners of the faceplate.
corner_block_width = 26;
corner_block_height = 44;
corner_block_radius = 6;
corner_blocks = [
    [-13, -22],
    [133, -22],
    [-13, 111],
    [133, 111]
];
corner_outward_shifts = [
    [-11, -20],
    [11, -20],
    [-11, 20],
    [11, 20]
];

module rounded_rectangle(size, radius) {
    translate([radius, radius])
        offset(r = radius)
            square([size[0] - 2*radius, size[1] - 2*radius]);
}

module outer_profile() {
    rounded_rectangle([outer_width, outer_height], outer_corner_radius);
}

module wall_profile() {
    // The 2 mm perimeter walls continue through the full case depth.
    intersection() {
        outer_profile();
        union() {
            square([outer_width, wall_thickness]);
            translate([0, outer_height - wall_thickness])
                square([outer_width, wall_thickness]);
            square([wall_thickness, outer_height]);
            translate([outer_width - wall_thickness, 0])
                square([wall_thickness, outer_height]);
        }
    }
}

module corner_profile(p) {
    intersection() {
        outer_profile();
        translate(p)
            rounded_rectangle(
                [corner_block_width, corner_block_height],
                corner_block_radius);
    }
}

module mounting_profile() {
    // Full mounting-pad footprint, used for the top 8 mm.
    intersection() {
        outer_profile();
        union() {
            for (p = corner_blocks)
                corner_profile(p);
        }
    }
}

module mounting_gussets() {
    // Each gusset starts at the inside faces of the 2 mm walls and grows to
    // the pad over 20 mm of height. The limiting direction is exactly 45°;
    // the other direction is steeper and therefore also support-free.
    for (i = [0 : len(corner_blocks) - 1])
        hull() {
            p = corner_blocks[i];
            s = corner_outward_shifts[i];
            translate([0, 0, case_depth - mounting_feature_depth])
                linear_extrude(height = 0.01)
                    corner_profile([p[0] + s[0], p[1] + s[1]]);
            translate([0, 0, case_depth - mounting_pad_thickness])
                linear_extrude(height = 0.01)
                    corner_profile(p);
        }
}

module case_body() {
    difference() {
        union() {
            // Full outer-profile floor, full-height walls, and upper pads.
            linear_extrude(height = floor_thickness)
                outer_profile();
            linear_extrude(height = case_depth)
                wall_profile();
            translate([0, 0, case_depth - mounting_pad_thickness])
                linear_extrude(height = mounting_pad_thickness)
                    mounting_profile();
            mounting_gussets();
        }

        // Through-holes in the upper 8 mm mounting pads. The small overlap at
        // either end guarantees a clean cut through the pad surfaces.
        for (p = mounting_holes)
            translate([p[0], p[1], case_depth - mounting_pad_thickness - 0.1])
                cylinder(h = mounting_pad_thickness + 0.2,
                         d = mounting_hole_diameter,
                         $fn = 48);
    }
}

case_body();
