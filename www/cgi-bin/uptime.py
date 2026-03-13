#!/usr/bin/env python3
import subprocess
import cgi
import html

print("Content-Type: text/html\n")

def get_server_uptime():
    try:
        cmd = "ps -C apache2 -o etime= | head -n 1"
        process_time = subprocess.check_output(cmd, shell=True).decode('utf-8').strip()
        
        system_uptime = subprocess.check_output("uptime -p", shell=True).decode('utf-8').strip()
        
        return process_time, system_uptime
    except Exception as e:
        return None, str(e)

proc_time, sys_uptime = get_server_uptime()

print(f"""
<!DOCTYPE html>
<html>
<head>
    <title>Server Status</title>
    <style>
        body {{ font-family: sans-serif; padding: 20px; line-height: 1.6; }}
        .stat-box {{ border: 1px solid #ddd; padding: 15px; border-radius: 5px; background: #f9f9f9; max-width: 400px; }}
        .label {{ font-weight: bold; color: #555; }}
        .value {{ color: #007bff; font-family: monospace; font-size: 1.2rem; }}
    </style>
</head>
<body>
    <h2>Web Server Diagnostics</h2>
    <div class="stat-box">
        <p><span class="label">Server Process Uptime:</span><br>
        <span class="value">{proc_time if proc_time else "Process not found"}</span></p>
        
        <p><span class="label">System Total Uptime:</span><br>
        <span class="value">{sys_uptime}</span></p>
    </div>
    <p><small>Last Checked: {subprocess.check_output("date", shell=True).decode('utf-8')}</small></p>
</body>
</html>
""")