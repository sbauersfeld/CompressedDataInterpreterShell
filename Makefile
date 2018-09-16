
default:
	gcc -g -Wextra -Wall -o lab1b-client lab1b-client.c -lz
	gcc -g -Wextra -Wall -o lab1b-server lab1b-server.c -lz

client:
	gcc -g -Wextra -Wall -o lab1b-client lab1b-client.c -lz

server:
	gcc -g -Wextra -Wall -o lab1b-server lab1b-server.c -lz

dist:
	tar -czvf lab1b-604813474.tar.gz lab1b-client.c lab1b-server.c Makefile README

clean:
	rm lab1b-604813474.tar.gz lab1b-client lab1b-server
