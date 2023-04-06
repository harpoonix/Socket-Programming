/*
** client.c -- a stream socket client demo
*/

#include <iostream>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <chrono>
#include <thread>


#include <arpa/inet.h>
using namespace std;

#define CHUNK_SIZE 512 // max number of bytes we can get at once 

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(int argc, char *argv[])
{
	int sockfd, numbytes;  
	char buf[CHUNK_SIZE];
	struct addrinfo hints, *servinfo, *p;
	int rv;
	char s[INET6_ADDRSTRLEN];
    FILE* file = fopen(argv[2], "w");

	if (argc != 4) {
	    cerr << "usage: ./SimpleFTPClientPhase1 ipaddr:port filename receiveInterval\nNote: receiveInterval x specifies the rate to be 1000 bytes per x milliseconds\n";
	    exit(1);
	}

	char* hostname = strtok(argv[1], ":");
    char* port = strtok (NULL, ":");
    cout << hostname << " port " << port << endl;
    memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	if ((rv = getaddrinfo(hostname, port, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}

	// loop through all the results and connect to the first we can
	for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype,
				p->ai_protocol)) == -1) {
			perror("client: socket");
			continue;
		}

		if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			perror("client: connect");
			close(sockfd);
			continue;
		}

		break;
	}

	if (p == NULL) {
		fprintf(stderr, "client: failed to connect\n");
		return 2;
	}

	inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr),
			s, sizeof s);
	printf("client: connecting to %s\n", s);

	freeaddrinfo(servinfo); // all done with this structure

	char get[] = "get ";
	char* name = strcat(get, argv[2]);
	cout << name << endl;
	while (true){
		int fileNameSent = send(sockfd, name, 85, 0);
		if (fileNameSent==-1){
			cerr << "Could not send file name, retrying\n";
			continue;
		}
		break;
	}
	cout << "sent" << endl;
	int total = 0;
	while (true){
        bzero(buf, CHUNK_SIZE);
        numbytes = recv(sockfd, buf, CHUNK_SIZE, MSG_WAITALL);
		// cout << numbytes << "\n";
        if (numbytes<=0){
            break;
        }
        fwrite(buf, sizeof(char), numbytes, file);
		total += numbytes;
		this_thread::sleep_for(chrono::milliseconds(stoi(argv[3])));
    }

	buf[numbytes] = '\0';
	cout << "Total "<< total << endl;

	close(sockfd);

	return 0;
}
