all: 	clean stuff 

stuff: server participant observer

server: 
	gcc -g -o server prog3_server.c

observer: 
	gcc -g -o observer prog3_observer.c

participant: 
	gcc -g -o participant prog3_participant.c

clean:
	rm -r server participant observer
