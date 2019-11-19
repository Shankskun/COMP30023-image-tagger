// COMP30023 assignment 1, SOONG ZE SHAUN 900793

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <strings.h>
#include <sys/select.h>
#include <sys/sendfile.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define MAX_BUFF    2049
#define MAX_WORD    100
#define MAX_PLAYER  2

#define BODY_END            "</body>"
#define BREAK_LINE          "<br>"
#define USER_REDUNDANCY     "user="
#define QUIT_REDUNDANCY     "quit=Quit"
#define GUESS_REDUNDANCY    "&guess=Guess"
#define KEYWORD_REDUNDANCY  "keyword="

// ---------------------------- constants ----------------------------

static char const * const HTTP_200_FORMAT = "HTTP/1.1 200 OK\r\n\
Content-Type: text/html\r\n\
Content-Length: %ld\r\n\r\n";
static char const * const HTTP_400 = "HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\n\r\n";
static int const HTTP_400_LENGTH = 47;
static char const * const HTTP_404 = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n";
static int const HTTP_404_LENGTH = 45;

// ---------------------------- typedef ----------------------------

    /* represents the types of method */
typedef enum
{
    GET,
    POST,
    UNKNOWN
} METHOD;

    /* pages stages */
typedef enum
{
    INTRO_1,
    START_2,
    FIRSTTURN_3,
    ACCEPTED_4,
    DISCARDED_5,
    ENDGAME_6,
    GAMEOVER_7
} PAGE;

    /* represents other player's state */
typedef enum
{
    EARLY,
    READY,
    ENDED,
    QUIT,
    RESTART
} STATE;

    /* player details */
typedef struct {int id;
                int socket;
                int num_words;
                int num_turns;
                char* name;
				char** guess_words;
                STATE state;   
                PAGE page;
		    } player_t;

// ------------------------- function Prototypes -------------------------

int other_player(int id);
int identify_player(int socket, player_t *p);
int compare_other_player(int id1, int id2, player_t *p);
void free_all_players(player_t *p);
void allocate_player(player_t *p, int socket);
void load_page(char* html, int sockfd, char* buff);
void reset_players(player_t *p, int id);
void initialise_player(player_t *p, int maxplayer);
void establish_server(int sockfd, char* ip, char* port);
void clear_guesses(int id, player_t *p);
void print_into_html(char* newword, char* old_buffer, char* new_buffer);
void resize_load_page(char* html, int sockfd, char* buff, char* newword);
bool change_players(player_t *p, int id);
bool change_image(player_t *p, int i);
static bool handle_http_request(int sockfd, player_t *p);

// -------------------------------- main --------------------------------

int main(int argc, char * argv[])
{
    if (argc < 3)
    {
        fprintf(stderr, "error, no *ip or *port provided\nusage: %s [ip] [port]\n", argv[0]);
        return 0;
    }

    // create TCP socket which only accept IPv4
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // reuse the socket if possible
    int const reuse = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(int)) < 0)
    {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    establish_server(sockfd, argv[1], argv[2]);
    return 0;
}

// ------------------------- function Prototypes -------------------------

    /* creates and handles the connections */
void establish_server(int sockfd, char* ip, char* port)
{

    // create and initialise address we will listen on
    struct sockaddr_in serv_addr;
    bzero(&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    // if ip parameter is not specified
    serv_addr.sin_addr.s_addr = inet_addr(ip);
    serv_addr.sin_port = htons(atoi(port));

    // bind address to socket
    if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        perror("bind");
        exit(EXIT_FAILURE);
    }

    // listen on the socket
    listen(sockfd, 2);

    // initialise an active file descriptors set
    fd_set masterfds;
    FD_ZERO(&masterfds);
    FD_SET(sockfd, &masterfds);
    // record the maximum socket number
    int maxfd = sockfd;

    /* connection established */
    fprintf(stderr, "image_tagger server is now running at IP: %s on port %s\n", ip, port);

    // create an array for players
    player_t player_list[MAX_PLAYER];
    initialise_player(player_list, MAX_PLAYER);

    while (1)
    {
        // monitor file descriptors
        fd_set readfds = masterfds;
        if (select(FD_SETSIZE, &readfds, NULL, NULL, NULL) < 0)
        {
            perror("select");
            exit(EXIT_FAILURE);
        }

        // loop all possible descriptor
        for (int i = 0; i <= maxfd; ++i)
            // determine if the current file descriptor is active
            if (FD_ISSET(i, &readfds))
            {
                // create new socket if there is new incoming connection request
                if (i == sockfd){

                    struct sockaddr_in cliaddr;
                    socklen_t clilen = sizeof(cliaddr);
                    int newsockfd = accept(sockfd, (struct sockaddr *)&cliaddr, &clilen);
                    if (newsockfd < 0)
                        perror("accept");
                    
                    else{
                        // add the socket to the set
                        FD_SET(newsockfd, &masterfds);
                        // assign socket to a new player
                        allocate_player(player_list, newsockfd);
                        // update the maximum tracker
                        if (newsockfd > maxfd)
                            maxfd = newsockfd;
                        // print out the IP and the socket number
                        char ip[INET_ADDRSTRLEN];
                        printf(
                            "new connection from %s on socket %d\n",
                            // convert to human readable string
                            inet_ntop(cliaddr.sin_family, &cliaddr.sin_addr, ip, INET_ADDRSTRLEN),
                            newsockfd
                        );
                    }
                }
                // a request is sent from the client
                else if (!handle_http_request(i, player_list))
                {
                    close(i);
                    FD_CLR(i, &masterfds);
                }
            }
    }
    // free all players
    free_all_players(player_list);

    /* close socket */
    close(sockfd);
}


