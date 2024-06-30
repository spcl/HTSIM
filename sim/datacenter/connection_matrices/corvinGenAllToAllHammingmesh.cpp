#include <iostream>
#include <fstream>
#include <stdio.h>

int main(int argc, char *argv[])
{
    FILE *f;
    f = fopen("corvinATAHM", "w+");

    int height = 5;
    int width = 5;
    int height_board = 4;
    int width_board = 4;
    
    int no_board_nodes = height * width * height_board * width_board;
    
    int ft_h, ft_w;
    if(2 * height > 64){ ft_h = (2 * height) / 63; }
    else{ ft_h = 1; }
    if(2 * width > 64){ ft_w = (2 * width) / 63; }
    else{ ft_w = 1; }
    
    fprintf(f, "Nodes %d\n", no_board_nodes + width * width_board * ft_h + height * height_board * ft_w);
    fprintf(f, "Connections %d\n", no_board_nodes * no_board_nodes);
    
    for (int i = 0; i < no_board_nodes; i++){
        for (int j = 0; j < no_board_nodes; j++){
            fprintf(f, "%d->%d id %d start 4 size 40960\n", i, j, i * no_board_nodes + j + 1);
        }
    }
    
    fclose(f);
}