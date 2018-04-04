#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <semaphore.h>
#include <assert.h>
#include <netinet/in.h>
#include <string>
#include <vector>
#include <signal.h>
#include <sys/stat.h>
#include <sys/mman.h>

const char* path = "/Users/mesher/tcp/mapped.dat";

#define PERMITIONS (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP)
#define MAX_MESSAGE_SIZE 1000
#define TCP_PORT_NUMBER 51000

pid_t master;
pid_t* processes;

int last_connection;
int* connections;
struct sockaddr_in* clients;

int num_connected = 0;

int socket_fd;
int new_socket_fd;

std::vector<sem_t*> semaphores;
sem_t* mutex;
sem_t* for_exit;
sem_t* stop;

int COMPUTERS_AMOUNT;
int height;
int width;
int arrayLen;

bool early_ctr_c;
bool before_fork;

struct _Field {
    int* width;
    int* height;
    char* field;

    _Field(void* start) {
        width = static_cast<int*>(start);
        height = width + 1;
        field = static_cast<char*>(start) + 2 * sizeof(int) / sizeof(char);
    }
};

int field_size;
int field_fd;
void* field_address;
_Field* Field;

int old_field_fd;
void* old_field_address;
_Field* OldField;

struct Utility {
    char* barrier;
    char* alive;
    char* different;

    Utility(void* start) {
        barrier = static_cast<char*>(start) ;
        alive = barrier + 1;
        different = alive + 1;
    }
};

int utility_size;
int utility_fd;
void* utility_address;
Utility* Util;

int for_up_fd;
void* for_up_address;
struct sockaddr_in* for_up;

void Client(int connection_num);
void close_connection(int socket_descriptor);
void finish();
void getResourcers();
void handler(int nsig);
void siguser_handler(int nsig);
void init();
void init_memory();
void init_sem();
void copy(_Field* to, _Field* from);
void _write(int x, int y, char state, char* destination);
char _read(int x, int y, char* source);
void print(char* source);
void start_state(char* field);

int main() {
    int ret;

    init();

    socket_fd = -1;
    new_socket_fd = -1;
	struct protoent *protocol_record = NULL;
	struct sockaddr_in server_address;
	socklen_t client_address_len;

    early_ctr_c = false;
    before_fork = true;

	protocol_record=getprotobyname("tcp");

	socket_fd = socket(PF_INET, SOCK_STREAM, protocol_record->p_proto);		if (socket_fd < 0) {std::cerr << "socket failure: " << errno << '\n'; exit(0);}
	bzero(&server_address, sizeof(server_address));
	server_address.sin_family=AF_INET;
	server_address.sin_port=htons(TCP_PORT_NUMBER);
	server_address.sin_addr.s_addr=inet_addr("127.0.0.1");

    int enable = 1;
    ret = setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int));     if (ret < 0) {std::cerr << "socket reuse failure: " << strerror(errno) << '\n'; close(socket_fd); exit(0);}

    if (early_ctr_c) {exit(0);}
	ret = bind(socket_fd, (const struct sockaddr *) &server_address, sizeof(struct sockaddr_in));
	if (ret < 0) {std::cerr << "bind failed: " << strerror(errno) << '\n'; close(socket_fd); exit(0);}

    if (early_ctr_c) {exit(0);}
	ret = listen(socket_fd, COMPUTERS_AMOUNT);		if (ret < 0) {std::cerr << "listen failed " << strerror(errno) << '\n'; close(socket_fd); exit(0);}
	
	connections = new int[COMPUTERS_AMOUNT];
	for(int i = 0;i < COMPUTERS_AMOUNT;++i) 
		connections[i] = -1;
	last_connection = 0;
	struct sockaddr_in tmp_addr;
	client_address_len = sizeof(struct sockaddr_in);
    struct sockaddr_in* clients = new struct sockaddr_in[COMPUTERS_AMOUNT];

    if (early_ctr_c) {close_connection(socket_fd); exit(0);}
	while (last_connection < COMPUTERS_AMOUNT) {
        if (early_ctr_c) {
            if (before_fork) {
                close_connection(socket_fd); exit(0);
            } else {
                sem_post(for_exit);
                break;
            }
        }
		new_socket_fd = accept(socket_fd, (struct sockaddr *) &clients[last_connection], &client_address_len);	
		if (new_socket_fd < 0) {
			std::cerr << "accept failed: " << strerror(errno) << '\n'; 
			continue;
		}

		connections[last_connection] = new_socket_fd;
        before_fork = false;
		pid_t ret = fork(); 
		if (ret < 0) {
			std::cerr << "fork failed, num: " << last_connection << '\n';
            sem_post(for_exit);
			break;
		} else if (ret == 0) {
			Client(last_connection);
		} else {
			processes[last_connection] = ret;
		}
        ++last_connection;
		close(new_socket_fd);
        new_socket_fd = -1;
	}
    close_connection(socket_fd);
    std::cout << "closed main listen\n";
    sem_wait(for_exit);
    finish();
	return 0;
}

