all: server client

server: server.c
	gcc -std=gnu99 pgmread.c server.c -o server

client: client.c
	gcc -std=gnu99 send_packet.c client.c -o client

clean:
	rm client server *.o hei.txt

## make && ./server 8080 ./big_set/ hei.txt
## ./client INADDR_ANY 8080 list_of_filenames.txt 0.2f
