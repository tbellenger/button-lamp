"""Shared paths. ROOT = the plywood/ folder this src/ lives in.
PLANS_STL = folder with the purchased Alexjulness STLs (not included in the repo); override with SPIRAL_PLANS_DIR."""
import os
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PLANS_STL = os.environ.get('SPIRAL_PLANS_DIR', "/workspace/buttonbox/plans/SpiralLamp_Alexjulness_v1/3D Printable Parts (STL Files)/").rstrip('/') + '/'
