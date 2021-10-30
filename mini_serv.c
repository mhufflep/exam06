#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

#include <netdb.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
//#include <arpa/inet.h>

typedef struct      s_user 
{
	int             fd;
    int             id;
	struct s_user   *next;
}                   t_user;

int listener = -1;
int max_id = -1;
int max_fd = -1;

fd_set master;
fd_set read_set;
fd_set write_set;

//int ids[FD_SETSIZE];
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

t_user *get_users(t_user *set) {

	static t_user *users = NULL;

	if (users == NULL)
		users = set;

	return users;
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
    int	max = listener;
    t_user *usr = get_users(NULL);

    while (usr) {
        if (usr->fd > max) {
            max = usr->fd;
        }
        usr = usr->next;
    }
    return (max);
}

void	send_all(int fd, char *message)
{
    t_user *usr = get_users(NULL);

    while (usr)
    {
        if (usr->fd != fd && FD_ISSET(usr->fd, &write_set))
        {
            if (send(usr->fd, message, strlen(message), 0) < 0) {
                // do not need fatal();
            }
        }
        usr = usr->next;
    }
}

t_user *new_user(int fd, int id, t_user *next) {

	t_user *new;

	if (!(new = calloc(1, sizeof(t_user))))
		fatal();

    new->id = id;
    new->fd = fd;
    new->next = NULL;

	return new;
}

t_user* lst_remove(t_user **head, int fd) {

	t_user *tmp = NULL;
	t_user *rem = NULL;

	if (head && *head) {
		tmp = *head;

		if (tmp->fd == fd) {
        	rem = *head;
			*head = (*head)->next;
		}
    	else {
			while (tmp && tmp->next && tmp->next->fd != fd) {
	            tmp = tmp->next;
			}
			if (tmp->next) {
				rem = tmp->next;
	        	tmp->next = tmp->next->next;
			}
		}
    }
    return rem;
}

void lst_push(t_user **head, t_user *new) {

	t_user *tmp;
	
    if (head) {
    	if (*head == NULL) {
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
}

void notify(int fd, int id, char *format) {
    char msg[50] = {0};

    sprintf(msg, format, id);
    send_all(fd, msg);
}

void add_user()
{
    struct sockaddr useraddr;
    socklen_t len = sizeof(useraddr);

    int user_fd;
    if ((user_fd = accept(listener, &useraddr, &len)) < 0) {
        // do not need fatal();
    }

	t_user *head = get_users(NULL);
	t_user *new = new_user(user_fd, max_id, NULL);
	lst_push(&head, new);
    
    notify(user_fd, max_id, "server: user %d just arrived\n");

    max_fd = get_max_fd();
    max_id++;
  
    FD_SET(user_fd, &master);
}

void rm_user(int fd)
{
    t_user *head = get_users(NULL);
	t_user *rem = lst_remove(&head, fd);

    if (rem != NULL) {

        notify(rem->fd, rem->id, "server: user %d just left\n");

        max_fd = get_max_fd();
        
        FD_CLR(rem->fd, &master);
        close(rem->fd);
        free(rem);
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


int main(int ac, char **av)
{
    if (ac != 2)
    {
    	error("Wrong number of arguments");
    	exit(1);
    }

    struct sockaddr_in servaddr;
    bzero(&servaddr, sizeof(servaddr));

	//Setting params in the structure
    in_port_t port = atoi(av[1]);
	servaddr.sin_family = AF_INET; 
	servaddr.sin_addr.s_addr = htonl(iptou(127, 0, 0, 1)); // use inet_addr() or inet_pton() instead
	servaddr.sin_port = htons(port);

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

    max_fd = listener;

    t_user head;

    head.fd = max_fd++;
    head.id = max_id++;
    head.next = NULL;
    get_users(&head);
    
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