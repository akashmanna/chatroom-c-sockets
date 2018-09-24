Chat server  

Execute server:  
	$ make clean   
	$ make server  
	$ make run  

Add Client:
	$ telnet <ip_addr_of_server> 44444

Client Functionalities:
	@<nick> <message>: Send message to User nick (If user is offline, message will be delivered on next login)
	@all <message>: Broadcase message to all registered users  
	\ShowAll: Show all Registered Users  
	\Online: Show all Online Users  
	\Help: brings up help box  
 	\Quit: Exit the chat room  

The server uses System V Message Queues internally for communication
