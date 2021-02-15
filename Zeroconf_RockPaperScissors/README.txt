zero_conf.out:
Authors:
Yang Li - liy38
Sam Temlock - temloo

Features:
Implements a Zeroconf server that handles a basic “Rock Paper Scissors” (RPS) game simulation between two clients. Server prompts player for name and RPS choice, and displays to each client who the winner was.


Notes:
1. In our implementation, the first player does not have to wait for the second player to connect before entering information regarding his name and RPS choice.
2. We integrated both pthreads as well as the select call in our program. 

Bugs:
None that we know of