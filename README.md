# Client-Server in C
## Introduction
In this client-server project, a client can request a file or a set of files from the server. The
server searches for the file/s in its file directory rooted at its ~ and returns the tar.gz of the
file/files requested to the client (or an appropriate message otherwise). Multiple clients can
connect to the server from different machines and can request file/s as per the commands
listed in section 2

The server, the mirror and the client processes must run on different machines and
must communicate using sockets only.
