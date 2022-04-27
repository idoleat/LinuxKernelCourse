all:
	gcc ./HazardPointer.c -o ./hp -Wall -lpthread

clean:
	rm ./hp

test:
	./hp

tsan:
	gcc ./HazardPointer.c -o ./hp -Wall -lpthread -fsanitize=thread

asan:
	gcc ./HazardPointer.c -o ./hp -Wall -lpthread -fsanitize=address
