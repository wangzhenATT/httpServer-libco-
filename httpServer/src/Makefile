httpd:main.o httpd.o
	g++ -g -o $@ $^ -I../include -L../lib -lcolib -ldl -lpthread
httpd.o:httpd.c
	gcc -c httpd.c
main.o:main.cpp
	g++ -c main.cpp -I../include 

.PHONY:clean
clean:
	rm -f httpd *.o
