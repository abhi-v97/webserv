#!/usr/bin/env python3
import os

print("Content-Type: text/plain")
print()

print("--- Environment Variables Received ---")
for key, value in os.environ.items():
    print(f"{key}: {value}")