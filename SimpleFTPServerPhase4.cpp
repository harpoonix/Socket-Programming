/*
** pollserver.c -- a cheezy multiperson chat server
*/

#include <iostream>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <poll.h>
#include <vector>
using namespace std;

#define CHUNK_SIZE 1024

// Get sockaddr, IPv4 or IPv6:
void *get_in_addr(sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((sockaddr_in*)sa)->sin_addr);
    }

    return &(((sockaddr_in6*)sa)->sin6_addr);
}

struct connection {
    bool getfile;
    char* filename;
};

// Checks if the command is either a GET or a PUT. Nothing else is allowed!
bool is_correct_command(char * cmd, bool is_get){
	const char get[] = "get ";
	const char put[] = "put ";
	for (int i=0; i<4; i++) {
		if (is_get){
			if (cmd[i]=='\0' || (cmd[i]!=get[i])){
				return false;
			}
		}
		else {
			if (cmd[i]=='\0' || (cmd[i]!=put[i])){
				return false;
			}
		}
	}
	return true;
}

// Return the index of the char array which is null

// Return a listening socket
int get_listener_socket(char* port)
{
    int listener;     // Listening socket descriptor
    int yes=1;        // For setsockopt() SO_REUSEADDR, below
    int rv;

    addrinfo hints, *ai, *p;

    // Get us a socket and bind it
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    if ((rv = getaddrinfo(NULL, port, &hints, &ai)) != 0) {
        fprintf(stderr, "selectserver: %s\n", gai_strerror(rv));
        exit(1);
    }
    
    for(p = ai; p != NULL; p = p->ai_next) {
        listener = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (listener < 0) { 
            continue;
        }
        
        // Lose the pesky "address already in use" error message
        setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

        if (bind(listener, p->ai_addr, p->ai_addrlen) < 0) {
            close(listener);
            continue;
        }

        break;
    }

    // If we got here, it means we didn't get bound
    if (p == NULL) {
        return -1;
    }
    else {
        cout << "Bind succesful: "<< p->ai_addr << endl;
    }

    freeaddrinfo(ai); // All done with this

    // Listen
    if (listen(listener, 10) == -1) {
        return -1;
    }
    else {
		cout << "Listen successful" << endl;
	}

	cout << "Server.... waiting for connections" <<endl;

    return listener;
}

// Add a new file descriptor to the set
void add_to_pfds(vector<pollfd> &pfds, vector <connection> &connections, int &newfd, int *fd_count, int *fd_size)
{
    pollfd* newpollfd = new pollfd;
    newpollfd->events = POLLIN;
    newpollfd->fd = newfd;
    pfds.push_back(*newpollfd);
    connection* newconnect = new connection;
    newconnect->getfile = 0;
    newconnect->filename = NULL;
    connections.push_back(*newconnect);

    (*fd_count)++;
}

// Remove an index from the set
void del_from_pfds(vector<pollfd> &pfds, vector<connection> &cnn, int i, int *fd_count)
{
    // Copy the one from the end over this one
    pfds[i] = pfds[*fd_count-1];
    cnn[i] = cnn[*fd_count - 1];

    (*fd_count)--;
}

