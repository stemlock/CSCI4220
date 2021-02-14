hw3.out:
Authors:
Yang Li - liy38
Sam Temlock - temloo

Features:
For HW3, we implemented a CRI server that serves as a chat service between both IPv4 and IPv6 clients. Within the server, there can be multiple users as well as multiple channels that are supported. The protocol allows users to implement the following commands: 
• USER
• LIST
• JOIN
• PART
• OPERATOR
• KICK
• PRIVMSG
• QUIT

Notes:
- If the password used in --opt-pass is invalid (i.e. does not match regex) or the flag is not passed, then the default password of "password" will be used
- If the user inputs a username that is already in use, the server will disconnect the client

Bugs:
None that we know of