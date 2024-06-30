#include <iostream>
#include <fstream>
#include <stdio.h>

int main(int argc, char *argv[])
{
    FILE *f;
    f = fopen("corvinATA", "w+");
    
    int no_nodes = 32;
    
    fprintf(f, "Nodes %d\n", no_nodes);
    fprintf(f, "Connections %d\n", no_nodes * no_nodes);
    
    for (int i = 0; i < no_nodes; i++){
        for (int j = 0; j < no_nodes; j++){
            fprintf(f, "%d->%d id %d start 4 size 40960\n", i, j, i * no_nodes + j + 1);
        }
    }
    
    fclose(f);
}