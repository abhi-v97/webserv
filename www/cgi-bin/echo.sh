#!/bin/sh
# print all env variables
printf "Content-Type: text/html\r\n"
printf "\r\n"
echo "Environment variables:"
echo "<pre>"
env | sort
echo "</pre>"