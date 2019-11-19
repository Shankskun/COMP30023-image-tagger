# Image Tagger Game
This program is written in the language C as part of the subject COMP30023: Computer Systems 2019, University of Melbourne

### Goal:
This program is acts as a game server between 2 players from 2 different browsers over HTTP. It ultilises the concept of socket programming in C, which allows the plaers to configure both *the server IP address* as well as *the port number*

This games requires players to *guess* the words or phase that have been submmited by the other player, based on the given image. Hence it is important that the players enter guesses that matches/describes the image as close as possible, to maximise the chances of players to guess the exact words.

---
### How to Run the Server
1. Linux environment is required in order to run the server.
2. Compile the code files using the `makefile`.
3. Run the server by calling `./image *IP_adress* *PORT_number*`.

### How to play
1. The game consists of two players and one server, where each of the players can only communicate with the server via *local host*. They will be required to enter their names before the game starts.
1. The game starts when both players hit the `submit button`; where the server will then submit both players with the same image.
1. Both players have to try and guess the words entered by the other player; if the words done match, players can continue to enter new phases until they hit a match.
1. Once the words matched, the game ends. They will be prompted to either `quit` or `start` over.
1. If a player quits, the game will be over and the server *resets*.
1. If both player continues, the process repeats and they will be given a new image for the next round.

---

##### How the site looks like:
- Start Screen:

  ![start-screen](https://github.com/nickangmc/image-tagger/blob/master/readme-images/start-screen.png)
  
- Ready Screen:

  ![ready-screen](https://github.com/nickangmc/image-tagger/blob/master/readme-images/ready-screen.png)


