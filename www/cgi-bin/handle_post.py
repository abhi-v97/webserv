#!/usr/bin/env python3
import sys
import os

# 1. Set the response header
# The server needs to know what kind of data is coming back.
print("Content-Type: text/html")
print()  # Mandatory blank line after headers

# 2. Read Environment Variables
request_method = os.environ.get("HTTP_REQUEST_METHOD", "POST")
content_length = os.environ.get("HTTP_CONTENT_LENGTH", "4")

# 3. Read the POST body from stdin
post_data = ""
if request_method == "POST" and content_length != "0":
    # It is important to read only the specified number of bytes
	post_data = sys.stdin.read(int(content_length))

# 4. Generate the HTML response
print("<html>")
print("<head><title>CGI POST Test</title></head>")
print("<body>")
print(f"<h1>Method: {request_method}</h1>")
print(f"<p><strong>Content-Length:</strong> {content_length}</p>")
print(f"<p><strong>Raw POST Data:</strong> {post_data}</p>")
print("</body>")
print("</html>")