void Client(int connection_num) {
    new_socket_fd = -1;
    getResourcers();
    int n;
    //std::cout << "\nsin_port " << ntohl(clients[last_connection].sin_port) << "\nsin_addr.s_addr " << ntohl(clients[last_connection].sin_addr.s_addr) << '\n';    
    int client_socket = connections[connection_num];
    //arraylen
    int arrayLenNetwork = htons(arrayLen);
    n = write(client_socket, &arrayLenNetwork, sizeof(arrayLenNetwork));  if (n < 0) {std::cerr << "write failed " << strerror(errno) << '\n'; close_connection(client_socket); sem_post(for_exit); sem_wait(stop);}
    if (n == 0) {std::cout << "closing\n"; close(client_socket); connections[connection_num] = -1; sem_post(for_exit); sem_wait(stop);}
    std::cout << "arrayLen: " << arrayLen << '\n';
    //width
    int widthNetwork = htons(width);
    n = write(client_socket, &widthNetwork, sizeof(widthNetwork));  if (n < 0) {std::cerr << "write failed " << strerror(errno) << '\n'; close_connection(client_socket); sem_post(for_exit); sem_wait(stop);}
    if (n == 0) {std::cout << "closing\n"; close(client_socket); connections[connection_num] = -1; sem_post(for_exit); sem_wait(stop);}
    std::cout << "width: " << width << '\n';
    //connection_number
    int connection_num_network = htons(connection_num);
    n = write(client_socket, &connection_num_network, sizeof(connection_num_network));  if (n < 0) {std::cerr << "write failed connection_num" << strerror(errno) << '\n'; close_connection(client_socket); sem_post(for_exit); sem_wait(stop);}
    if (n == 0) {std::cout << "closing\n"; close(client_socket); connections[connection_num] = -1; sem_post(for_exit); sem_wait(stop);}
    std::cout << "connection num: " << connection_num << '\n';
    //piece of field
    for (int i = 0;i < arrayLen;++i) {
        n = write(client_socket, Field->field + arrayLen * connection_num + i, 1);  if (n < 0) {std::cerr << "write failed " << strerror(errno) << '\n'; close_connection(client_socket); sem_post(for_exit); sem_wait(stop);}
        if (n == 0) {std::cout << "closing\n"; close(client_socket); connections[connection_num] = -1; sem_post(for_exit); sem_wait(stop);}
    }
    //print(Field->field);
    //up neighbour
    int up_connection_num = (connection_num + 1) % COMPUTERS_AMOUNT;
    std::cout << "up connection num: " << up_connection_num << '\n';
    bzero(&for_up[up_connection_num], sizeof(struct sockaddr_in));
    n = read(client_socket, &for_up[up_connection_num].sin_addr.s_addr, sizeof(for_up[up_connection_num].sin_addr.s_addr));    if (n < 0) {std::cerr << "read failed for up" << strerror(errno) << '\n'; close_connection(client_socket); sem_post(for_exit); sem_wait(stop);}
    if (n == 0) {std::cout << "closing\n"; close(client_socket); connections[connection_num] = -1; sem_post(for_exit); sem_wait(stop);}
    n = read(client_socket, &for_up[up_connection_num].sin_port, sizeof(for_up[up_connection_num].sin_port));    if (n < 0) {std::cerr << "read failed for up" << strerror(errno) << '\n'; close_connection(client_socket); sem_post(for_exit); sem_wait(stop);}
    if (n == 0) {std::cout << "closing\n"; close(client_socket); connections[connection_num] = -1; sem_post(for_exit); sem_wait(stop);}
    std::cout << "ip: " << ntohl(for_up[up_connection_num].sin_addr.s_addr) << ' ' << "port: " << ntohs(for_up[up_connection_num].sin_port) << '\n';
    sem_post(semaphores[up_connection_num]);
    sem_wait(semaphores[connection_num]);
    std::cout << connection_num << "gets address to connect to " << ((connection_num == 0) ? COMPUTERS_AMOUNT - 1 : connection_num - 1) << '\n'; 
    n = write(client_socket, &for_up[connection_num].sin_addr.s_addr, sizeof(for_up[connection_num].sin_addr.s_addr));  if (n < 0) {std::cerr << "write failed for up" << strerror(errno) << '\n'; close_connection(client_socket); sem_post(for_exit); sem_wait(stop);}
    if (n == 0) {std::cout << "closing\n"; close(client_socket); connections[connection_num] = -1; sem_post(for_exit); sem_wait(stop);}
    n = write(client_socket, &for_up[connection_num].sin_port, sizeof(for_up[connection_num].sin_port));  if (n < 0) {std::cerr << "write failed for up" << strerror(errno) << '\n'; close_connection(client_socket); sem_post(for_exit); sem_wait(stop);}
    if (n == 0) {std::cout << "closing\n"; close(client_socket); connections[connection_num] = -1; sem_post(for_exit); sem_wait(stop);}
    std::cout << "ip: " << ntohl(for_up[connection_num].sin_addr.s_addr) << ' ' << "port: " << ntohs(for_up[connection_num].sin_port) << '\n';
    for (int shift = 0;shift < COMPUTERS_AMOUNT * arrayLen;++shift)
        *(Field->field + shift) = 0;
    while(true) {
        //field
        for (int i = 0;i < arrayLen;++i) {
            n = read(client_socket, Field->field + arrayLen * connection_num + i, 1); if (n < 0) {std::cerr << "read failed from client field" << connection_num << ' ' << strerror(errno) << '\n'; close_connection(client_socket); sem_post(for_exit); sem_wait(stop);}
            if (n == 0) {std::cout << "closing read from client field" << connection_num << '\n'; close(client_socket); connections[connection_num] = -1; sem_post(for_exit); sem_wait(stop);}
        }
        //alive
        char _alive = 0;
        n = read(client_socket, &_alive, 1); if (n < 0) {std::cerr << "read failed from client alive" << connection_num << ' ' << strerror(errno) << '\n'; close_connection(client_socket); sem_post(for_exit); sem_wait(stop);}
        if (n == 0) {std::cout << "closing read from client alive" << connection_num << '\n'; close(client_socket); connections[connection_num] = -1; sem_post(for_exit); sem_wait(stop);}
        //different
        char _different = 0;
        n = read(client_socket, &_different, 1); if (n < 0) {std::cerr << "read failed from client _different" << connection_num << ' ' << strerror(errno) << '\n'; close_connection(client_socket); sem_post(for_exit); sem_wait(stop);}
        if (n == 0) {std::cout << "closing read from client _different" << connection_num << '\n'; close(client_socket); connections[connection_num] = -1; sem_post(for_exit); sem_wait(stop);}

        sem_wait(mutex);
        if (_alive) *(Util->alive) = 1;
        if (_different) *(Util->different) = 1;
        *(Util->barrier) = *(Util->barrier) + 1;
        if (*(Util->barrier) == COMPUTERS_AMOUNT) {
            copy(OldField, Field);
            print(Field->field);
            if (*(Util->alive) && *(Util->different)) {
                for (int i = 0; i < COMPUTERS_AMOUNT;++i) 
                    sem_post(semaphores[i]);
                *(Util->barrier) = 0;
            } else {
                sem_post(for_exit);
                sem_wait(stop);
            }
        }
        sem_post(mutex);
        sem_wait(semaphores[connection_num]);
        sleep(1);
        char ready = 1;
        n = write(client_socket, &ready, sizeof(char)); if (n < 0) {std::cerr << "write failed ready to client " << connection_num << ' ' << strerror(errno) << '\n'; close_connection(client_socket); sem_post(for_exit); sem_wait(stop);}
        if (n == 0) {std::cout << "closing write ready to client " << connection_num << '\n'; close(client_socket); connections[connection_num] = -1; sem_post(for_exit); sem_wait(stop);}
        std::cout << "step\n";
    }
}

