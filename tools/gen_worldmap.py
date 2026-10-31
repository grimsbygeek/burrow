#!/usr/bin/env python3
"""Regenerates src/ui/resources/worldmap.txt from Natural Earth's 110m land polygons.

Natural Earth is public domain: https://www.naturalearthdata.com/about/terms-of-use/

Usage: gen_worldmap.py ne_110m_land.geojson > src/ui/resources/worldmap.txt
Output: one polygon ring per line, "lon lat lon lat ..." in tenths of a degree (integers).
Antarctica is dropped so the map keeps a useful aspect ratio behind the details pane.
"""
import json
import sys

MIN_LAT = -58.0

def rings(geometry):
    polys = [geometry["coordinates"]] if geometry["type"] == "Polygon" else geometry["coordinates"]
    for poly in polys:
        yield poly[0]  # outer ring only; holes are irrelevant at this scale

def main(path):
    data = json.load(open(path))
    for feature in data["features"]:
        for ring in rings(feature["geometry"]):
            if max(lat for _, lat in ring) < MIN_LAT:
                continue
            pts, last = [], None
            for lon, lat in ring:
                p = (round(lon * 10), round(max(lat, MIN_LAT) * 10))
                if p != last:
                    pts.append(p)
                    last = p
            if len(pts) >= 3:
                print(" ".join(f"{x} {y}" for x, y in pts))

if __name__ == "__main__":
    main(sys.argv[1])
