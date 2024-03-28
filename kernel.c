#include "keyboard_map.h"

/* there are 25 lines each of 80 columns; each element takes 2 bytes */
#define LINES 25
#define COLUMNS_IN_LINE 80
#define BYTES_FOR_EACH_ELEMENT 2
#define SCREENSIZE BYTES_FOR_EACH_ELEMENT * COLUMNS_IN_LINE * LINES
#define RAND_MAX 32767  // Pseudo-random number generator max value

#define KEYBOARD_DATA_PORT 0x60
#define KEYBOARD_STATUS_PORT 0x64
#define IDT_SIZE 256
#define INTERRUPT_GATE 0x8e
#define KERNEL_CODE_SEGMENT_OFFSET 0x08

#define ENTER_KEY_CODE 0x1C
#define LEFT_KEY_CODE 0x4B 
#define RIGHT_KEY_CODE 0x4D 
#define SPACE_KEY_CODE 0x39 
#define SIZE 50

int board[SIZE-5][SIZE];
const char *SSG = "Space Shooter Game";
char *star = " * ";
char *wall1 = "| ";
char *wall2 = " | ";
int gameStart = 0;
int didStart = 0;
int instr = 0;
int gameOver = 0;
int movePlayer = 0;
int score=0;
int playerx = 38;
int playery = 24;
int bulletx = 0;
int bullety = 0;
extern unsigned char keyboard_map[128];
extern void keyboard_handler(void);
extern char read_port(unsigned short port);
extern void write_port(unsigned short port, unsigned char data);
extern void load_idt(unsigned long *idt_ptr);
unsigned int current_loc = 0;
char *vidptr = (char*)0xb8000;

void gotoxy(unsigned int x, unsigned int y);
void draw_strxy(const char *str, unsigned int x, unsigned int y);
void draw_player(void);
void generate_star_positions(int positions[][SIZE], int color);
void draw_bullet(void);
void clear_bullet(void);
void fire_bullet(void);
void check_collision(void);
void update_score(void);
void clear_player_right(void);
void clear_player_left(void);
void check_direction(void);
void update_instr(void);
void instruction_screen(void);
void start_screen(void);
void game(void);
void end_screen(void);
void move_bullet(void);
void sleep(int sec);
int my_rand(void);

void generate_star_positions(int positions[][SIZE], int color);
int positions[SIZE][SIZE] = {0};
struct IDT_entry {
    unsigned short int offset_lowerbits;
    unsigned short int selector;
    unsigned char zero;
    unsigned char type_attr;
    unsigned short int offset_higherbits;
};

struct IDT_entry IDT[IDT_SIZE];

void idt_init(void)
{
    unsigned long keyboard_address;
    unsigned long idt_address;
    unsigned long idt_ptr[2];

    keyboard_address = (unsigned long)keyboard_handler;
    IDT[0x21].offset_lowerbits = keyboard_address & 0xffff;
    IDT[0x21].selector = KERNEL_CODE_SEGMENT_OFFSET;
    IDT[0x21].zero = 0;
    IDT[0x21].type_attr = INTERRUPT_GATE;
    IDT[0x21].offset_higherbits = (keyboard_address & 0xffff0000) >> 16;

    write_port(0x20 , 0x11);
    write_port(0xA0 , 0x11);
    write_port(0x21 , 0x20);
    write_port(0xA1 , 0x28);
    write_port(0x21 , 0x00);
    write_port(0xA1 , 0x00);
    write_port(0x21 , 0x01);
    write_port(0xA1 , 0x01);

    write_port(0x21 , 0xff);
    write_port(0xA1 , 0xff);

    idt_address = (unsigned long)IDT ;
    idt_ptr[0] = (sizeof (struct IDT_entry) * IDT_SIZE) + ((idt_address & 0xffff) << 16);
    idt_ptr[1] = idt_address >> 16 ;

    load_idt(idt_ptr);
}

void kb_init(void)
{
    write_port(0x21 , 0xFD);
}

