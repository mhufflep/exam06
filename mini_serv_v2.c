#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

#include <netdb.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>

int listener = 0;
int max_fd = 0;

fd_set master;
fd_set read_set;
fd_set write_set;

int users[FD_SETSIZE];
char str[100000];
char buf[100000 + 50];

void print_error(char *err_msg) {
	write(2, err_msg, strlen(err_msg));
	write(2, "\n", 1);
}

void error(char *err_msg) {
	print_error(err_msg);
	exit(1);
}

void fatal() {
	close(listener);
	error("Fatal error");
}

int get_id(int fd)
{
    for (size_t i = 0; i < FD_SETSIZE; i++) {
        if (users[i] == fd)
            return i;
    }
    return -1;
}

int		get_max_fd() 
{
    int	max = listener;

    for (size_t i = 0; i < FD_SETSIZE; i++) {
        if (users[i] > max) {
            max = users[i];
        }
    }
    return (max);
}

void	send_all(int fd, char *message)
{
    for (size_t i = 0; i < FD_SETSIZE; i++)
    {
        if (users[i] != fd && FD_ISSET(users[i], &write_set))
        {
            if (send(users[i], message, strlen(message), 0) < 0) {
                // do not need fatal();
            }
        }
    }
}

void notify(int fd, int id, char *format) {
    char msg[50] = {0};

    sprintf(msg, format, id);
    send_all(fd, msg);
}

int get_free_id() {
    for (size_t i = 0; i < FD_SETSIZE; i++) {
        if (users[i] == -1) {
            return i;
        }
    }
}

void add_user()
{
    struct sockaddr useraddr;
    socklen_t len = sizeof(useraddr);

    int user_fd;
    if ((user_fd = accept(listener, &useraddr, &len)) > 0) {

        int id = get_free_id();
        if (id != -1) {

            users[id] = user_fd;

            notify(user_fd, id, "server: user %d just arrived\n");

            max_fd = get_max_fd();
            FD_SET(user_fd, &master);
        }
    }
}

int free_id(int fd) {
    for (size_t i = 0; i < FD_SETSIZE; i++) {
        if (users[i] == fd) {
            users[i] = -1;
            return i;
        }
    }
    return -1;
}

void rm_user(int fd)
{
    int id = free_id(fd);

    if (id != -1) {

        notify(fd, id, "server: user %d just left\n");

        max_fd = get_max_fd();
        
        FD_CLR(fd, &master);
        close(fd);
    }
}

void send_by_line(int fd)
{
    int i = 0;
    int id = get_id(fd);
    
    while (str[i])
    {
        if (str[i] == '\n')
        {
            str[i] = '\0';
            sprintf(buf, "client %d: %s\n", id, str);
            send_all(fd, buf);
            bzero(&buf, strlen(buf));
        }
        i++;
    }
    bzero(&str, strlen(str));
}

uint32_t iptou(uint8_t o1, uint8_t o2, uint8_t o3, uint8_t o4)
{
    uint32_t res = 0U;
    
    res = (res | o1) << 8;
    res = (res | o2) << 8;
    res = (res | o3) << 8;
    res = res | o4;

    return res; 
}

int	is_little_endian(void) {
	int i = 1;
	return *(char *)&i;
}

uint16_t my_htons(uint16_t val) {
	if (is_little_endian()) {
		return ((val >> 8) | (val << 8));
	} else {
		return (val);
	}
}

uint32_t my_htonl(uint32_t val) {
	if (is_little_endian()) {
		return (my_htons((uint16_t)val) << 16 | my_htons(val >> 16));
	} else {
		return (val);
	}
}

int main(int ac, char **av)
{
    if (ac != 2) {
    	error("Wrong number of arguments");
    	exit(1);
    }

    struct sockaddr_in servaddr;
    bzero(&servaddr, sizeof(servaddr));

    in_port_t port = atoi(av[1]);
    servaddr.sin_family = AF_INET; 
    servaddr.sin_addr.s_addr = my_htonl(iptou(127, 0, 0, 1));
    servaddr.sin_port = my_htons(port);

    if ((listener = socket(PF_INET, SOCK_STREAM, 0)) < 0)
        fatal();

    if (bind(listener, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0)
        fatal();

    if (listen(listener, 10) < 0)
        fatal();
    
    FD_ZERO(&master);
    FD_SET(listener, &master);
    bzero(&buf, sizeof(buf));
    bzero(&str, sizeof(str));
    memset(users, -1, FD_SETSIZE);

    max_fd = listener;

    for (;;)
    {
        write_set = master;
        read_set = master;
   
        if (select(max_fd + 1, &read_set, &write_set, NULL, NULL) < 0)
            continue ;
		
        for (int fd = 0; fd <= max_fd; fd++)
        {
            if (FD_ISSET(fd, &read_set))
            {
                if (fd == listener)
                {
                    add_user();
                    break;
                }
                else if (recv(fd, str, sizeof(str), 0) <= 0) {           
                    rm_user(fd);
                    break;
                }
                else {
                    send_by_line(fd);
                }
            }
        }
    }

    return (0);
}
