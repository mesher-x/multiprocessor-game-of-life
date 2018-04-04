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


#define PERMITIONS (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP)
#define TCP_PORT_NUMBER 51000
#define NEIGHBOUR_PORTS_START 52000

std::string name;

std::vector<sem_t*> semaphores;
sem_t* mutex;
sem_t* for_exit;
sem_t* stop;
sem_t* share_up;
sem_t* share_down;
sem_t* next_share_up;
sem_t* next_share_down;
sem_t* counted;
sem_t* next_count;

int server_socket_fd;
int up_socket_fd;
int down_socket_fd;
int listening_socket_fd;

int WORKERS_AMOUNT;
int height;
int width;
int wholeArrayLen;
int arrayLen;
int connection_num;

pid_t master;
pid_t up_process;
pid_t down_process;
pid_t result_sender_process;
pid_t* workers;

int field_size;
int field_fd;
void* field_address;
char* Field;
char* StepField;

int next_field_fd;
void* next_field_address;
char* NextField;

bool early_ctr_c;
bool before_fork;

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
	
void up(struct sockaddr_in listening_address);
void down(struct sockaddr_in down_address);
void result_sender();
void init_sem();
void init_memory();
void getResourcers();
void close_connection(int socket_descriptor);
void finish();
void handler(int nsig);
void siguser_handler(int nsig);
void segv_handler(int nsig);
void clean();
void _write(int x, int y, char state, char* destination);
char _read(int x, int y, char* source);
void print(char* source);
void copy(char* to, char* from);


void worker(int arrayNum);
void update_core(int arrayNum, bool& alive_, bool& different_);
void update_bound(int arrayNum, bool& alive_, bool& different_);
int near_live_amount(int _x, int _y, char* source);





