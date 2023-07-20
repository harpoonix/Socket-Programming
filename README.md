# Socket-Programming : SimpleFTP
Two way communication between client and a server that supports multiple simultaneous clients, written in C++.  
The development in done in phases, from unilateral single connection to simultaneous clients and bilateral exchanges.  
A simple file transfer protocol using TCP sockets, with bandwidth limiting (still pretty fast though).  

## Phase 1: Plain File Transfer

Simple file transfer from server to client. Supports transfer of a single file, after which the connection closes.

## Phase 2: File Name from Client

The client can communicate the name of the file requested from the server. Multiple clients can connect to the server, albeit one by one (not simultaneously).

## Phase 3: Simultaneous Multiple Clients

Supports simultaneous clients, wthout employing multiple threads. Polls all connectiion effectively and manages communication via single thread.

## Phase 4: File Transfer in Both Directions

Truly bilateral file transfer, server and client can exchange file names and data in either direction. Supports simultaneity without using multiple threads (so scales well).