void finish() {
    int n;
    int ret;
    std::cout << "finish\n";
    for (int i = 0;i < COMPUTERS_AMOUNT;++i) {
        char terminate_symbol = '#';
        if (connections[i] < 0)
            continue;
        n = write(connections[i], &terminate_symbol, sizeof(char)); if (n < 0) {std::cerr << "finish write failed to client " << i << ' ' << strerror(errno) << '\n';}
        if (n == 0) {std::cout << "alerady closed " << i << '\n';}
    }
        
    for (int i = 0;i < last_connection;++i)
        if (processes[i]) {
            ret = kill(processes[i], SIGTERM); if (ret < 0) {std::cout << "terminate failed for " << i << ' ' << strerror(errno) << '\n';}
            std::cout << "terminated " << i << '\n';
        }
    for (int i = 0;i < last_connection;++i)
        if (processes[i]) {
            ret = waitpid(processes[i], NULL, 0); if (ret < 0) {std::cout << "waitpid failed for " << i << ' ' << strerror(errno) << '\n';}
            std::cout << "killed" << i << '\n';
        }
    close_connection(new_socket_fd);
    for (int i = 0;i < COMPUTERS_AMOUNT;++i)
        close_connection(connections[i]);
    copy(Field, OldField);
    print(Field->field);
    for (int i = 0; i < COMPUTERS_AMOUNT; ++i) {
        std::string sem_name = "/sem" + std::to_string(i);
        ret = sem_unlink(sem_name.c_str()); if (ret < 0) std::cerr << "sem_unlink failed for " << i << "on exit\n";
    }
    ret = sem_unlink("/for_exit"); if (ret < 0) std::cerr << "sem_unlink failed for /for_exit on exit\n";

    ret = sem_unlink("/stop"); if (ret < 0) std::cerr << "sem_unlink failed for /stop on exit\n";

    ret = sem_unlink("/mutex"); if (ret < 0) std::cerr << "sem_unlink failed for /mutex on exit\n";
    std::cout << "before exit\n";
    exit(0);
}