int main(int argc, char **argv) {
	int n;
	int ret;
	if (argc != 2) {
		std::cout << "need an ip address as an argument\n";
		exit(0);
	}

	early_ctr_c = false;
	before_fork = true;

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

	struct sigaction act2;
    sigemptyset(&act2.sa_mask);
    act2.sa_flags = 0;
    act2.sa_handler = segv_handler;
	ret = sigaction(SIGSEGV, &act2, NULL);    if (ret < 0) {std::cerr << "sigaction failed\n"; exit(0);}

	master = getpid();
	std::cout << "master: "<< master << '\n';

	server_socket_fd = -1;
	up_socket_fd = -1;
	down_socket_fd = -1;
	listening_socket_fd = -1;

	up_process = -1;
	down_process = -1;
	result_sender_process = -1;

	struct protoent *protocol_record = getprotobyname("tcp");

	struct sockaddr_in server_address;
	bzero(&server_address, sizeof(server_address));
	server_address.sin_family = AF_INET;
	server_address.sin_port = htons(TCP_PORT_NUMBER);
	server_address.sin_addr.s_addr = inet_addr(argv[1]); if (server_address.sin_addr.s_addr == (in_addr_t)(-1)) { std::cerr << "inet_adrr failed: " << strerror(errno) << '\n'; exit(0);}

	if (early_ctr_c) exit(0);
	ret = -1;
	while(ret < 0) {
		server_socket_fd = socket(PF_INET, SOCK_STREAM, protocol_record->p_proto); 		if (server_socket_fd < 0) {std::cerr << "socket failure server " << strerror(errno) << '\n'; exit(0);}
		std::cout << "in while to connect to server\n";
		ret = connect(server_socket_fd, (struct sockaddr *) &server_address, sizeof(struct sockaddr_in)); 
		if (ret < 0) {
			std::cout << strerror(errno) << '\n';
			close(server_socket_fd);
		}
		sleep(1);
	}


	if (early_ctr_c) {close_connection(server_socket_fd); exit(0);}
	//wholeArrayLen
	int wholeArrayLenNetwork;
	n = read(server_socket_fd, &wholeArrayLenNetwork, sizeof(wholeArrayLenNetwork)); 	if (n < 0) {std::cout << "read failed " << strerror(errno) << '\n'; close_connection(server_socket_fd); exit(0);}
	if (n == 0) {std::cout << "closing connection\n"; close(server_socket_fd); exit(0);}
	wholeArrayLen = ntohs(wholeArrayLenNetwork);
	std::cout << "wholeArrayLen : " << wholeArrayLen << '\n';
	

	if (early_ctr_c) {close_connection(server_socket_fd); exit(0);}
	//width
	int widthNetwork;
	n = read(server_socket_fd, &widthNetwork, sizeof(widthNetwork)); 	if (n < 0) {std::cout << "read failed " << strerror(errno) << '\n'; close_connection(server_socket_fd); exit(0);}
	if (n == 0) {std::cout << "closing connection\n"; close(server_socket_fd); server_socket_fd = -1; exit(0);}
	width = ntohs(widthNetwork);
	height = wholeArrayLen / width;
	if (height == 1)
		WORKERS_AMOUNT = 1;
	else {
		int i = 2;
		while (height % i) ++i;
		WORKERS_AMOUNT = i;
	}
	field_size = wholeArrayLen + 2 * width;
	std::cout << "field_size: " << field_size << '\n';
	std::cout << "width : " << width << '\n';
	std::cout << "height : " << height << '\n';
	std::cout << "WORKERS_AMOUNT : " << WORKERS_AMOUNT << '\n';
	arrayLen = wholeArrayLen / WORKERS_AMOUNT;


	if (early_ctr_c) {close_connection(server_socket_fd); exit(0);}
	//connection_num
	int connection_num_network;
	n = read(server_socket_fd, &connection_num_network, sizeof(connection_num_network)); 	if (n < 0) {std::cout << "read failed " << strerror(errno) << '\n'; close_connection(server_socket_fd); exit(0);}
	if (n == 0) {std::cout << "closing\n"; close(server_socket_fd); server_socket_fd = -1; }
	connection_num = ntohs(connection_num_network);
	std::cout << "connection_num: " << connection_num << '\n';
	
	init_memory();


	if (early_ctr_c) {close_connection(server_socket_fd); exit(0);}
	//field
	for (int i = 0;i < wholeArrayLen;++i) {
		n = read(server_socket_fd, Field + width + i, 1); 	if (n < 0) {std::cout << "read failed whole piece" << strerror(errno) << '\n'; close_connection(server_socket_fd); exit(0);}
		if (n == 0) {std::cout << "closing\n"; close(server_socket_fd); server_socket_fd = -1; exit(0);}
	}
	print(Field + width);
	copy(NextField, Field);

	//for up address
	struct sockaddr_in for_up_address;
	bzero(&for_up_address, sizeof(for_up_address));
	for_up_address.sin_family = AF_INET;
	for_up_address.sin_port = htons(NEIGHBOUR_PORTS_START + ntohs(connection_num_network));
	for_up_address.sin_addr.s_addr = inet_addr("127.0.0.1");


	if (early_ctr_c) {close_connection(server_socket_fd); exit(0);}
	n = write(server_socket_fd, &for_up_address.sin_addr.s_addr, sizeof(for_up_address.sin_addr.s_addr));   if (n < 0) {std::cerr << "write failed for up " << strerror(errno) << '\n'; close_connection(server_socket_fd); exit(0);}
	if (n == 0) {std::cout << "closing\n"; close(server_socket_fd); server_socket_fd = -1; exit(0);}


	if (early_ctr_c) {close_connection(server_socket_fd); exit(0);}
	n = write(server_socket_fd, &for_up_address.sin_port, sizeof(for_up_address.sin_port));   if (n < 0) {std::cerr << "write failed for up " << strerror(errno) << '\n'; close_connection(server_socket_fd); exit(0);}
	if (n == 0) {std::cout << "closing\n"; close(server_socket_fd); server_socket_fd = -1; exit(0);}
	std::cout << "ip: " << ntohl(for_up_address.sin_addr.s_addr) << ' ' << "port: " << ntohs(for_up_address.sin_port) << '\n';
	
	//down address
	struct sockaddr_in down_neighbour_address;
	down_neighbour_address.sin_family = AF_INET;

	if (early_ctr_c) {close_connection(server_socket_fd); exit(0);}
	n = read(server_socket_fd, &down_neighbour_address.sin_addr.s_addr, sizeof(down_neighbour_address.sin_addr.s_addr)); 	if (n < 0) {std::cout << "read failed for  " << strerror(errno) << '\n'; close_connection(server_socket_fd); exit(0);}
	if (n == 0) {std::cout << "! closing\n"; close(server_socket_fd); server_socket_fd = -1; exit(0);}
	if (early_ctr_c) {close_connection(server_socket_fd); exit(0);}
	n = read(server_socket_fd, &down_neighbour_address.sin_port, sizeof(down_neighbour_address.sin_port)); 	if (n < 0) {std::cout << "read failed for down " << strerror(errno) << '\n'; close_connection(server_socket_fd); exit(0);}
	if (n == 0) {std::cout << "closing\n"; close(server_socket_fd); server_socket_fd = -1; exit(0);}
	std::cout << "ip: " << ntohl(down_neighbour_address.sin_addr.s_addr) << ' ' << "port: " << ntohs(down_neighbour_address.sin_port) << '\n';
	
	init_sem();

	if (early_ctr_c) {close_connection(server_socket_fd); exit(0);}
	before_fork = false;
	//up
	ret = fork(); 
	if (ret < 0) {
		std::cerr << "fork failed up\n";
		close_connection(server_socket_fd); exit(0);
	} else if (ret == 0) {
		up(for_up_address);
	} else {
		up_process = ret;
		std::cout << "up_process: " << ret << '\n';
	}

	//down
	ret = fork(); 
	if (ret < 0) {
		std::cerr << "fork failed down\n";
		close_connection(server_socket_fd); exit(0);
	} else if (ret == 0) {
		down(down_neighbour_address);
	} else {
		down_process = ret;
		std::cout << "down_process: " << ret << '\n';
	}

	//result_sender
	ret = fork(); 
	if (ret < 0) {
		std::cerr << "fork failed result sender\n";
		close_connection(server_socket_fd); exit(0);
	} else if (ret == 0) {
		result_sender();
	} else {
		result_sender_process = ret;
		std::cout << "result_sender_process: " << ret << '\n';
	}

	std::cout << "before next count\n";
	sem_wait(next_count);
	std::cout << "after next count\n";
	copy(NextField, Field);
	//workers
	workers = new pid_t[WORKERS_AMOUNT];
	for (int i = 0;i < WORKERS_AMOUNT;++i) {
		ret = fork(); 
		if (ret < 0) {
			std::cerr << "fork failed worker\n";
			close_connection(server_socket_fd); exit(0);
		} else if (ret == 0) {
			std::cout << "spawned: " << i << '\n';
			worker(i);
		} else {
			workers[i] = ret;
			std::cout << "worker: " << ret << '\n';
		}
	}
	std::cout << "spawned workers\n";
	sem_wait(for_exit);
	finish();
}

