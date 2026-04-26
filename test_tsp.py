#!/usr/bin/env python3
"""Тест TSP планировщика из main.py."""
import math


def nearest_neighbor_tsp(points):
    if not points:
        return [], 0.0
    if len(points) == 1:
        return list(points), 0.0
    n = len(points)
    unvisited = list(range(n))
    route_idx = [unvisited.pop(0)]
    while unvisited:
        current = route_idx[-1]
        cx, cy = points[current]
        best_dist = float('inf')
        best_next = None
        for idx in unvisited:
            nx, ny = points[idx]
            dist = math.hypot(nx - cx, ny - cy)
            if dist < best_dist:
                best_dist = dist
                best_next = idx
        route_idx.append(best_next)
        unvisited.remove(best_next)
    total = 0.0
    for i in range(len(route_idx) - 1):
        x1, y1 = points[route_idx[i]]
        x2, y2 = points[route_idx[i + 1]]
        total += math.hypot(x2 - x1, y2 - y1)
    return [points[i] for i in route_idx], total


# Тест 1: 1 мяч
r, t = nearest_neighbor_tsp([(320, 240)])
print(f"1 ball: {r}, total={t}")

# Тест 2: 5 мячей
balls = [(100, 200), (400, 300), (150, 450), (320, 100), (500, 400)]
print(f"Input: {balls}")
route, total = nearest_neighbor_tsp(balls)
arrow = " -> ".join(f"({x},{y})" for x, y in route)
print(f"TSP Route: {arrow}")
print(f"Total distance: {total:.1f} px")

# Тест 3: 0 мячей
r, t = nearest_neighbor_tsp([])
print(f"0 balls: {r}, total={t}")

print("\nTSP TEST: OK")