static bool handle_http_request(int sockfd, player_t *p)
{
    // try to read the request
    char buff[MAX_BUFF];
    memset(buff, 0, sizeof MAX_BUFF);

    int n = read(sockfd, buff, MAX_BUFF);
    if (n <= 0)
    {
        if (n < 0)
            perror("read");
        else
            printf("socket %d close the connection\n", sockfd);
        return false;
    }

    // terminate the string
    buff[n] = 0;
    char * curr = buff;

    // parse the method
    METHOD method = UNKNOWN;
    if (strncmp(curr, "GET ", 4) == 0)
    {
        curr += 4;
        method = GET;
    }
    else if (strncmp(curr, "POST ", 5) == 0)
    {
        curr += 5;
        method = POST;
    }
    else if (write(sockfd, HTTP_400, HTTP_400_LENGTH) < 0)
    {
        perror("write");
        return false;
    }

    // sanitise the URI
    while (*curr == '.' || *curr == '/')
        ++curr;

    // identify the player requesting, get their id
    int id = identify_player(sockfd, p);
    int id_other = other_player(id);

    // for pages without parameters (buttons)
    if (*curr == ' ' || *curr == '?')
    {
        // all GET methods
        if (method == GET)
        {   
            // first page
            if(p[id].page == INTRO_1)
            {
                // load first page
                load_page("1_intro.html", sockfd, buff);

            }
            // START_2 -> FIRSTTURN_3
            // or when player RESTARTS the game
            else if((p[id].page == START_2) || (p[id].state == ENDED) || (p[id].state == RESTART))
            {
                // load page 3
                if(change_image(p, id)){
                    load_page("3_first_turn.html", sockfd, buff);
                }else{
                    load_page("3_first_turn_1.html", sockfd, buff);
                }

                //load_page("3_first_turn_1.html", sockfd, buff);
                // change player's state
                p[id].state = READY;

                // change game state of other player
                // only if they still in endgame page
                if(p[id_other].state == ENDED){
                    p[id_other].state = RESTART;
                }
            }
        }
        // all POST methods
        else if (method == POST)
        {
            // for QUIT button
            if(strstr(buff, QUIT_REDUNDANCY) != NULL){

                // load GAMEOVER_7
                load_page("7_gameover.html", sockfd, buff);
                // change player's state
                p[id].state = QUIT;
                p[id].page  = GAMEOVER_7;
                p[id_other].state = QUIT;
            }    

            // INTRO_1 -> START_2
            else if(p[id].page == INTRO_1)
            {
                // switch page
                p[id].page = START_2;

                // locate the username
                char * user = strstr(buff, USER_REDUNDANCY) + 5;
                // store username
                strcpy(p[id].name, user);

                // the length includes the <br> in the html
                char username[MAX_BUFF];
                memset(username, 0, MAX_BUFF);

                strcpy(username, BREAK_LINE);
                strcat(username, user);
                username[strlen(username)] = '\0';

                // load html with added words
                resize_load_page("2_start.html", sockfd, buff, username);
            }

            // FIRSTTURN_3
            // handling request logic 
            else if(p[id].page==START_2 || p[id].page==FIRSTTURN_3)
            {
                // change state
                p[id].page = FIRSTTURN_3;

                // player quit
                if(p[id].state == QUIT){
                    load_page("7_gameover.html", sockfd, buff);
                    p[id].page  = GAMEOVER_7;
                }

                // too early wait. Or when player restarted, waiting for reply
                else if(p[id_other].state == EARLY || p[id_other].state == RESTART){
                    // load page 5
                    if(change_image(p, id)){
                        load_page("5_discarded.html", sockfd, buff);
                    }else{
                        load_page("5_discarded_1.html", sockfd, buff);
                    }
                }

                // game ended (word match)
                else if(p[id].state == ENDED || p[id].state == RESTART){
                    // both players in endgame
                    load_page("6_endgame.html", sockfd, buff);

                    // other player restarted and is waiting
                    if(p[id].state == RESTART){
                        p[id].state = EARLY;
                        p[id].page  = START_2;
                    }else{
                        p[id].state = ENDED;
                    }
                }

                // player ready
                else{
                    // locate the guess and get keyword from post request
                    char * new_guess = strstr(buff, KEYWORD_REDUNDANCY) + 8;
                    new_guess[strlen(new_guess)- strlen(GUESS_REDUNDANCY)] = '\0';

                    // store keyword
                    strcpy(p[id].guess_words[p[id].num_words], new_guess);
                    p[id].num_words ++;

                    // checks for same word
                    int result = compare_other_player(id, id_other, p);

                    // correct guess
                    if(result == 1){
                        // change player's state
                        p[id].state = ENDED;
                        p[id_other].state = ENDED;

                        // increase players' turns
                        p[id].num_turns += 1;
                        p[id_other].num_turns += 1;
                    
                        load_page("6_endgame.html", sockfd, buff);
                        // clear both player's guesses
                        clear_guesses(id, p);
                        clear_guesses(id_other, p);
                    }
                    // wrong guess
                    else{
                        // the length includes the <br> in the html
                        char guess_a[MAX_BUFF];
                        strcpy(guess_a, BREAK_LINE);

                        if (p[id].num_words == 1)
                        {
                            strcat(guess_a, new_guess);
                            guess_a[strlen(guess_a)] = '\0';
                        }
                        else
                        {
                            for(int i=0; i<p[id].num_words; i++){
                                strcat(guess_a, p[id].guess_words[i]);
                                strcat(guess_a, BREAK_LINE);
                            }
                        }
                        // load page 4
                        if(change_image(p, id)){
                            resize_load_page("4_accepted.html", sockfd, buff, guess_a);
                        }else{
                            resize_load_page("4_accepted_1.html", sockfd, buff, guess_a);
                        }
                    }
                }
            }
        }
        else
            // never used, just for completeness
            fprintf(stderr, "no other methods supported");
    }
    // send 404
    else if (write(sockfd, HTTP_404, HTTP_404_LENGTH) < 0)
    {
        perror("write");
        return false;
    }
    // return false when player quit to close socket
    return change_players(p, id);
}

    /* adds the new word in the html (between the <body> */