void handler(int nsig) {
    std::cout << "sigint\n";
    if (before_fork)
        early_ctr_c = true;
    else
        sem_post(for_exit);
}

void siguser_handler(int nsig) {
    std::cout << "siguser\n";
    sem_post(for_exit);
}

void init() {
    int ret;
    int n;

    struct sigaction act;
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;
    act.sa_handler = handler;

    ret = sigaction(SIGINT, &act, NULL);    if (ret < 0) {std::cerr << "sigaction failed\n"; exit(0);}

    struct sigaction act1;
    sigemptyset(&act1.sa_mask);
    act1.sa_flags = 0;
    act1.sa_handler = siguser_handler;

    ret = sigaction(SIGUSR1, &act1, NULL);    if (ret < 0) {std::cerr << "sigaction failed\n"; exit(0);}

    std::cout << "computers: ";
    std::cin >> COMPUTERS_AMOUNT;
    processes = new pid_t[COMPUTERS_AMOUNT]; //childs

    master = getpid();

    std::cout << "use old data or input new one(n): ";
    std::string answer;
    std::cin >> answer;

    if (answer == "n") {
        std::cout << "height: ";
        std::cin >> height;
        std::cout << "width: ";
        std::cin >> width;
        assert(COMPUTERS_AMOUNT <= height && height % COMPUTERS_AMOUNT == 0);
        arrayLen = (height / COMPUTERS_AMOUNT) * width;

        field_fd = open(path, O_RDWR, 0666);    if (field_fd < 0) {std::cerr << "open failed, " << strerror(errno) << "\n"; exit(0);}
        field_size = 2 * sizeof(int) + COMPUTERS_AMOUNT * arrayLen * sizeof(char);
        ret = ftruncate(field_fd, field_size);      
        if (ret < 0) {
            std::cerr << "ftruncate failed field, " << strerror(errno) << "\n";
            write(field_fd, 0, field_size);
        }
        field_address = mmap(NULL, field_size, PROT_READ | PROT_WRITE, MAP_SHARED, field_fd, 0);    if (field_address == MAP_FAILED) {std::cerr << "first map failed, " << strerror(errno) << "\n"; exit(0);}
        Field = new _Field(field_address);
        *(Field->width) = width;
        *(Field->height) = height;
        for (int shift = 0;shift < COMPUTERS_AMOUNT * arrayLen;++shift)
            *(Field->field + shift) = 0;
       
        start_state(Field->field);
        print(Field->field);
    } else {
        field_fd = open(path, O_RDWR, 0666);    if (field_fd < 0) {std::cerr << "open failed, " << strerror(errno) << "\n"; exit(0);}
        struct stat st;
        fstat(field_fd, &st);
        field_size = st.st_size;
        field_address = mmap(NULL, field_size, PROT_READ | PROT_WRITE, MAP_SHARED, field_fd, 0);    if (field_address == MAP_FAILED) {std::cerr << "first map failed, " << strerror(errno) << "\n"; exit(0);}
        Field = new _Field(field_address);
        width = *(Field->width);
        height = *(Field->height);
        assert(COMPUTERS_AMOUNT <= height || height % COMPUTERS_AMOUNT == 0);
        arrayLen = (height / COMPUTERS_AMOUNT) * width;
        
        print(Field->field);
    }
    init_memory();
    copy(OldField, Field);
    init_sem();

}