void kprint(const char *str,int color)
{
    unsigned int i = 0;
    while (str[i] != '\0') {
        vidptr[current_loc++] = str[i++];
        vidptr[current_loc++] = color;
    }
}

void generate_star_positions(int positions[][SIZE], int color) {
    int i, j;
    for (j = 0; j < SIZE; j++) {
        int random_row = my_rand() % (SIZE *2); // Randomly select a row
        for (i = 0; i < SIZE*2; i++) {
            if (i == random_row) { // Place a star at the randomly selected row
                positions[i][j] = color;
                // Print the star at the corresponding position using kprint
                gotoxy(j * 2, i);
                kprint("*", color);
            } 
        }
    }
}

void gotoxy(unsigned int x, unsigned int y) {
    unsigned int line_size = BYTES_FOR_EACH_ELEMENT * COLUMNS_IN_LINE;
    current_loc = BYTES_FOR_EACH_ELEMENT * (x * COLUMNS_IN_LINE + y);
}
void kprint_newline(void)
{
    unsigned int line_size = BYTES_FOR_EACH_ELEMENT * COLUMNS_IN_LINE;
    current_loc = current_loc + (line_size - current_loc % (line_size));
}

void print_integer(int n, int color)
{
    int i = 0;
    char buffer[20];  
    do {
        buffer[i++] = (n % 10) + '0';  
        n /= 10;
    } while (n != 0);
    while (i > 0) {
      vidptr[current_loc++] = buffer[--i];
      vidptr[current_loc++] = color;
    }
}
int my_rand(void) {
    static unsigned long rand_seed = 123456789;
    rand_seed = rand_seed * 1103515245 + 12345;
    return (unsigned int)(rand_seed / 65536) % RAND_MAX;
}

void clear_screen(void)
{
    unsigned int i = 0;
    while (i < SCREENSIZE) {
        vidptr[i++] = ' ';
        vidptr[i++] = 0x07;
    }
    current_loc = 0;
}

void keyboard_handler_main(void)
{
    unsigned char status;
    char keycode;

    write_port(0x20, 0x20);

    status = read_port(KEYBOARD_STATUS_PORT);
    if (status & 0x01) {
        keycode = read_port(KEYBOARD_DATA_PORT);
        if(keycode < 0)
            return;
          		
        if(keycode == SPACE_KEY_CODE && gameStart==0) {
            gameStart = 1;
            movePlayer = 1;
            update_instr();
            return;
        }

        if(keycode == ENTER_KEY_CODE) {
            didStart = 1;
            return;
        }
        if (keycode == SPACE_KEY_CODE) {
            if (gameStart == 1 && movePlayer == 1) {
                fire_bullet();
            }
            return;
        }
        
        if(keycode == LEFT_KEY_CODE) {
            if(playerx > 0 && didStart && movePlayer)
            {
              clear_player_left();
              playerx = playerx - 1;
              draw_player();
            }
            return;
        }
        
        if(keycode == RIGHT_KEY_CODE) {
            if(playerx < 75 && didStart && movePlayer)
            {
              clear_player_right();
              playerx = playerx + 1;
              draw_player();
            }
            return; 
        }
        
        if(keycode == 23) {
            instr = 1;
            return; 
        }
        
        if(keycode == 14) {
            instr = 0;
            return; 
        }

        vidptr[current_loc++] = keyboard_map[(unsigned char) keycode];
        vidptr[current_loc++] = 0x07;
    }
}


void sleep(int sec)
{
    int i;
    for(i=0;i<sec;i++);
}

void draw_strxy(const char *str,unsigned int x, unsigned int y) {
    gotoxy(y,x);
    kprint(str,0x0F);
}

void draw_player(void)
{
const char *player = "X";

    if(!gameOver) draw_strxy(player,playerx,playery);
}

void clear_player_right(void)
{
    const char *player_delete = "  ";
    draw_strxy(player_delete,playerx,playery);
}

void clear_player_left(void)
{
    const char *player_delete = "  ";
    draw_strxy(player_delete,playerx+4,playery);
}