void worker(int arrayNum) {
	if (server_socket_fd >= 0) close(server_socket_fd);
	if (up_socket_fd >= 0) close(up_socket_fd);
	if (down_socket_fd >= 0) close(down_socket_fd);
	if (listening_socket_fd >= 0) close(listening_socket_fd);
	getResourcers();
	// std::cout << int(*(Util->alive)) << '\n';
	// std::cout << int(*(Util->different)) << '\n';
	// std::cout << int(*(Util->barrier)) << '\n';
	// print(NextField);
	// print(Field);
	// sem_wait(mutex);
	// sem_wait(mutex);
    bool alive_, different_;
    while (true) {
        for (int x = 0; x < width; ++x)
            for (int y = (height / WORKERS_AMOUNT) * arrayNum; y < (height / WORKERS_AMOUNT) * (arrayNum + 1); ++y) {
                int lives = near_live_amount(x, y, Field + width);
                if (_read(x, y, Field + width)) {
                    if (lives < 2 || lives > 3)
                        _write(x, y, 0, NextField + width);
                } else {
                    if (lives == 3)
                        _write(x, y, 1, NextField + width);
                }
            }
        //std::cout << "number: " << arrayNum << '\n';
        alive_ = false;
        different_ = false;
        update_core(arrayNum, alive_, different_);
        //std::cout << "before mutex\n";
        sem_wait(mutex);
        //std::cout << "alive\n";
        if (alive_) *(Util->alive) = 1;
        //std::cout << "different\n";
        if (different_) *(Util->different) = 1;
        //std::cout << "barrier\n";
        *(Util->barrier) = *(Util->barrier) + 1;
        if (*(Util->barrier) == WORKERS_AMOUNT) {
        	//std::cout << "post semaphores\n";
            for (int i = 0; i < WORKERS_AMOUNT; i++)
                sem_post(semaphores[i]);
            *(Util->barrier) = 0;
        }
        sem_post(mutex);
        sem_wait(semaphores[arrayNum]);
        //std::cout << "update core\n";
        //updateBounds
        alive_ = false;
        different_ = false;
        update_bound(arrayNum, alive_, different_);
        sem_wait(mutex);
        if (alive_) *(Util->alive) = 1;
        if (different_) *(Util->different) = 1;
        *(Util->barrier) = *(Util->barrier) + 1;
        if (*(Util->barrier) == WORKERS_AMOUNT) {
   //      	std::cout << "Field:";
			// print(Field + width);

			// std::cout << "NextField:";
			// print(NextField + width);
        	sem_post(counted);
        	sem_wait(next_count);
        	copy(NextField, Field);
            for (int i = 0; i < WORKERS_AMOUNT; i++)
                sem_post(semaphores[i]);
            *(Util->barrier) = 0;
            *(Util->alive) = 0;
            *(Util->different) = 0;
        }
        sem_post(mutex);
        sem_wait(semaphores[arrayNum]);
    }
}

void update_core(int arrayNum, bool& alive_, bool& different_) {
    for (int x = 0; x < width; ++x)
        for (int y = (height / WORKERS_AMOUNT) * arrayNum + 1; y < (height / WORKERS_AMOUNT) * (arrayNum + 1) - 1; ++y) {
            if (_read(x, y, NextField + width))
                alive_ = true;
            if (_read(x, y, NextField + width) != _read(x, y, Field + width))
                different_ = true;
            _write(x, y, _read(x, y, NextField + width), Field + width);
        }
   	//std::cout << "updated_core\n";
}


void update_bound(int arrayNum, bool& alive_, bool& different_) {
    int y = (height / WORKERS_AMOUNT) * (arrayNum + 1) - 1;
    for (int x = 0; x < width; ++x) {
        if (_read(x, y, NextField + width))
            alive_ = true;
        if (_read(x, y, NextField + width) != _read(x, y, Field + width))
            different_ = true;
        _write(x, y, _read(x, y, NextField + width), Field + width);
    }
    y = (height / WORKERS_AMOUNT) * arrayNum;
    for (int x = 0; x < width; ++x) {
        if (_read(x, y, NextField + width))
            alive_ = true;
        if (_read(x, y, NextField + width) != _read(x, y, Field + width))
            different_ = true;
        _write(x, y, _read(x, y, NextField + width), Field + width);
    }
    //std::cout << "updated_bound\n";
}

