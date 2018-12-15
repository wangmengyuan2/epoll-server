#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/epoll.h>

#define MAX_EVENTS	128
#define PORT		8888 
#define BUF_SIZE	1024

int main()
{
	int ret, lfd, cfd, efd;
	struct sockaddr_in server_addr, client_addr;
	socklen_t len;
	struct epoll_event temp;
	int reuse;

	lfd = socket(AF_INET, SOCK_STREAM, 0);
	if (lfd < 0) {
		printf("socket error: %s\n", strerror(errno));
		exit(1);
	}

	reuse = 1;
	ret = setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
	if (ret < 0) {
		printf("setsockopt error: %s\n", strerror(errno));
		exit(1);
	}
	
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(PORT);
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	ret = bind(lfd, (struct sockaddr *)&server_addr, sizeof(server_addr));
	if (ret < 0) {
		printf("bind error: %s\n", strerror(errno));
		exit(1);
	}

	ret = listen(lfd, 128);
	if (ret < 0) {
		printf("listen error: %s\n", strerror(errno));
		exit(1);
	}

	efd = epoll_create1(EPOLL_CLOEXEC);
	if (efd < 0) {
		printf("epoll_create1 error: %s\n", strerror(errno));
		exit(1);
	}

	temp.events = EPOLLIN;
	temp.data.fd = lfd;

	ret = epoll_ctl(efd, EPOLL_CTL_ADD, lfd, &temp);
	if (ret < 0) {
		printf("epoll_ctl error: %s\n", strerror(errno));
		exit(1);
	}

	while(1) {
		int i, nfds;
		struct epoll_event events[MAX_EVENTS];
wait:
		nfds = epoll_wait(efd, events, MAX_EVENTS, -1);
	        if (nfds < 0) {
			printf("epoll_wait error: %s\n", strerror(errno));
			continue;
		}

		for (i = 0; i < nfds; i++) {
			if (events[i].data.fd == lfd) {
				len = sizeof(client_addr);
				cfd = accept(lfd, (struct sockaddr *)&client_addr, &len);
				if (cfd < 0) {
					printf("accept error: %s\n", strerror(errno));
					continue;
				}
				printf("Connect from %s\n", inet_ntoa(client_addr.sin_addr));

				temp.events = EPOLLIN;
				temp.data.fd = cfd;
				ret = epoll_ctl(efd, EPOLL_CTL_ADD, cfd, &temp);
				if (ret < 0) {
					printf("epoll_ctl error: %s\n", strerror(errno));
					exit(1);
				}
				if (nfds == 1)
						goto wait;
			} else {
				int n, ret_fd = events[i].data.fd;
				uint8_t buf[BUF_SIZE];

				bzero(buf, BUF_SIZE);
				n = read(ret_fd, buf, BUF_SIZE);
				if (n < 0) {
					printf("read error: %s\n", strerror(errno));
					continue;
				} else if (n == 0) { // Disconnect from IP.
					struct sockaddr_in peer;
					socklen_t peer_len = sizeof(peer);

					ret = epoll_ctl(efd, EPOLL_CTL_DEL, ret_fd, NULL);
					if (ret < 0) {
						printf("epoll_ctl error: %s\n", strerror(errno));
						exit(1);
					}
					close(ret_fd);
				} else 
						write(ret_fd, buf, n);
			}
		}
	}
}

