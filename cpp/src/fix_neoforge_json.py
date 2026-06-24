import json, os
p = r'D:\latest-code\cpp\build\Release\.minecraft\versions\26.2-neoforge-26.2.0.7-beta\26.2-neoforge-26.2.0.7-beta.json'
with open(p, 'r', encoding='utf-8') as f:
    data = json.load(f)

# Add universal JAR as library
uni = {
    "name": "net.neoforged:neoforge:26.2.0.7-beta",
    "downloads": {
        "artifact": {
            "path": "net/neoforged/neoforge/26.2.0.7-beta/neoforge-26.2.0.7-beta-universal.jar",
            "url": "https://maven.neoforged.net/releases/net/neoforged/neoforge/26.2.0.7-beta/neoforge-26.2.0.7-beta-universal.jar"
        }
    }
}
data['libraries'].insert(0, uni)

with open(p, 'w', encoding='utf-8') as f:
    json.dump(data, f, indent=2, ensure_ascii=False)
print(f'Done. Libraries count: {len(data["libraries"])}')
