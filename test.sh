printf "POST /addr HTTP/1.1\r\nHost: idk\nContent-length:28\r\n\r\nTHIS IS THE BODY MESSAGE\r\n\r\n" | netcat -C -v localhost 8080
