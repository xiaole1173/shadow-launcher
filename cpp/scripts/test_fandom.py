#!/usr/bin/env python3
"""Test Fandom API to list cape textures."""
import requests, json

url = "https://minecraft.fandom.com/api.php"
params = {
    "action": "query",
    "list": "categorymembers",
    "cmtitle": "Category:Cape_textures",
    "cmtype": "file",
    "cmlimit": "max",
    "format": "json"
}
headers = {"User-Agent": "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36"}
r = requests.get(url, params=params, headers=headers, timeout=15)
print("Status:", r.status_code)
data = r.json()
if "query" in data:
    for m in data["query"]["categorymembers"]:
        print(m["title"])
    print("Total:", len(data["query"]["categorymembers"]))
else:
    print(json.dumps(data, indent=2)[:2000])