int near_live_amount(int _x, int _y, char* source) {
    int y = _y - 1;
    int x = (_x == 0) ? width - 1 : _x - 1;
    int count = 0;
    for (;y != (_y + 2);y = ++y) {
        for (;x != (_x + 2) % width; x = ++x % width) {
            if (x == _x && y == _y)
                continue;
            if (_read(x, y, source))
                count += 1;
        }
        x = (_x == 0) ? width - 1 : _x - 1;
    }
    return count;
}

void result_sender() {
	if (up_socket_fd >= 0) close(up_socket_fd);
	if (down_socket_fd >= 0) close(down_socket_fd);
	if (listening_socket_fd >= 0) close(listening_socket_fd);
	int n;
	getResourcers();
	while(true) {
		std::cout << "stuck on share_up\n";
		sem_wait(share_up);
		std::cout << "stuck on share_down\n";
		sem_wait(share_down);
		sem_post(next_count);
		std::cout << "stuck on counted\n";
		sem_wait(counted);
		//print(Field + width);
		for (int i = 0;i < wholeArrayLen;++i) {
			n = write(server_socket_fd, Field + width + i, 1);   if (n < 0) {std::cerr << "write failed server field" << strerror(errno) << '\n'; close_connection(server_socket_fd); sem_post(for_exit); sem_wait(stop);}
			if (n == 0) {std::cout << "closing field write server\n"; close(server_socket_fd); server_socket_fd = -1; sem_post(for_exit); sem_wait(stop);}
		}

		n = write(server_socket_fd, Util->alive, 1);   if (n < 0) {std::cerr << "write failed server alive" << strerror(errno) << '\n'; close_connection(server_socket_fd); sem_post(for_exit); sem_wait(stop);}
		if (n == 0) {std::cout << "closing write server alive\n"; close(server_socket_fd); server_socket_fd = -1; sem_post(for_exit); sem_wait(stop);}

		n = write(server_socket_fd, Util->different, 1);   if (n < 0) {std::cerr << "write failed server different" << strerror(errno) << '\n'; close_connection(server_socket_fd); sem_post(for_exit); sem_wait(stop);}
		if (n == 0) {std::cout << "closing write server different\n"; close(server_socket_fd); server_socket_fd = -1; sem_post(for_exit); sem_wait(stop);}
		
		std::cout << "stuck on read\n";
		char ready = 0;
		n = read(server_socket_fd, &ready, sizeof(char)); 	if (n < 0) {std::cout << "read failed ready server" << strerror(errno) << '\n'; close_connection(server_socket_fd); sem_post(for_exit); sem_wait(stop);}
		if (n == 0) {std::cout << "closing read ready server\n"; close(server_socket_fd); server_socket_fd = -1; sem_post(for_exit); sem_wait(stop);}
		if (!ready || ready == '#') {
			sem_post(for_exit); sem_wait(stop);
		}
		sem_post(next_share_up);
		sem_post(next_share_down);
		std::cout << "step\n";
	}
}

void up(struct sockaddr_in listening_address) {
	if (server_socket_fd >= 0) close(server_socket_fd);
	if (down_socket_fd >= 0) close(down_socket_fd);
	getResourcers();
	int ret;
	struct protoent *protocol_record = NULL;
	protocol_record=getprotobyname("tcp");

	listening_socket_fd = socket(PF_INET, SOCK_STREAM, protocol_record->p_proto);		if (listening_socket_fd < 0) {std::cerr << "listening socket failure: " << strerror(errno)  << '\n'; sem_post(for_exit); sem_wait(stop);}

    int enable = 1;
    ret = setsockopt(listening_socket_fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int));     if (ret < 0) {std::cerr << "socket reuse failure: " << strerror(errno) << '\n'; close(listening_socket_fd); listening_socket_fd = -1; sem_post(for_exit); sem_wait(stop);}

	ret = bind(listening_socket_fd, (const struct sockaddr *) &listening_address, sizeof(struct sockaddr_in));
	if (ret < 0) {std::cerr << "listening_socket_fd bind failed: " << strerror(errno) << '\n';  close(listening_socket_fd); listening_socket_fd = -1; sem_post(for_exit); sem_wait(stop);}

	ret = listen(listening_socket_fd, 1);		if (ret < 0) {std::cerr << "listening_socket_fd listen failed " << strerror(errno) << '\n';  close(listening_socket_fd); listening_socket_fd = -1; sem_post(for_exit); sem_wait(stop);}
	std::cout << "i listen!\n";
	socklen_t client_address_len = sizeof(struct sockaddr_in);
	up_socket_fd = accept(listening_socket_fd, NULL, NULL);		if (up_socket_fd < 0) {std::cerr << "accept failed: " << strerror(errno) << '\n'; close_connection(listening_socket_fd); sem_post(for_exit); sem_wait(stop);}
	close_connection(listening_socket_fd);
	std::cout << "i heard!\n";
	while(true) {
		int n;
		std::cout << "stuck on next_share_up\n";
		sem_wait(next_share_up);
		for (int i = 0;i < width;++i) {
			n = read(up_socket_fd, Field + field_size - width + i, 1); 	if (n < 0) {
				std::cout << "read failed up border " << strerror(errno) << '\n'; close_connection(listening_socket_fd); close_connection(up_socket_fd); sem_post(for_exit); sem_wait(stop);;
			}
			if (n == 0 || *(Field + field_size - width + i) == '#') {std::cout << "closing up read\n"; close_connection(listening_socket_fd); close(up_socket_fd); up_socket_fd = -1; sem_post(for_exit); sem_wait(stop);}
		}
		for (int i = 0;i < width;++i) {
			n = write(up_socket_fd, Field + field_size - 2 * width + i, 1);   if (n < 0) {
				std::cerr << "write failed for down border " << strerror(errno) << '\n'; close_connection(listening_socket_fd); close_connection(up_socket_fd); sem_post(for_exit); sem_wait(stop);
			}
			if (n == 0) {std::cout << "closing up write\n"; close_connection(listening_socket_fd); close(up_socket_fd); up_socket_fd = -1; sem_post(for_exit); sem_wait(stop);}
		}
		sem_post(share_up);
		std::cout << "up step\n";
	}
}

