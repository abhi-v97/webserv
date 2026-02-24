#!/usr/bin/env python3

import cgi
import os
import time

print("<!DOCTYPE html>")
print("<html lang='en'>")
print("<head>")
print("<meta charset='UTF-8'>")
print("<meta name='viewport' content='width=device-width, initial-scale=1.0'>")
print("<title>42 Webserv</title>")
print("<style>")
print("h1 { color: DodgerBlue; }")
print("body { font-family: Arial, sans-serif; background-color: GhostWhite; color: #333; margin: 2rem; }")
print("p { background-color: Gainsboro; padding: 1rem; border: 2px solid DeepSkyBlue; border-radius: 20px;  }")
print("</style>")
print("</head>")
print("<body>")
print("<h1>Python CGI Test</h1>")
print("<p>Could potentially use this page to show server status, maybe list all server environment variables. Similar idea as a router's homepage.</p>")

current_time = time.strftime("%Y-%m-%d %H:%M:%S", time.localtime())
print(f"<h2>Server Details:</h2>")
print(f"<p><strong>Current Server Time:</strong> {current_time}</p>")

print("<h2>Request Details:</h2>")
print(f"<p><strong>Client's IP Address:</strong> {os.environ.get('REMOTE_ADDR', 'N/A')}</p>")
print(f"<p><strong>User Agent (Browser):</strong> {os.environ.get('HTTP_USER_AGENT', 'N/A')}</p>")
print(f"<p><strong>Request Method:</strong> {os.environ.get('REQUEST_METHOD', 'N/A')}</p>")

print("</body>")
print("</html>")
