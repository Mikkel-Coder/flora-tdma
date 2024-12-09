from random import uniform
from math import pi, sin, cos



center = 100, 100
max_radius = 100 
max_number_of_loranodes = 2000


def random_position():
    # random angle
    angle = uniform(0, 2*pi)

    # random length
    length = uniform(0, max_radius)

    x = sin(angle) * length + center[0]
    y = cos(angle) * length + center[1]

    return x, y

for node in range(max_number_of_loranodes):
    x, y = random_position()
    print(f"**.loRaNodes[{node}].**.initialX = {x:.2f}m")
    print(f"**.loRaNodes[{node}].**.initialY = {y:.2f}m")

