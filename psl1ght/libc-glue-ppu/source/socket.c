#include <psl1ght/lv2/net.h>

#include <psl1ght/lv2/errno.h>

#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <stdio.h>
#include <errno.h>

int h_errno = 0;

#define FD(socket) (socket & ~SOCKET_FD_MASK)


int accept(int socket, const struct sockaddr* address, socklen_t* address_len) {
	s32 ret = accept_sprx(FD(socket), address, address_len);
	if (ret < 0)
		return (ret);
	return ret | SOCKET_FD_MASK;
}

int bind(int socket, const struct sockaddr* address, socklen_t address_len) {
	return bind_sprx(FD(socket), address, (lv2_socklen_t)address_len);
}

int connect(int socket, const struct sockaddr* address, socklen_t address_len)
{
	return connect_sprx(FD(socket), address, address_len);
}

int listen(int socket, int backlog)
{
	return listen_sprx(FD(socket), backlog);
}

int socket(int domain, int type, int protocol)
{
	s32 ret = socket_sprx(domain, type, protocol);
	if (ret < 0)
		return (ret);
	return ret | SOCKET_FD_MASK;
}

ssize_t send(int socket, const void* message, size_t length, int flags)
{
	return send_sprx(FD(socket), message, length, flags);
}

ssize_t sendto(int socket, const void* message, size_t length, int flags, const struct sockaddr* dest_addr, socklen_t dest_len)
{
	return sendto_sprx(FD(socket), message, length, flags, dest_addr, dest_len);
}

ssize_t recv(int s, void *buf, size_t len, int flags)
{
	return recv_sprx(FD(s),buf,len,flags);
}

ssize_t recvfrom(int s, void *buf, size_t len, int flags, struct sockaddr *from, socklen_t *fromlen)
{
	return recvfrom_sprx(FD(s), buf, len, flags, from, fromlen);
}

int shutdown(int socket, int how)
{
	return shutdown_sprx(FD(socket), how);
}

int closesocket(int socket)
{
	return closesocket_sprx(FD(socket));
}

int select(int nfds, fd_set* readfds, fd_set* writefds, fd_set* errorfds, struct timeval* timeout)
{
	errno = ENOSYS;
	return -1;
}


