#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>

//SELECT

//int select(int nfds, fd_set *readfds, fd_set *writefds,
//                  fd_set *exceptfds, struct timeval *timeout);

// Allow  a  program  to  monitor multiple file descriptors, waiting until one or more of the file descriptors become
//    "ready" for some class of I/O operation. 
// A file descriptor is considered ready if it is possible to perform a corre‐
//    sponding I/O operation without blocking.


//select() can monitor only file descriptors numbers that are less than FD_SETSIZE; poll(2) does not have this limitation.




//SOCKET

//int socket(int domain, int type, int protocol);



//accept
//listen
//send
//recv
//bind
//sprintf



#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>

typedef struct		s_user 
{
	int				fd;
    int             id;
	struct s_user	*next;
}					t_user;

//t_user *g_users = NULL;

int sock_fd = 0;
int g_id = 0;

fd_set curr_sock;
fd_set cpy_read;
fd_set cpy_write;

char msg[42];
char str[42*4096];
char tmp[42*4096];
char buf[42*4096 + 42];

void print_error(char *msg) {
	
	write(2, msg, strlen(msg));
}

void error(char *err_msg) {
	print_error(err_msg);
	exit(1);
}

void fatal() 
{
	close(sock_fd);
	error("Fatal error");
}

int get_id(int fd)
{
    t_user *tmp = get_users(NULL);

    while (tmp)
    {
        if (tmp->fd == fd)
            return (tmp->id);
        tmp = tmp->next;
    }
    return (-1);
}

int		get_max_fd() 
{
	int	max = sock_fd;
    t_user *tmp = get_users(NULL);

    while (tmp) {
        if (tmp->fd > max)
            max = tmp->fd;
        tmp = tmp->next;
    }
    return (max);
}

void	send_all(int fd, char *str_req)
{
    t_user *temp = g_users;

    while (temp)
    {
        if (temp->fd != fd && FD_ISSET(temp->fd, &cpy_write))
        {
            if (send(temp->fd, str_req, strlen(str_req), 0) < 0)
                fatal();
        }
        temp = temp->next;
    }
}

t_user *new_user(int fd, int id, t_user *next) {
	t_user *new;

	if (!(new = calloc(1, sizeof(t_user))))
		fatal();

	new->id = g_id++;
    new->fd = fd;
    new->next = NULL;

	return new;
}

void lst_remove(t_user **head, int id) {
	t_user *tmp = NULL;
	t_user *rem = NULL;

	if (head && *head) {
		tmp = *head;

		if (tmp->id == id) {
        	rem = *head;
			*head = (*head)->next;
		}
    	else {
			while (tmp && tmp->next && tmp->next->id != id) {
	            tmp = tmp->next;
			}
			if (tmp->next) {
				rem = tmp->next;
	        	tmp->next = tmp->next->next;
			}
		}
		
		if (rem != NULL) {
			free(rem);
			bzero(rem, sizeof(*rem));
		}
    }
}

void lst_push(t_user **head, t_user *new) {
	t_user *tmp;
	
	if (!head) {
        *head = new;
    }
    else
    {
		tmp = *head;
        while (tmp->next) {
            tmp = tmp->next;
		}
        tmp->next = new;
    }
}

t_user *get_users(t_user *set) {
	static t_user *users;

	if (users == NULL)
		users = set;

	return users;
}

void add_user()
{
    struct sockaddr useraddr;
    socklen_t len = sizeof(useraddr);

    int user_fd;
    if ((user_fd = accept(sock_fd, &useraddr, &len)) < 0)
        fatal();

	//Adding a new user to list	
	t_user *head = get_users(NULL);
	t_user *new = new_user(user_fd, g_id, NULL);
	lst_push(&head, new);

    sprintf(msg, "server: user %d just arrived\n", g_id);

	send_all(user_fd, msg);
    FD_SET(user_fd, &curr_sock);
}

int rm_user(int fd)
{
    t_user *head = get_users(NULL);
    
    int id = get_id(fd);
	lst_remove(&head, id);

    return (id);
}

void ex_msg(int fd)
{
    int i = 0;
    int j = 0;

    while (str[i])
    {
        tmp[j] = str[i];
        j++;
        if (str[i] == '\n')
        {
            sprintf(buf, "user %d: %s", get_id(fd), tmp);
            send_all(fd, buf);
            j = 0;
            bzero(&tmp, strlen(tmp));
            bzero(&buf, strlen(buf));
        }
        i++;
    }
    bzero(&str, strlen(str));
}

int main(int ac, char **av)
{
    if (ac != 2)
    {
		error("Wrong number of arguments");
        exit(1);
    }

	//Socket addr structure creation
    struct sockaddr_in servaddr;
    bzero(&servaddr, sizeof(servaddr));

	//Setting params in the structure
    uint16_t port = atoi(av[1]);
	servaddr.sin_family = AF_INET; 
	servaddr.sin_addr.s_addr = htonl(2130706433); //127.0.0.1
	servaddr.sin_port = htons(port);

    //Create socket
	if ((sock_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        fatal();

	// Bind socket to serveraddr
    if (bind(sock_fd, (const struct sockaddr *)&servaddr, sizeof(servaddr)) < 0)
        fatal();

	// Listen socket
    if (listen(sock_fd, 0) < 0)
        fatal();
    
    FD_ZERO(&curr_sock);
    FD_SET(sock_fd, &curr_sock);
    bzero(&tmp, sizeof(tmp));
    bzero(&buf, sizeof(buf));
    bzero(&str, sizeof(str));

    while(1)
    {
		int max_fd = get_max_fd();

        cpy_write = cpy_read = curr_sock;
        if (select(max_fd + 1, &cpy_read, &cpy_write, NULL, NULL) < 0)
            continue ;
		
        for (int fd = 0; fd <= max_fd; fd++)
        {
            if (FD_ISSET(fd, &cpy_read))
            {
                if (fd == sock_fd)
                {
                    bzero(&msg, sizeof(msg));
                    add_user();
                    break;
                }
                else if (recv(fd, str, sizeof(str), 0) <= 0)
                {
					bzero(&msg, sizeof(msg));
                    sprintf(msg, "server: user %d just left\n", rm_user(fd));
                    send_all(fd, msg);
	                FD_CLR(fd, &curr_sock);
                    close(fd);
                    break;
                }
                else {
                    ex_msg(fd);
                }
            }
        }       
    }
    return (0);
}