void init_memory() {
    //old_field
    int ret;
    old_field_fd = shm_open("/shared_memory_old", O_CREAT | O_RDWR, PERMITIONS);     
    if (old_field_fd < 0) {
        std::cerr << "shm_open failed old_field, " << strerror(errno) << '\n';
        ret = shm_unlink("/shared_memory_old"); 
        if (ret < 0) {
            std::cerr << "shm_unlink failed old field, " << strerror(errno) << "\n"; 
            exit(0);
        }
        old_field_fd = shm_open("/shared_memory_old", O_CREAT | O_RDWR, PERMITIONS);
    }
    ret = ftruncate(old_field_fd, field_size);      
    if (ret < 0) {
        std::cerr << "ftruncate failed old_field, " << strerror(errno) << "\n";
        write(old_field_fd, 0, field_size);
    }
    old_field_address = mmap(NULL, field_size, PROT_READ | PROT_WRITE, MAP_SHARED, old_field_fd, 0);    if (old_field_address == MAP_FAILED) {std::cerr << "first map failed, " << strerror(errno) << "\n"; exit(0);}
    OldField = new _Field(old_field_address);
    copy(OldField, Field);
    //utility
    utility_fd = shm_open("/shared_memory_utility", O_CREAT | O_RDWR, PERMITIONS);       
    if (utility_fd < 0) {
        std::cerr << "shm_open failed, " << strerror(errno) << '\n';
        ret = shm_unlink("/shared_memory_utility");
        if (ret < 0) {
            std::cerr << "shm_unlink failed utility " << strerror(errno) << '\n'; 
            exit(0);
        }
        utility_fd = shm_open("/shared_memory_utility", O_CREAT | O_RDWR, PERMITIONS); 
    }
    utility_size = 3 * sizeof(char);
    ret = ftruncate(utility_fd, utility_size);   
    if (ret < 0) {
        std::cerr << "ftruncate failed utility " << strerror(errno) << "\n";
        write(utility_fd, 0, utility_size);
    }
    utility_address = mmap(NULL, utility_size, PROT_READ | PROT_WRITE, MAP_SHARED, utility_fd, 0);       if (utility_address == MAP_FAILED) {std::cerr << "first map failed, " << strerror(errno) << "\n"; exit(0);}
    Util = new Utility(utility_address);
    *(Util->alive) = 1;
    *(Util->different) = 1;
    *(Util->barrier) = 0;

    for_up_fd = shm_open("/shared_memory_for_up", O_CREAT | O_RDWR, PERMITIONS);     
    if (for_up_fd < 0) {
        std::cerr << "shm_open failed for up, " << strerror(errno) << '\n';
        ret = shm_unlink("/shared_memory_for_up"); 
        if (ret < 0) {
            std::cerr << "shm_unlink failed for up, " << strerror(errno) << "\n"; 
            exit(0);
        }
        for_up_fd = shm_open("/shared_memory_for_up", O_CREAT | O_RDWR, PERMITIONS);
    }
    ret = ftruncate(for_up_fd, sizeof(struct sockaddr_in) * COMPUTERS_AMOUNT);      
    if (ret < 0) {
        std::cerr << "ftruncate failed old_field, " << strerror(errno) << "\n";
        write(for_up_fd, 0, sizeof(struct sockaddr_in) * COMPUTERS_AMOUNT);
    }
    for_up_address = mmap(NULL, sizeof(struct sockaddr_in) * COMPUTERS_AMOUNT, PROT_READ | PROT_WRITE, MAP_SHARED, for_up_fd, 0);    if (for_up_address == MAP_FAILED) {std::cerr << "map failed for up " << strerror(errno) << "\n"; exit(0);}
    for_up = static_cast<struct sockaddr_in*>(for_up_address);
}


