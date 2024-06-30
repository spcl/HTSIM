#include <iostream>
#include <fstream>
#include <stdio.h>

int main(int argc, char *argv[])
{
    FILE *f;
    f = fopen("corvinIncast", "w+");
    
    int no_nodes = 32;
    int input_node = 0;

    if (input_node < 0 || input_node >= no_nodes){
        std::cerr << "input_node has to be in the range of [0, no_nodes-1]." << std::endl;
    }
    
    fprintf(f, "Nodes %d\n", no_nodes);
    fprintf(f, "Connections %d\n", no_nodes);
    
    for (int i = 0; i < no_nodes; i++){
        fprintf(f, "%d->%d id %d start 4 size 40960\n", i, input_node, i + 1);
    }
    
    fclose(f);
}