void print_into_html(char* newword, char* old_buffer, char* new_buffer){

    // slice the html string into 2 parts
    char start[MAX_BUFF];
    char* end = strstr(old_buffer, BODY_END);

    // front part
    strncpy(start, old_buffer, strlen(old_buffer)-strlen(end));
    start[strlen(start)] = '\0';

    // combine 
    strcpy(new_buffer, start);
    strcat(new_buffer, newword);
    strcat(new_buffer, end);

    new_buffer[strlen(new_buffer)] = '\0';
}

    /* loads the html page */
void load_page(char* html, int sockfd, char* buff){

    // get the size of the file
    struct stat st;
    stat(html, &st);
    int n = sprintf(buff, HTTP_200_FORMAT, st.st_size);

    // send the header first
    if (write(sockfd, buff, n) < 0)
    {
        perror("write");
        return;
    }

    // read the content of the HTML file
    int filefd = open(html, O_RDONLY);

    do
    {
        n = sendfile(sockfd, filefd, NULL, 2048);
    }
    while (n > 0);
    if (n < 0)
    {
        perror("sendfile");
        close(filefd);
        return;
    }
    close(filefd);
}

    /* load the html witha added text */
void resize_load_page(char* html, int sockfd, char* buff, char* newword){

    // get the size of the file
    struct stat st;
    stat(html, &st);

    // increase file size to accommodate the new guess
    int added_length    = strlen(newword);
    long size           = st.st_size + added_length;
    int n = sprintf(buff, HTTP_200_FORMAT,size);
    // send the header first
    if (write(sockfd, buff, n) < 0)
    {
        perror("write");
        return;
    }
    // read the content of the HTML file
    int filefd = open(html, O_RDONLY);
    n = read(filefd, buff, st.st_size);
    if (n < 0)
    {
        perror("read");
        close(filefd);
        return;
    }
    close(filefd);

    // makes new buffer, copies the username into it
    char* new_buffer = (char *) malloc(sizeof(MAX_BUFF));
    print_into_html(newword, buff, new_buffer);

    if (write(sockfd, new_buffer, size) < 0)
    {
        perror("write");
        return;
    }
    free(new_buffer);
}

    // new player details when first connected
