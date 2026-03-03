include os

print("Content-Type: text/html\n");
print("\n");

print("Hello! This is a CGI script.");
print(f"Current value of VAR is: {os.getenv('VAR', 'not set')}");

