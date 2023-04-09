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

bool is_correct_command(char * cmd){
	const char get[] = "get ";
	for (int i=0; i<4; i++) {
		if (cmd[i]=='\0' || cmd[i]!=get[i]){
			return false;
		}
	}
	return true;
}


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
		cout << "BindDone: " << port << endl;
    }

    freeaddrinfo(ai); // All done with this

    // Listen
    if (listen(listener, 10) == -1) {
        return -1;
    }
    else {
		cout << "ListenDone: " << port << endl;
	}

    return listener;
}

// Add a new file descriptor to the set
void add_to_pfds(vector<pollfd> &pfds, int &newfd, int *fd_count, int *fd_size)
{
    pollfd* newpollfd = new pollfd;
    newpollfd->events = POLLIN;
    newpollfd->fd = newfd;
    pfds.push_back(*newpollfd);

    (*fd_count)++;
}

// Remove an index from the set
void del_from_pfds(vector<pollfd> &pfds, int i, int *fd_count)
{
    // Copy the one from the end over this one
    pfds[i] = pfds[*fd_count-1];

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
                        add_to_pfds(pfds, newfd, &fd_count, &fd_size);

                        inet_ntop(remoteaddr.ss_family, get_in_addr((sockaddr*)&remoteaddr), 
                        remoteIP, INET6_ADDRSTRLEN);
                        cout << ("Client: ") << remoteIP  << ":" << ntohs(((sockaddr_in*)&remoteaddr)->sin_port) <<endl;
                    }
                } else {
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

                        del_from_pfds(pfds, i, &fd_count);

                    } else {
                        // We got some good data from a client
                        if (!is_correct_command(filename)){
                            cout << "UnknownCmd\n";
                            cout << filename << endl;
                            cerr << "The command should be of the form: get <fileName>\n";
                            close(pfds[i].fd);
                            del_from_pfds(pfds, i, &fd_count);
                            exit(3);
                        }
                        cout << "File Requested: "<< filename + 4 << endl;
                        FILE* file = fopen(filename + 4, "rb");
                        if (file==NULL){
                            cout << "FileTransferFail\n";
                            cerr << "File does not exist" <<endl;
                            close(pfds[i].fd);
                            del_from_pfds(pfds, i, &fd_count);
                            exit(3);
                        }

                        // Now send the file
                        ssize_t num_bytes = 0;
			            ssize_t total = 0;
                        while ((num_bytes = fread(file_data, sizeof (char), CHUNK_SIZE, file)) > 0){
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
                        del_from_pfds(pfds, i, &fd_count);
                    }
                } // END handle data from client
            } // END got ready-to-read from poll()
        } // END looping through file descriptors
    } // END for(;;)--and you thought it would never end!
    
    return 0;
}

