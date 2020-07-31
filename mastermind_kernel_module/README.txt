
###########To create the driver##############

make				
sudo rmmod mastermind                       										---->remove module if exist  										
sudo insmod mastermind.ko major_number=142 mmind_max_guesses=10 mmind_number=4283	---->major number, max guess number and secret number can be given as module parameters
lsmod 																				---->to see mastermind in the list of loaded modules
sudo rm /dev/mastermind																---->remove device node if exist
sudo mknod -m 666 /dev/mastermind c 142 0											---->create device node		

echo "1234" > /dev/mastermind			---->write to device (example)
cat /dev/mastermind						---->read from device



gcc -o game game.c 						---->compile test code
./game									---->execute test code

######### When program is executed the process given below will be performed ##########

Enter secret: 								---->type 4 digit secret number
Staring new game secret= <secret number>	---->Your secret number will be showing here
Commands:									---->Possible commands that user can select
(New Game) new <secret>						---->To start new game, type "new" and your secret number
(Exit Game) end 0							---->To end game, type "end" and 0
(Guess) gus <guess>							---->To do guess, type "gus" and your guess number