void down(struct sockaddr_in down_address) {
	if (server_socket_fd >= 0) close(server_socket_fd);
	if (up_socket_fd >= 0) close(down_socket_fd);
	if (listening_socket_fd >= 0) close(down_socket_fd);
	getResourcers();
	struct protoent *protocol_record = NULL;
	protocol_record=getprotobyname("tcp");
	int ret = -1;
	while(ret < 0) {
		down_socket_fd = socket(PF_INET, SOCK_STREAM, protocol_record->p_proto);		if (down_socket_fd < 0) {std::cerr << "socket failure: " << errno << '\n'; sem_post(for_exit); sem_wait(stop);}
		std::cout << "in while down\n";
		ret = connect(down_socket_fd, (struct sockaddr *) &down_address, sizeof(struct sockaddr_in));
		if (ret < 0) {
			std::cout << strerror(errno) << '\n';
			close(down_socket_fd);
			down_socket_fd = -1;
		}
	}
	while(true) {
		int n;
		std::cout << "stuck on next_share_down\n";
		sem_wait(next_share_down);
		for (int i = 0;i < width;++i) {
			n = write(down_socket_fd, Field + width + i, 1);   if (n < 0) {std::cerr << "write failed for down border" << strerror(errno) << '\n'; close_connection(down_socket_fd); sem_post(for_exit); sem_wait(stop);}
			if (n == 0) {std::cout << "closing down write\n"; close(down_socket_fd); down_socket_fd = -1; sem_post(for_exit); exit(0);}
		}

		for (int i = 0;i < width;++i) {
			n = read(down_socket_fd, Field + i, 1); 	if (n < 0) {std::cout << "read failed down border" << strerror(errno) << '\n'; close_connection(down_socket_fd); sem_post(for_exit); sem_wait(stop);}
			if (n == 0 || *(Field + field_size - width + i) == '#') {std::cout << "closing down read\n"; close(down_socket_fd); down_socket_fd = -1; sem_post(for_exit); sem_wait(stop);}
		}
		sem_post(share_down);
		std::cout << "down step\n";
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

void finish() {
	int ret;
	int n;
	std::cout << "in finish\n";
	//kill workers
	if (result_sender_process >= 0) {
        std::cout << "terminate result_sender\n";
        ret = kill(result_sender_process, SIGTERM); if (ret < 0) {std::cout << "terminate failed result_sender_process" << strerror(errno) << '\n';}
        ret = waitpid(result_sender_process, NULL, 0); if (ret < 0) {std::cout << "waitpid failed result_sender_process" << strerror(errno) << '\n';}
        result_sender_process = -1;
        std::cout << "killed result_sender\n";
    }
	close_connection(server_socket_fd);

	if (up_process >= 0) {
        std::cout << "terminate up_process\n";
        ret = kill(up_process, SIGTERM); if (ret < 0) {std::cout << "terminate failed up_process" << strerror(errno) << '\n';}
        ret = waitpid(up_process, NULL, 0); if (ret < 0) {std::cout << "waitpid failed up_process" << strerror(errno) << '\n';}
        up_process = -1;
        std::cout << "killed up_process\n";
    }
	close_connection(up_socket_fd);

	if (down_process >= 0) {
        std::cout << "terminate down_process\n";
        ret = kill(down_process, SIGTERM); if (ret < 0) {std::cout << "terminate failed down_process" << strerror(errno) << '\n';}
        ret = waitpid(down_process, NULL, 0); if (ret < 0) {std::cout << "waitpid failed down_process" << strerror(errno) << '\n';}
        down_process = -1;
        std::cout << "killed down_process\n";
    }
	close_connection(down_socket_fd);

	for (int i = 0;i < WORKERS_AMOUNT;++i) {
		std::cout << "terminate worker\n";
        ret = kill(workers[i], SIGTERM); if (ret < 0) {std::cout << "terminate failed worker" << strerror(errno) << '\n';}
        ret = waitpid(workers[i], NULL, 0); if (ret < 0) {std::cout << "waitpid failed worker" << strerror(errno) << '\n';}
        down_process = -1;
        std::cout << "killed worker\n";
	}

    for (int i = 0; i < WORKERS_AMOUNT; ++i) {
        name = "/csem" + std::to_string(connection_num) + std::to_string(i);
        ret = sem_unlink(name.c_str()); if (ret < 0) std::cerr << "sem_unlink failed for " << i << "on exit\n";
    }
    name = "/cfor_exit" + std::to_string(connection_num);
    ret = sem_unlink(name.c_str()); if (ret < 0) std::cerr << "sem_unlink failed for /cfor_exit on exit\n";

    name = "/cmutex" + std::to_string(connection_num);
    ret = sem_unlink(name.c_str()); if (ret < 0) std::cerr << "sem_unlink failed for /mutex on exit\n";

    name = "/share_up" + std::to_string(connection_num);
    ret = sem_unlink(name.c_str()); if (ret < 0) std::cerr << "sem_unlink failed for /share_up on exit\n";

    name = "/share_down" + std::to_string(connection_num);
    ret = sem_unlink(name.c_str()); if (ret < 0) std::cerr << "sem_unlink failed for /share_down on exit\n";

    name = "/next_share_up" + std::to_string(connection_num);
    ret = sem_unlink(name.c_str()); if (ret < 0) std::cerr << "sem_unlink failed for /next_share_up on exit\n";

    name = "/next_share_down" + std::to_string(connection_num);
    ret = sem_unlink(name.c_str()); if (ret < 0) std::cerr << "sem_unlink failed for /next_share_down on exit\n";
    std::cout << "killed myself\n";
    exit(0);
}

void getResourcers() {
	name = "/cfor_exit" + std::to_string(connection_num);
    for_exit = sem_open(name.c_str(), O_RDWR); if (for_exit == SEM_FAILED) {std::cerr <<  strerror(errno) << " in child sem_open failure for /cfor_exit\n"; kill(master, SIGUSR1); exit(0);}

	name = "/cstop" + std::to_string(connection_num);
    stop = sem_open(name.c_str(), O_RDWR); if (stop == SEM_FAILED) {std::cerr <<  strerror(errno) << " in child sem_open failure for /cstop\n"; sem_post(for_exit); exit(0);}

    field_address = mmap(NULL, field_size, PROT_READ | PROT_WRITE, MAP_SHARED, field_fd, 0); if (field_address == MAP_FAILED) {std::cerr << "map failed for child " << strerror(errno) << "\n"; sem_post(for_exit); sem_wait(stop);}
    Field = static_cast<char*>(field_address);

    next_field_address = mmap(NULL, field_size, PROT_READ | PROT_WRITE, MAP_SHARED, next_field_fd, 0); if (next_field_address == MAP_FAILED) {std::cerr << "child old field map failed, " << strerror(errno) << "\n"; sem_post(for_exit); sem_wait(stop);}
    NextField = static_cast<char*>(next_field_address);

    utility_address = mmap(NULL, utility_size, PROT_READ | PROT_WRITE, MAP_SHARED, utility_fd, 0); if (utility_address == MAP_FAILED) {std::cerr << "child old field map failed, " << strerror(errno) << "\n"; sem_post(for_exit); sem_wait(stop);}
    Util = new Utility(utility_address);

    name = "/cmutex" + std::to_string(connection_num);
    mutex = sem_open(name.c_str(), O_RDWR); if (mutex == SEM_FAILED) {std::cerr <<  strerror(errno) << " in child sem_open failure for /mutex\n"; sem_post(for_exit); sem_wait(stop);}

    name = "/share_up" + std::to_string(connection_num);
    share_up = sem_open(name.c_str(), O_RDWR); if (share_up == SEM_FAILED) {std::cerr <<  strerror(errno) << " in child sem_open failure for /share_up\n"; sem_post(for_exit); sem_wait(stop);}

    name = "/share_down" + std::to_string(connection_num);
    share_down = sem_open(name.c_str(), O_RDWR); if (share_down == SEM_FAILED) {std::cerr <<  strerror(errno) << " in child sem_open failure for /share_down\n"; sem_post(for_exit); sem_wait(stop);}
    
    name = "/next_share_up" + std::to_string(connection_num);
    next_share_up = sem_open(name.c_str(), O_RDWR); if (next_share_up == SEM_FAILED) {std::cerr <<  strerror(errno) << " in child sem_open failure for /next_share_up\n"; sem_post(for_exit); sem_wait(stop);}
    
    name = "/next_share_down" + std::to_string(connection_num);
    next_share_down = sem_open(name.c_str(), O_RDWR); if (next_share_down == SEM_FAILED) {std::cerr <<  strerror(errno) << " in child sem_open failure for /next_share_down\n"; sem_post(for_exit); sem_wait(stop);}

    name = "/counted" + std::to_string(connection_num);
    counted = sem_open(name.c_str(), O_RDWR); if (counted == SEM_FAILED) {std::cerr <<  strerror(errno) << " in child sem_open failure for /counted\n"; sem_post(for_exit); sem_wait(stop);}

    name = "/next_count" + std::to_string(connection_num);
    next_count = sem_open(name.c_str(), O_RDWR); if (next_count == SEM_FAILED) {std::cerr <<  strerror(errno) << " in child sem_open failure for /next_count\n"; sem_post(for_exit); sem_wait(stop);}

    semaphores.resize(WORKERS_AMOUNT);
    for (int i = 0; i < WORKERS_AMOUNT; ++i) {
        std::string name = "/csem" + std::to_string(connection_num) + std::to_string(i);
        sem_t *sem = sem_open(name.c_str(), O_RDWR); if (sem == SEM_FAILED) {std::cerr << strerror(errno) << " in child sem_open failure for " << i << '\n'; sem_post(for_exit); sem_wait(stop);}
        semaphores[i] = sem;
    }
}

void init_memory() {
	int ret;

	//field
	name = "/cshared_memory_field" + std::to_string(connection_num);
    field_fd = shm_open(name.c_str(), O_CREAT | O_RDWR, PERMITIONS);     
    if (field_fd < 0) {
        std::cerr << "shared_memory_field field " << strerror(errno) << '\n';
        ret = shm_unlink(name.c_str()); 
        if (ret < 0) {
            std::cerr << "cshm_unlink failed for field, " << strerror(errno) << "\n"; 
            exit(0);
        }
        field_fd = shm_open(name.c_str(), O_CREAT | O_RDWR, PERMITIONS);
    }
    ret = ftruncate(field_fd, field_size);      
    if (ret < 0) {
        std::cerr << "ftruncate failed for field, " << strerror(errno) << "\n";
        write(field_fd, 0, field_size);
    }
    field_address = mmap(NULL, field_size, PROT_READ | PROT_WRITE, MAP_SHARED, field_fd, 0);    if (field_address == MAP_FAILED) {std::cerr << "map failed field " << strerror(errno) << "\n"; exit(0);}
    Field = static_cast<char*>(field_address);

    //old_field
    name = "/cshared_memory_next" + std::to_string(connection_num);
    next_field_fd = shm_open(name.c_str(), O_CREAT | O_RDWR, PERMITIONS);     
    if (next_field_fd < 0) {
        std::cerr << "shm_open failed next_field, " << strerror(errno) << '\n';
        ret = shm_unlink(name.c_str()); 
        if (ret < 0) {
            std::cerr << "shm_unlink failed next field, " << strerror(errno) << "\n"; 
           	exit(0);
        }
        next_field_fd = shm_open(name.c_str(), O_CREAT | O_RDWR, PERMITIONS);
    }
    ret = ftruncate(next_field_fd, field_size);      
    if (ret < 0) {
        std::cerr << "ftruncate failed old_field, " << strerror(errno) << "\n";
        write(next_field_fd, 0, field_size);
    }
    next_field_address = mmap(NULL, field_size, PROT_READ | PROT_WRITE, MAP_SHARED, next_field_fd, 0);    if (next_field_address == MAP_FAILED) {std::cerr << "map failed next field " << strerror(errno) << "\n"; exit(0);}
    NextField = static_cast<char*>(next_field_address);
    
    //utility
	name = "/cshared_memory_utility" + std::to_string(connection_num);
    utility_fd = shm_open(name.c_str(), O_CREAT | O_RDWR, PERMITIONS);       
    if (utility_fd < 0) {
        std::cerr << "shm_open failed, " << strerror(errno) << '\n';
        ret = shm_unlink(name.c_str());
        if (ret < 0) {
            std::cerr << "shm_unlink failed utility " << strerror(errno) << '\n'; 
            exit(0);
        }
        utility_fd = shm_open(name.c_str(), O_CREAT | O_RDWR, PERMITIONS); 
    }
    utility_size = 3 * sizeof(char);
    ret = ftruncate(utility_fd, utility_size);  
    if (ret < 0) {
        std::cerr << "ftruncate failed utility " << strerror(errno) << "\n";
        write(utility_fd, 0, utility_size);
    }
    utility_address = mmap(NULL, utility_size, PROT_READ | PROT_WRITE, MAP_SHARED, utility_fd, 0);       if (utility_address == MAP_FAILED) {std::cerr << "utility map failed " << strerror(errno) << "\n"; exit(0);}
    Util = new Utility(utility_address);
    *(Util->alive) = 1;
    *(Util->different) = 1;
    *(Util->barrier) = 0;
}


void init_sem() {
	int ret;
	//stop
    name = "/cstop" + std::to_string(connection_num);
    stop = sem_open(name.c_str(), O_CREAT | O_EXCL, PERMITIONS, 0);
    if (stop == SEM_FAILED) {
        int ret = sem_unlink(name.c_str());     if (ret < 0) std::cerr << strerror(errno) << " sem_unlink failed for /cstop\n";
        stop = sem_open(name.c_str(), O_CREAT | O_EXCL, PERMITIONS, 0);
    }
    //for_exit
    name = "/cfor_exit" + std::to_string(connection_num);
    for_exit = sem_open(name.c_str(), O_CREAT | O_EXCL, PERMITIONS, 0);
    if (for_exit == SEM_FAILED) {
        int ret = sem_unlink(name.c_str());     if (ret < 0) std::cerr << strerror(errno) << " sem_unlink failed for /cfor_exit\n";
        for_exit = sem_open(name.c_str(), O_CREAT | O_EXCL, PERMITIONS, 0);
    }
    //mutex
    name = "/cmutex" + std::to_string(connection_num);
    mutex = sem_open(name.c_str(), O_CREAT | O_EXCL, PERMITIONS, 1);
    if (mutex == SEM_FAILED) {
        int ret = sem_unlink(name.c_str());     if (ret < 0) std::cerr << strerror(errno) << " sem_unlink failed for /mutex\n";
        mutex = sem_open(name.c_str(), O_CREAT | O_EXCL, PERMITIONS, 1);
    }
    //share_up
    name = "/share_up" + std::to_string(connection_num);
    share_up = sem_open(name.c_str(), O_CREAT | O_EXCL, PERMITIONS, 0);
    if (share_up == SEM_FAILED) {
        int ret = sem_unlink(name.c_str());     if (ret < 0) std::cerr << strerror(errno) << " sem_unlink failed for /share_up\n";
        share_up = sem_open(name.c_str(), O_CREAT | O_EXCL, PERMITIONS, 0);
    }
    //share_down
    name = "/share_down" + std::to_string(connection_num);
    share_down = sem_open(name.c_str(), O_CREAT | O_EXCL, PERMITIONS, 0);
    if (share_down == SEM_FAILED) {
        int ret = sem_unlink(name.c_str());     if (ret < 0) std::cerr << strerror(errno) << " sem_unlink failed for /share_down\n";
        share_down = sem_open(name.c_str(), O_CREAT | O_EXCL, PERMITIONS, 0);
    }
    //next_share_up
   	name = "/next_share_up" + std::to_string(connection_num);
    next_share_up = sem_open(name.c_str(), O_CREAT | O_EXCL, PERMITIONS, 1);
    if (next_share_up == SEM_FAILED) {
        int ret = sem_unlink(name.c_str());     if (ret < 0) std::cerr << strerror(errno) << " sem_unlink failed for /next_share_up\n";
        next_share_up = sem_open(name.c_str(), O_CREAT | O_EXCL, PERMITIONS, 1);
    }
    //next_share_down
    name = "/next_share_down" + std::to_string(connection_num);
    next_share_down = sem_open(name.c_str(), O_CREAT | O_EXCL, PERMITIONS, 1);
    if (next_share_down == SEM_FAILED) {
        int ret = sem_unlink(name.c_str());     if (ret < 0) std::cerr << strerror(errno) << " sem_unlink failed for /next_share_down\n";
        next_share_down = sem_open(name.c_str(), O_CREAT | O_EXCL, PERMITIONS, 1);
    }
    //counted
    name = "/counted" + std::to_string(connection_num);
    counted = sem_open(name.c_str(), O_CREAT | O_EXCL, PERMITIONS, 0);
    if (counted == SEM_FAILED) {
        int ret = sem_unlink(name.c_str());     if (ret < 0) std::cerr << strerror(errno) << " sem_unlink failed for /counted\n";
        counted = sem_open(name.c_str(), O_CREAT | O_EXCL, PERMITIONS, 0);
    }
    //next_count
    name = "/next_count" + std::to_string(connection_num);
    next_count = sem_open(name.c_str(), O_CREAT | O_EXCL, PERMITIONS, 0);
    if (next_count == SEM_FAILED) {
        int ret = sem_unlink(name.c_str());     if (ret < 0) std::cerr << strerror(errno) << " sem_unlink failed for /next_count\n";
        next_count = sem_open(name.c_str(), O_CREAT | O_EXCL, PERMITIONS, 0);
    }
    //workers
    semaphores.resize(WORKERS_AMOUNT);
    for (int i = 0; i < WORKERS_AMOUNT; ++i) {
        std::string sem_name = "/csem" + std::to_string(connection_num) + std::to_string(i);
        semaphores[i] = sem_open(sem_name.c_str(), O_CREAT | O_EXCL, PERMITIONS, 0);
        if (semaphores[i] == SEM_FAILED) {
            int ret = sem_unlink(sem_name.c_str());     if (ret < 0) std::cerr << strerror(errno) << " sem_unlink failed for " << i << '\n';
            semaphores[i] = sem_open(sem_name.c_str(), O_CREAT | O_EXCL, PERMITIONS, 0);
        }
    }
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

void copy(char* to, char* from) {
    for (int shift = 0;shift < WORKERS_AMOUNT * arrayLen + 2 * width;++shift)
        *(to + shift) = *(from + shift);
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

void segv_handler(int nsig) {
	std::cout << "segv in " << getpid() << '\n';
	sem_post(for_exit);
	exit(0);
}