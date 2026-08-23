# ARC-210 open-top tray

`arc210_open_tray.scad` is an editable OpenSCAD model for a 3D-printed case:

- Overall size: 146 mm wide x 133 mm high x 40 mm deep
- Wall thickness: 2 mm
- Floor thickness: 3 mm
- Open top
- 2 mm outside corner radius, matching the SVG outline
- Four 28 mm-deep corner mounting features: an 8 mm top pad plus a 20 mm
  wall-to-pad transition at 45° or steeper, with 6 mm-rounded inside corners
- Four 6 mm through-holes at the SVG's M1-M4 locations; each passes through
  the full 8 mm mounting pad

## Exporting an STL

1. Open `arc210_open_tray.scad` in OpenSCAD.
2. Press **F6** to render the model.
3. Choose **File → Export → Export as STL**.

The hole locations, diameter, pad thickness, and gusset depth are parameters
near the top of the SCAD file and can be changed before exporting.
