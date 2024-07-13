#include <iostream>
#include <fstream>
#include <stdio.h>

int main(int argc, char *argv[])
{
    FILE *f;
    f = fopen("corvinIncast", "w+");
    
    int no_nodes = 21;
    int incast_node = 0;
    int cnt = 1;

    if (incast_node < 0 || incast_node >= no_nodes){
        std::cerr << "incast_node has to be in the range of [0, no_nodes-1]." << std::endl;
    }
    
    fprintf(f, "Nodes %d\n", no_nodes);
    fprintf(f, "Connections %d\n", (no_nodes-1));
    
    for (int i = 0; i < no_nodes; i++){
        if (i != incast_node){
            fprintf(f, "%d->%d id %d start 0 size 1048576\n", i, incast_node, cnt++);
        }
    }
    
    fclose(f);
}