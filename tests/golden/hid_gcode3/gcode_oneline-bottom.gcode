(Created by G-code exporter)
(Fri Nov  2 00:02:58 2012)
(Units: mm)
(Board size: 50.80 x 25.40 mm)
(Accuracy 1200 dpi)
(Tool diameter: 0.200000 mm)
#100=2.000000  (safe Z)
#101=-0.050000  (cutting depth)
#102=25.000000  (plunge feedrate)
#103=50.000000  (feedrate)
(with predrilling)
(---------------------------------)
G17 G21 G90 G64 P0.003 M3 S3000 M7
G0 Z#100
(polygon 1)
G0 X27.813000 Y13.546667    (start point)
G1 Z#101 F#102
F#103
G1 X27.664833 Y13.504333
G1 X27.495500 Y13.419667
G1 X27.347333 Y13.313833
G1 X7.450667 Y13.292667
G1 X7.260167 Y13.208000
G1 X7.090833 Y13.038667
G1 X7.006167 Y12.848167
G1 X7.006167 Y12.530667
G1 X7.090833 Y12.340167
G1 X7.260167 Y12.170833
G1 X7.450667 Y12.086167
G1 X27.347333 Y12.065000
G1 X27.495500 Y11.959167
G1 X27.728333 Y11.853333
G1 X28.130500 Y11.853333
G1 X28.448000 Y12.001500
G1 X28.638500 Y12.213167
G1 X28.765500 Y12.488333
G1 X28.765500 Y12.890500
G1 X28.638500 Y13.165667
G1 X28.448000 Y13.377333
G1 X28.130500 Y13.525500
G1 X27.813000 Y13.546667
G0 Z#100
(polygon end, distance 45.39)
(predrilling)
F#102
G81 X27.940000 Y12.700000 Z#101 R#100
(1 predrills)
(milling distance 45.39mm = 1.79in)
M5 M9 M2