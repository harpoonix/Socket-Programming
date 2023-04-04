/*
** server.c -- a stream socket server demo
*/

#include <iostream>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <cstring>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <signal.h>
using namespace std;

#define BACKLOG 10	 // how many pending connections queue will hold
#define CHUNK_SIZE 512

bool is_correct_command(char * cmd){
	const char get[] = "get ";
	for (int i=0; i<4; i++) {
		if (cmd[i]=='\0' || cmd[i]!=get[i]){
			return false;
		}
	}
	return true;
}

void sigchld_handler(int s)
{
	(void)s; // quiet unused variable warning

	// waitpid() might overwrite errno, so we save and restore it:
	int saved_errno = errno;

	while(waitpid(-1, NULL, WNOHANG) > 0);

	errno = saved_errno;
}


// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(int argc, char* argv[])
{
	if (argc!=2){
		cerr << "Usage: ./SimpleFTPServerPhase2 portNum" << endl;
		exit(1);
	}
	int sockfd, new_fd;  // listen on sock_fd, new connection on new_fd
	struct addrinfo hints, *servinfo, *p;
	struct sockaddr_storage their_addr; // connector's address information
	socklen_t sin_size;
	struct sigaction sa;
	int yes=1;
	char s[INET6_ADDRSTRLEN];
	char file_data[CHUNK_SIZE];
	FILE* file;
	
	int rv;

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE; // use my IP

	if ((rv = getaddrinfo(NULL, argv[1], &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}

	// loop through all the results and bind to the first we can
	for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype,
				p->ai_protocol)) == -1) {
			perror("server: socket");
			continue;
		}

		if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
				sizeof(int)) == -1) {
			perror("setsockopt");
			exit(1);
		}

		if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
			perror("server: bind");
			continue;
		}

		break;
	}

	freeaddrinfo(servinfo); // all done with this structure

	if (p == NULL)  {
		cerr<< "server: failed to bind on given port\n";
		exit(2);
	}
	else {
		cout << "Bind successful" << p->ai_addr << endl;
	}

	if (listen(sockfd, BACKLOG) == -1) {
		perror("listen");
		exit(1);
	}
	else {
		cout << "Listen successful" << endl;
	}

	cout << "Server.... waiting for connections" <<endl;

	// sa.sa_handler = sigchld_handler; // reap all dead processes
	// sigemptyset(&sa.sa_mask);
	// sa.sa_flags = SA_RESTART;
	// if (sigaction(SIGCHLD, &sa, NULL) == -1) {
	// 	perror("sigaction");
	// 	exit(1);
	// }

	while(1) {  // main accept() loop
		sin_size = sizeof their_addr;
		new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
		cout << "New connection request from " << (sockaddr*) &their_addr << endl;
		if (new_fd == -1) {
			perror("accept");
			continue;
		}

		inet_ntop(their_addr.ss_family,
			get_in_addr((struct sockaddr *)&their_addr),
			s, sizeof s);
		cout << ("server: got connection from ") << s <<endl;
		char filename[80];
		int is_file = recv(new_fd, filename, 80, 0);

		if (!is_correct_command(filename)){
			cout << "UnknownCmd\n";
			cout << filename << endl;
			cerr << "The command should be of the form: get <fileName>\n";
			close(new_fd);
			exit(3);
		}
		cout << "File Requested: "<<filename + 4 << endl;
		file = fopen(filename + 4, "rb");
		if (file==NULL){
			cout << "FileTransferFail\n";
			cerr << "File does not exist" <<endl;
			close(new_fd);
			exit(3);
		}

		if (!fork()){
			ssize_t num_bytes = 0;
			ssize_t total = 0;
			
			while ((num_bytes = fread(file_data, sizeof (char), CHUNK_SIZE, file)) > 0){
				int sent = send(new_fd, file_data, num_bytes, MSG_WAITALL);
				if (sent == -1) {
					cerr << "Error in sending" << endl;
					exit(3);
				}
				// cout << sent << endl;
				total+=sent;
			}
			cout<<"Transfer Done: "<<total <<" bytes" <<endl;
		}
		sleep(1);
		close(new_fd);  // parent doesn't need this
	}

	return 0;
}