void initialise_player(player_t *p, int maxplayer){

    for(int i=0; i<maxplayer; i++){
        p[i].id        = i;
        p[i].socket    = -1;
        p[i].num_words = 0;
        p[i].num_turns = 0;
        p[i].state     = EARLY;
        p[i].page      = INTRO_1;
        p[i].name      = malloc(MAX_BUFF * sizeof(char));
        p[i].guess_words = malloc(MAX_WORD * sizeof(char*));

        // allocate memory for array of words
        for(int j=0; j<MAX_WORD; j++){
            p[i].guess_words[j] = malloc((MAX_BUFF)* sizeof(char));
        }
    }
}

    // set socket number to player for identification
void allocate_player(player_t *p, int socket){

    // check for empty player slots
    for(int i=0; i<MAX_PLAYER; i++){

        // empty sockets
        if(p[i].socket == -1){
            p[i].socket = socket;
            break;
        }
    }
}


    // find the players according to the socket connected
int identify_player(int socket, player_t *p){

    for(int i=0; i<MAX_PLAYER; i++){

        if(p[i].socket == socket){
            // returns id of player
            return i;
        }
    }
    return false;
}

    // get other player's id (only 2 players, hence ids are 0 & 1)
int other_player(int id){

    if(id == 0){
        return 1;
    }
    else{
        return 0;
    }
}

    // compare player id1 with all of id2s
int compare_other_player(int id1, int id2, player_t *p){

    // return 1 if success 
    int result = 0;

    // newest word
    char new_word[MAX_BUFF];
    strcpy(new_word, p[id1].guess_words[p[id1].num_words - 1]);

    // temp word for id2
    char temp_word[MAX_BUFF];

    // loop all words
    for(int i=0; i<p[id2].num_words; i++){

        strcpy(temp_word, p[id2].guess_words[i]);
        
        // match found
        if(strcmp(temp_word, new_word) == 0){
            result = 1;

            // clear array
            memset(new_word, 0, sizeof new_word);
            memset(temp_word, 0, sizeof temp_word);
            break;
        }
    }
    return result;
}

    // free memory for both players, or sets to null when player 
    // press start game again, (1 restart, 0 quit)
void clear_guesses(int id, player_t *p){
 
    for(int i=0; i<MAX_WORD; i++){
        memset(p[id].guess_words[i],'\0',MAX_BUFF);                
        p[id].num_words=0;
    }
}

    // resets user details
void reset_players(player_t *p, int id){

    p[id].socket    = -1;
    p[id].num_words = 0;
    p[id].num_turns = 0;
    p[id].state     = EARLY;
    p[id].page      = INTRO_1;
    
    memset(p[id].name, 0, MAX_BUFF);
    // allocate memory for array of words
    for(int j=0; j<MAX_WORD; j++){
        memset(p[id].guess_words[j], '\0', MAX_BUFF);
    }
}

    // change both players (reset) when they are at gameover page
bool change_players(player_t *p, int id){

    // player quited and is in game over page, reset them
    if((p[id].state == QUIT) && (p[id].page == GAMEOVER_7)){
        printf("socket %d closed\n", p[id].socket);

        reset_players(p, id);

        // close socket
        return false;
    }
    else{
        // keep socket open
        return true;
    }
}

    // free all players' memory
void free_all_players(player_t *p){

    // free each word
    for(int id=0; id<MAX_PLAYER; id++){

        for(int i=0; i<MAX_WORD; i++){
            free(p[id].guess_words[i]);
        }
    }

    // free the word array
    for(int id=0; id<MAX_PLAYER; id++){
        free(p[id].guess_words);
    }
}

    // change the images for each turn
bool change_image(player_t *p, int i){

    // first image
    if(p[i].num_turns == 0){
        return true;
    }
    // second image for second turn and more
    else{
        return false;
    }
}
