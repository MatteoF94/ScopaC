# ScopaC
This C-written application consists in the famous game "Sweep". Different files form the program: roserv.c is the code of the server,
that will handle all the incoming connection requests from the players; room_UI.c and game_UI.c are the files in which you can find 
the code about the graphic interface of the program, such as the function that "moves" a card or the one that creates a room in the lobby;
room.c is the logic coded body, that includes main, memory handlings and variable declaration. Finally you can find two custom libraries - game_UI.h
and room_UI.h - and a CSS file, written to improve the "appealing" side of the application. To run the program and play, you
need the GTK+ platform, available at http://www.gtk.org/download/index.php. 
Now a basic explanation of what you will see (client side, the server has been already started up): the first step is in the lobby, where you,
the player, will be able to join a room or create one. Once the room is full, two players, the one who has been in it the most will now
be able to start the game (START MATCH button)! 
For more information, check the technic description of the program in the file "Scheda tecnica.doc" in this repository and remember... have fun playing!!