// Main
int main(int argc, char* argv[])
{
    if (argc!=2){
		cerr << "Usage: ./SimpleFTPServerPhase3 portNum" << endl;
		exit(1);
	}
    int listener;     // Listening socket descriptor

    int newfd;        // Newly accept()ed socket descriptor
    sockaddr_storage remoteaddr; // Client address
    socklen_t addrlen;

    char file_data[CHUNK_SIZE];    // Buffer for client data

    char remoteIP[INET6_ADDRSTRLEN];

    // Start off with room for 5 connections
    // (We'll realloc as necessary)
    int fd_count = 0;
    int fd_size = 5;

    // Set up and get a listening socket
    listener = get_listener_socket(argv[1]);

    if (listener == -1) {
        fprintf(stderr, "error getting listening socket\n");
        exit(1);
    }

    vector<pollfd> pfds;
    pfds.resize(1);
    // Add the listener to set
    pfds[0].fd = listener;
    pfds[0].events = POLLIN; // Report ready to read on incoming connection
    vector<connection> connections;
    connections.resize(1);
    connections[0].getfile = 0;
    connections[0].filename = NULL;
    fd_count = 1; // For the listener

    // Main loop
    while(true) {
        int poll_count = poll(&pfds[0], fd_count, -1);

        if (poll_count == -1) {
            perror("poll");
            exit(1);
        }

        // Run through the existing connections looking for data to read
        for(int i = 0; i < fd_count; i++) {

            // Check if someone's ready to read
            if (pfds[i].revents & POLLIN) { // We got one!!

                if (pfds[i].fd == listener) {
                    // If listener is ready to read, handle new connection

                    addrlen = sizeof remoteaddr;
                    newfd = accept(listener,
                        (sockaddr *)&remoteaddr,
                        &addrlen);

                    if (newfd == -1) {
                        perror("accept");
                    } else {
                        add_to_pfds(pfds, connections, newfd, &fd_count, &fd_size);

                        printf("pollserver: new connection from %s on "
                            "socket %d\n",
                            inet_ntop(remoteaddr.ss_family,
                                get_in_addr((sockaddr*)&remoteaddr),
                                remoteIP, INET6_ADDRSTRLEN),
                            newfd);
                    }
                } else {
                    cout << "Not the listener" << endl;
                    if (!connections[i].getfile){
                        cout << "Getting command" << endl;
                        // If not the listener, we're just a regular client
                        char filename[80];
                        int nbytes = recv(pfds[i].fd, filename, 80, 0);

                        int sender_fd = pfds[i].fd;

                        if (nbytes <= 0) {
                            // Got error or connection closed by client
                            if (nbytes == 0) {
                                // Connection closed
                                printf("pollserver: socket %d hung up\n", sender_fd);
                            } else {
                                perror("recv");
                            }

                            close(pfds[i].fd); // Bye!

                            del_from_pfds(pfds, connections, i, &fd_count);

                        } else {
                            // We got some good data from a client
                            cout << "Received data from client " << filename << endl;
                            if (!is_correct_command(filename, true) && !is_correct_command(filename, false)){
                                cout << "UnknownCmd\n";
                                cout << filename << endl;
                                cerr << "The command should be of the form: get/put <fileName>\n";
                                close(pfds[i].fd);
                                del_from_pfds(pfds, connections, i, &fd_count);
                                exit(3);
                            }
                            if (filename[0]=='g'){
                                cout << "File Requested: "<< filename + 4 << endl;
                                FILE* file = fopen(filename + 4, "rb");
                                if (file==NULL){
                                    cout << "FileTransferFail\n";
                                    cerr << "File does not exist" <<endl;
                                    close(pfds[i].fd);
                                    del_from_pfds(pfds, connections, i, &fd_count);
                                    exit(3);
                                }

                                // Now send the file
                                ssize_t num_bytes = 0;
                                ssize_t total = 0;
                                while ((num_bytes = fread(file_data, sizeof (char), CHUNK_SIZE, file)) > 0){
                                    // cout << "Read "<< num_bytes << endl;
                                    int sent = send(pfds[i].fd, file_data, num_bytes, MSG_WAITALL);
                                    if (sent == -1) {
                                        cerr << "Error in sending" << endl;
                                        exit(3);
                                    }
                                    // cout << sent << endl;
                                    total+=sent;
                                }
                                cout<<"Transfer Done: "<<total <<" bytes" <<endl;
                                close(pfds[i].fd);
                                del_from_pfds(pfds, connections, i, &fd_count);
                            }
                            else if (filename[0]=='p'){
                                connections[i].getfile = true;
                                connections[i].filename = filename + 4;
                            }
                        }
                    }
                    else {
                        FILE* file = fopen(connections[i].filename, "w");
                        if (file==NULL){
                            cerr << "File " << connections[i].filename << " does not exist" << endl;
                            exit(3);
                        }
                        char buf[CHUNK_SIZE];
                        int total = 0;
                        int numbytes = 0;
                        cout << connections[i].filename << " being received" << endl;
                        while (true){
                            bzero(buf, CHUNK_SIZE);
                            numbytes = recv(pfds[i].fd, buf, CHUNK_SIZE, 0);
                            cout << numbytes << endl;
                            if (numbytes <= 0){
                                break;
                            }
                            cout << buf << endl;
                            fwrite(buf, sizeof(char), numbytes, file);
                            total+=numbytes;
                        }
                        cout << "Total "<< total << " received" << endl;
                        fclose(file);
                        close(pfds[i].fd);
                        del_from_pfds(pfds, connections, i, &fd_count);
                    }
                } // END handle data from client
            } // END got ready-to-read from poll()
        } // END looping through file descriptors
    } // END for(;;)--and you thought it would never end!
    
    return 0;
}

