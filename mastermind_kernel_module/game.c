#include <sys/ioctl.h>
#include <linux/ioctl.h>
#include <stdio.h>
#include <fcntl.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAGIC 5813
#define MMIND_REMAINING _IO(MAGIC, 0)
#define MMIND_ENDGAME   _IO(MAGIC, 1)
#define MMIND_NEWGAME   _IOWR(MAGIC, 2, int)
#define MAXNR 2





void toString(int number, char* numberArray){
	int i = 3;
    for (; i > -1 ; i--) {
    	numberArray[i] = number % 10 + '0';
    	number = number / 10;
    }
}

int main(void) {
	char* guess = (char*)malloc(sizeof(char) * 5);
	char* buffer = (char*)malloc(sizeof(char) * 4096);
	int parameter = 4823;
	char* command = (char*)malloc(sizeof(char) * 4);
	int remaining;
	int fd = open("/dev/mastermind" , O_RDWR);
	printf("Enter secret: ");
	scanf("%d", &parameter);
	ioctl(fd, MMIND_NEWGAME, parameter);
	printf("Staring new game secret=%d\n",parameter);
	printf("Commands:\n (New Game) new <secret>\n (Exit Game) end 0\n(Guess) gus <guess>\n");
	close(fd);


START_AGAIN:
	while(ioctl(fd, MMIND_REMAINING))
	{
		fd = open("/dev/mastermind" , O_RDWR);
		printf("Enter command: ");
		parameter = 0;
		scanf("%s %d", command, &parameter);

		if (!strcmp(command, "new")) {
			printf("Starting new game with %d\n", parameter);
			ioctl(fd, MMIND_NEWGAME, parameter);
			goto START_AGAIN;

		} else if (!strcmp(command, "end")) {
		FIN:
			printf("Finishing game!\n");
			ioctl(fd, MMIND_ENDGAME);
			close(fd);
			break;
		} 
		else if (!strcmp(command, "gus") && parameter > 999 && parameter < 10000) {
			remaining = ioctl(fd, MMIND_REMAINING);
			toString(parameter, guess);
			write(fd, guess, 5);
			int read_count = read(fd, buffer, 4096);
			printf("******************* Remaining Guess: %d ************************\n", --remaining);
			//Print current results
			int i = 0;
			for(; i < read_count; i++){
				putchar(buffer[i]);
			}
			if(buffer[read_count - 11] == '4'){

				printf("Well done you won!!! \n");
				goto FIN;
			}
			if (remaining == 0) {
				printf("You lost\n");
				goto FIN;
			}
		} else {
			printf("Not a valid command\n");
		}
		close(fd);
	}

	free(buffer);
	free(guess);
	printf("Shutting down Bye Bye!!!\n");

	return 0;
}