void init_sem() {
    //for_exit
    for_exit = sem_open("/for_exit", O_CREAT | O_EXCL, PERMITIONS, 0);
    if (for_exit == SEM_FAILED) {
        int ret = sem_unlink("/for_exit");     if (ret < 0) std::cerr << strerror(errno) << " sem_unlink failed for /for_exit\n";
        for_exit = sem_open("/for_exit", O_CREAT | O_EXCL, PERMITIONS, 0);
    }
    //stop
    stop = sem_open("/stop", O_CREAT | O_EXCL, PERMITIONS, 0);
    if (stop == SEM_FAILED) {
        int ret = sem_unlink("/stop");     if (ret < 0) std::cerr << strerror(errno) << " sem_unlink failed for /stop\n";
        stop = sem_open("/stop", O_CREAT | O_EXCL, PERMITIONS, 0);
    }
    //mutex
    mutex = sem_open("/mutex", O_CREAT | O_EXCL, PERMITIONS, 1);
    if (mutex == SEM_FAILED) {
        int ret = sem_unlink("/mutex");     if (ret < 0) std::cerr << strerror(errno) << " sem_unlink failed for /mutex\n";
        mutex = sem_open("/mutex", O_CREAT | O_EXCL, PERMITIONS, 0);
    }
    //clients
    semaphores.resize(COMPUTERS_AMOUNT);
    for (int i = 0; i < COMPUTERS_AMOUNT; ++i) {
        std::string sem_name = "/sem" + std::to_string(i);
        semaphores[i] = sem_open(sem_name.c_str(), O_CREAT | O_EXCL, PERMITIONS, 0);
        if (semaphores[i] == SEM_FAILED) {
            int ret = sem_unlink(sem_name.c_str());     if (ret < 0) std::cerr << strerror(errno) << " sem_unlink failed for " << i << '\n';
            semaphores[i] = sem_open(sem_name.c_str(), O_CREAT | O_EXCL, PERMITIONS, 0);
        }
    }
}