void clear_player(void)
{
    const char *player_delete = "     ";
    draw_strxy(player_delete,playerx,playery);
}

void update_score(void)
{
    const char *score_s = "Score:";
    draw_strxy(score_s,1,0); 
    print_integer(score, 0x02);
}

void update_instr(void)
{
    if(movePlayer == 1)
    {
        const char *start_s = "                    ";
        draw_strxy(start_s,32,15);
    }
}

void gameboard(void)
{
    int i, j, x, y;
    draw_strxy(SSG,34,0);
    update_score();
    kprint_newline();
    kprint_newline();
    generate_star_positions(board, 0x0E); 
    draw_player();
}

void checkDirection(void)
{	
    int directionX = 1; // Initialize directionX
    if (directionX ==  1) { bulletx += 1; }
    if (directionX == -1) { bulletx -= 1; }
}

void draw_bullet(void) {
    gotoxy(bulletx, bullety);
    vidptr[current_loc++] = '*';
    vidptr[current_loc++] = 0x0F; // Set color for bullet (white)
}

void fire_bullet(void) {
    // Raketin hemen üstüne mermiyi yerleştir
    bulletx = playerx + 2; // Raket genişliğine göre pozisyonu ayarla
    bullety = playery - 1;

    // Mermiyi çiz
    draw_bullet();
}

void end_screen(void)
{
    int i;
    for(i = 0; i < 10;i++)
    {
        kprint_newline();
    }
    kprint("                               Your score is  : ",0x0F);
    print_integer(score,0x02);
    kprint_newline();
    kprint_newline();
    kprint("                               To restart press any key                                ",0x0F);
    for(i = 0; i < 10;i++)
    {
        kprint_newline();
    }
    kprint(" EDA NUR YARDIM                                                                    ",0x0B);
}

void start_screen(void)
{
    int i;
    for(i = 0; i < 10;i++)
    {
        kprint_newline();
    }
    kprint("                                SPACE SHOOTER GAME                                 ",0x0F);
    kprint_newline();
    kprint_newline();
    kprint("                               To start press ENTER                               ",0x0F);
    for(i = 0; i < 10;i++)
    {
        kprint_newline();
    }
    kprint(" EDA NUR YARDIM                                           Instructions(press 'i')   ",0x0B);
    while(!didStart && !instr);
    if(instr == 1)
    {
        clear_screen();
        instruction_screen();
    }
    clear_screen();
}

void instruction_screen(void)
{
    int i;
    for(i = 0; i < 5;i++)
    {
        kprint_newline();
    }
    kprint("                                  INTRUCTIONS                                  ",0x0F);
    kprint_newline();
    kprint_newline();
    kprint("               THE PURPOSE OF THE GAME IS TO HIT THE STARS AND                 ",0x0A);
    kprint("               EARN POINTS BY MOVING WITH THE RIGHT LEFT KEYS.                 ",0x0B);
    kprint("               							           ",0x0C);
    kprint("                        							   ",0x0D);
    kprint_newline();
    kprint_newline();
    kprint("                                 HAVE FUN!!!                                   ",0x0F);
    for(i = 0; i < 12;i++)
    {
        kprint_newline();
    }
    kprint("                  Press (<--)Backspace to return to the menu                   ",0x0B);
    kprint_newline();
    while(instr);
    clear_screen();
    start_screen();
}

void game(void)
{
    gameboard();
    while(1)
    {
        if(movePlayer == 0)
        {
            const char *start_s = "press space to start";
            draw_strxy(start_s,32,15);
        }
      
        if(gameOver == 1)
        {
            break;
        }
     
        move_bullet(); // bullet_control çağrısını kaldırın veya bullet_control fonksiyonunu tanımlayın
        sleep(13000000); 
    }  
    clear_screen();
}

void move_bullet(void) {
    bullety -= 1; 
}

void kmain(void)
{
    idt_init();
    kb_init();  
    clear_screen();
    start_screen();
    game();
    end_screen();      
}

