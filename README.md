# WebServer
Web Server in C
This is a web server that handles multiple clients concurrently and supports multiple types of files (the acceptable file types are in the ws.conf file)
To compile this code use "gcc -g -Wall servert.c -o servert -lpthread"
To run this code use "./servert" and open a web browser to "http://localhost:(port number used in ws.conf file)"