void getResourcers() {
    for_exit = sem_open("/for_exit", O_RDWR); if (for_exit == SEM_FAILED) {std::cerr <<  strerror(errno) << " in child sem_open failure for /for_exit\n"; kill(master, SIGUSR1); exit(0);}
    
    stop = sem_open("/stop", O_RDWR); if (stop == SEM_FAILED) {std::cerr <<  strerror(errno) << " in child sem_open failure for /stop\n"; sem_post(for_exit); exit(0);}
    
    mutex = sem_open("/mutex", O_RDWR); if (mutex == SEM_FAILED) {std::cerr <<  strerror(errno) << " in child sem_open failure for /mutex\n"; sem_post(for_exit); sem_wait(stop);}

    field_address = mmap(NULL, field_size, PROT_READ | PROT_WRITE, MAP_SHARED, field_fd, 0); if (field_address == MAP_FAILED) {std::cerr << "map failed for child " << strerror(errno) << "\n"; sem_post(for_exit); sem_wait(stop);}
    Field = new _Field(field_address);

    old_field_address = mmap(NULL, field_size, PROT_READ | PROT_WRITE, MAP_SHARED, old_field_fd, 0); if (old_field_address == MAP_FAILED) {std::cerr << "child old field map failed, " << strerror(errno) << "\n"; sem_post(for_exit); sem_wait(stop);}
    OldField = new _Field(old_field_address);

    utility_address = mmap(NULL, utility_size, PROT_READ | PROT_WRITE, MAP_SHARED, utility_fd, 0); if (utility_address == MAP_FAILED) {std::cerr << "child old field map failed, " << strerror(errno) << "\n"; sem_post(for_exit); sem_wait(stop);}
    Util = new Utility(utility_address);

    for_up_address = mmap(NULL, sizeof(struct sockaddr_in) * COMPUTERS_AMOUNT, PROT_READ | PROT_WRITE, MAP_SHARED, for_up_fd, 0); if (for_up_address == MAP_FAILED) {std::cerr << "for up map failed, " << strerror(errno) << "\n"; sem_post(for_exit); sem_wait(stop);}
    for_up = static_cast<struct sockaddr_in*>(for_up_address);

    semaphores.resize(COMPUTERS_AMOUNT);
    for (int i = 0; i < COMPUTERS_AMOUNT; ++i) {
        std::string sem_name = "/sem" + std::to_string(i);
        sem_t *sem = sem_open(sem_name.c_str(), O_RDWR); if (sem == SEM_FAILED) {std::cerr << strerror(errno) << " in child sem_open failure for " << i << '\n'; sem_post(for_exit); sem_wait(stop);}
        semaphores[i] = sem;
    }
}

void close_connection(int socket_descriptor) {
    if (socket_descriptor >= 0) {
        int n;
        int ret;
        int err = 1;
        socklen_t len = sizeof(err);
        ret = getsockopt(socket_descriptor, SOL_SOCKET, SO_ERROR, (char *)&err, &len);
        if (ret < 0)
            std::cout << "getsockopt failed" << strerror(errno) << '\n';

        n = shutdown(socket_descriptor, SHUT_RDWR); 
        if (n < 0)
            std::cout << "shutdown failed " << strerror(errno) << '\n';
        else {
            int buffer;
            n = read(socket_descriptor, &buffer, sizeof(buffer)); 
            if (n < 0)
                std::cout << "closing read failed " << strerror(errno) << '\n';
        }
        close(socket_descriptor);
        socket_descriptor = -1;
        std::cout << "closed socket\n";
    }
}

void copy(_Field* to, _Field* from) {
    for (int shift = 0;shift < COMPUTERS_AMOUNT * arrayLen;++shift)
        *(to->field + shift) = *(from->field + shift);
}


void _write(int x, int y, char state, char* destination) {
    *(destination + y * width + x) = state;
}


char _read(int x, int y, char* source) {
    return *(source + y * width + x);
}


void print(char* source) {
    std::cout << '\n';
    for (int y = height - 1;y >= 0;--y) {
        for (int x = 0; x < width; ++x)
            std::cout << (int)_read(x, y, source) << ' ';
        std::cout << '\n';
    }
    std::cout << '\n';
}


void start_state(char* field) {
    int amount_of_points;
    std::cout << "amount of points: ";
    std::cin >> amount_of_points;
    for (int i = 0;i < amount_of_points;i++) {
        int x, y;
        std::cin >> x >> y;
        _write(x, y, 1, field);
    }
}