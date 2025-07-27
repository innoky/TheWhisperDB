import json

N = 100_000
nodes = []

for i in range(1, N + 1):
    node = {
        "id": i,
        "title": f"Node{i}",
        "description": f"/tmp/node_{i}"
    }
    nodes.append(node)

data = {"nodes": nodes}

with open("db.json", "w") as f:
    json.dump(data, f, indent=2)

print(f"Generated db.json with {N} nodes")

