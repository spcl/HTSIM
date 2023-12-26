/*
 * Copyright (c) 2009 The Trustees of Indiana University and Indiana
 *                    University Research and Technology
 *                    Corporation.  All rights reserved.
 *
 * Author(s): Torsten Hoefler <htor@cs.indiana.edu>
 *            Timo Schneider <timoschn@cs.indiana.edu>
 *
 */

#include <algorithm>
#include <assert.h>
#include <fstream>
#include <math.h>
#include <queue>
#include <sstream>
#include <stdlib.h>
#include <string.h>

#include "MersenneTwister.h"
#include "schedgen.hpp"

#define RANK2VRANK(rank, vrank, root)                                          \
    {                                                                          \
        vrank = rank;                                                          \
        if (rank == 0)                                                         \
            vrank = root;                                                      \
        if (rank == root)                                                      \
            vrank = 0;                                                         \
    }
#define VRANK2RANK(rank, vrank, root)                                          \
    {                                                                          \
        rank = vrank;                                                          \
        if (vrank == 0)                                                        \
            rank = root;                                                       \
        if (vrank == root)                                                     \
            rank = 0;                                                          \
    }

int MAKE_TAG(int comm, int tag);

Goal::Goal(gengetopt_args_info *args_info, int nranks)
        : sends(0), recvs(0), execs(0), ranks(0), reqs(0), curtag(-1) {
    // create filename
    this->filename.clear();
    this->filename.append(std::string(args_info->filename_arg));
    this->myfile.open(this->filename.c_str(),
                      std::fstream::out | std::fstream::trunc);
    std::stringstream tmp;
    tmp << "num_ranks " << nranks << "\n";
    this->AppendString(tmp.str());

    this->cpu = args_info->cpu_arg;
    this->nb = args_info->nb_given;
    if (nb) {
        this->poll_int = args_info->nb_poll_arg;
        this->nbfunc = args_info->nb_arg;
        this->ranks_init.resize(nranks);
        std::fill(this->ranks_init.begin(), this->ranks_init.end(), false);
    }
}

Goal::~Goal() {

    if (myfile.is_open()) {
        myfile.close();
    } else {
        std::cout << "Unable to open file " << this->filename;
        std::cout << " for writing" << std::endl;
    }

    std::cout << sends << " sends, " << recvs << " recvs, " << execs
              << " execs, and " << reqs << " reqs among " << ranks
              << " hosts written" << std::endl;
}

void Goal::StartRank(int rank) {

    this->ranks++;
    this->curtag = 0;

    std::stringstream tmp;
    tmp << "\nrank " << rank << " {\n";
    AppendString(tmp.str());

    // reset label counter
    this->id_counter = 0;

    if (nb) {
        if (!ranks_init[rank]) {
            if (poll_int) { // do we segment?
                int last = 0;
                for (int i = 0; i < nbfunc; i += poll_int) {
                    int cur = this->Exec("nbfunc", poll_int, cpu);
                    // no last in first round :)
                    if (i)
                        this->Requires(cur, last);
                    last = cur;
                }
            } else { // no segmentation
                this->Exec("nbfunc", nbfunc, cpu);
            }
        }
        ranks_init[rank] = true;
    }
}

Goal::t_id Goal::Send(int bufsize, int dest) {

    this->sends++;

    std::stringstream tmp;

    this->id_counter++;
    tmp << "l" << this->id_counter << ": ";
    tmp << "send ";
    tmp << bufsize << "b ";
    tmp << "to " << dest << " tag " << curtag;
    tmp << std::endl;
    AppendString(tmp.str());

    // append to independent set
    start.insert(id_counter);
    end.insert(id_counter);

    return this->id_counter;
}

void Goal::Comment(std::string comment) {
    std::stringstream tmp;
    tmp << "/* " << comment << " */" << std::endl;
    AppendString(tmp.str());
}

Goal::t_id Goal::Recv(int bufsize, int src) {

    this->recvs++;

    std::stringstream tmp;

    this->id_counter++;
    tmp << "l" << this->id_counter << ": ";
    tmp << "recv ";
    tmp << bufsize << "b ";
    tmp << "from " << src << " tag " << curtag;
    tmp << std::endl;
    AppendString(tmp.str());

    // append to independent set
    start.insert(id_counter);
    end.insert(id_counter);

    return this->id_counter;
}

void Goal::Requires(Goal::t_id tail, Goal::t_id head) {

    this->reqs++;

    std::stringstream tmp;

    tmp << "l" << tail << " requires ";
    tmp << "l" << head << std::endl;

    // erase from independent set
    start.erase(tail);
    end.erase(head);

    AppendString(tmp.str());
}

void Goal::Irequires(int tail, int head) {

    this->reqs++;

    std::stringstream tmp;

    tmp << "l" << tail << " irequires ";
    tmp << "l" << head << std::endl;

    // append to independent set
    start.erase(tail);
    end.erase(head);

    AppendString(tmp.str());
}

void Goal::EndRank() { AppendString("}\n"); }

void Goal::AppendString(std::string str) {
    this->schedule.append(str);

    if (this->schedule.length() > 1024 * 1024 * 16) {
        // write the schedule if it is bigger than 16 MB
        this->Write();
    }
}

void Goal::Write() {

    if (myfile.is_open()) {
        myfile << this->schedule;
        this->schedule.clear();
        myfile.sync();
    } else {
        std::cout << "Unable to open file " << this->filename;
        std::cout << " for writing" << std::endl;
    }
}

// this exec stuff is a big mess and needs to be cleaned up sometime ...
int Goal::Exec(std::string opname, btime_t size, int proc) {

    this->execs++;

    std::stringstream tmp;

    this->id_counter++;
    tmp << "l" << this->id_counter << ": ";
    tmp << "calc " << size;
    if (cpu)
        tmp << " cpu " << proc;
    tmp << std::endl;
    this->schedule.append(tmp.str());

    return this->id_counter;
}

int Goal::Exec(std::string opname, std::vector<buffer_element> buf) {

    this->execs++;

    std::stringstream tmp;

    this->id_counter++;
    tmp << "l" << this->id_counter << ": ";
    tmp << "calc ";
    int size = 0;
    for (unsigned int i = 0; i < buf.size(); i++) {
        size += buf[i].size;
    }
    tmp << size << std::endl;
    this->schedule.append(tmp.str());

    return this->id_counter;
}

int Goal::Exec(std::string opname, btime_t size) {

    std::vector<buffer_element> buf;
    buffer_element elem = buffer_element(1, 1, size);
    buf.push_back(elem);
    return Exec(opname, buf);
}

void create_binomial_tree_bcast_rank(Goal *goal, int root, int comm_rank,
                                     int comm_size, int datasize) {
    int vrank;
    RANK2VRANK(comm_rank, vrank, root);

    Goal::t_id recv = -1, send = -1;
    for (int r = 0; r < ceil(log2(comm_size)); r++) {
        int vpeer = vrank + (int)pow(2, r);
        int peer;
        VRANK2RANK(peer, vpeer, root);
        if ((vrank + pow(2, r) < comm_size) and (vrank < pow(2, r))) {
            send = goal->Send(datasize, peer);
        }
        if ((send >= 0) and (recv >= 0)) {
            goal->Requires(send, recv);
        }
        vpeer = vrank - (int)pow(2, r);
        VRANK2RANK(peer, vpeer, root);
        if ((vrank >= pow(2, r)) and (vrank < pow(2, r + 1))) {
            recv = goal->Recv(datasize, peer);
        }
    }
}

void create_binomial_tree_bcast(gengetopt_args_info *args_info) {

    int comm_size = args_info->commsize_arg;
    int datasize = args_info->datasize_arg;
    Goal goal(args_info, comm_size);

    for (int comm_rank = 0; comm_rank < comm_size; comm_rank++) {
        goal.StartRank(comm_rank);
        create_binomial_tree_bcast_rank(&goal, args_info->root_arg, comm_rank,
                                        comm_size, datasize);
        goal.EndRank();
    }
    goal.Write();
}

void create_binary_tree_bcast_rank(Goal *goal, int root, int comm_rank,
                                   int comm_size, int datasize) {
    int vrank;
    RANK2VRANK(comm_rank, vrank, root);

    std::queue<int> q;
    int cnt = 0;
    q.push(0);

    const int GOAL_ID_NULL = -1;
    Goal::t_id recv = GOAL_ID_NULL;
    Goal::t_id send[2] = {GOAL_ID_NULL, GOAL_ID_NULL};
    while (cnt < comm_size) {
        int src = q.front();
        q.pop();
        int tgt = ++cnt;
        int vtgt, vsrc;
        VRANK2RANK(vtgt, tgt, root);
        VRANK2RANK(vsrc, src, root);
        if (tgt < comm_size) {
            if (comm_rank == src) {
                if (send[0] == GOAL_ID_NULL)
                    send[0] = goal->Send(datasize, vtgt);
                else if (send[1] == GOAL_ID_NULL)
                    send[1] = goal->Send(datasize, vtgt);
                else
                    assert(false);
            }
            if (comm_rank == tgt) {
                assert(recv == GOAL_ID_NULL);
                recv = goal->Recv(datasize, vsrc);
            }
            q.push(tgt);
        }
        tgt = ++cnt;
        VRANK2RANK(vtgt, tgt, root);
        if (tgt < comm_size) {
            if (comm_rank == src) {
                if (send[0] == GOAL_ID_NULL)
                    send[0] = goal->Send(datasize, vtgt);
                else if (send[1] == GOAL_ID_NULL)
                    send[1] = goal->Send(datasize, vtgt);
                else
                    assert(false);
            }
            if (comm_rank == tgt) {
                assert(recv == GOAL_ID_NULL);
                recv = goal->Recv(datasize, vsrc);
            }
            q.push(tgt);
        }
    }

    if (comm_rank > 0) {
        if (recv != GOAL_ID_NULL) {
            if (send[0] != GOAL_ID_NULL)
                goal->Requires(send[0], recv);
            if (send[1] != GOAL_ID_NULL)
                goal->Requires(send[1], recv);
        }
    }

    /*

          Goal::t_id recv = -1, send = -1;
          for (int r = 0; r < ceil(log2(comm_size)); r++) {
                  int vpeer =  vrank+(int)pow(2,r);
                  int peer;
                  VRANK2RANK(peer, vpeer, root);
                  if ((vrank+pow(2,r) < comm_size) and (vrank < pow(2,r))) {
                          send = goal->Send(datasize, peer);
                  }
                  if ((send >= 0) and (recv >= 0)) {
                          goal->Requires(send, recv);
                  }
                  vpeer = vrank-(int)pow(2, r);
                  VRANK2RANK(peer, vpeer, root);
                  if ((vrank >= pow(2,r)) and (vrank < pow(2, r+1))) {
                          recv = goal->Recv(datasize, peer);
                  }
          }*/
}

void create_binary_tree_bcast(gengetopt_args_info *args_info) {

    int comm_size = args_info->commsize_arg;
    int datasize = args_info->datasize_arg;
    Goal goal(args_info, comm_size);

    for (int comm_rank = 0; comm_rank < comm_size; comm_rank++) {
        goal.StartRank(comm_rank);
        create_binary_tree_bcast_rank(&goal, args_info->root_arg, comm_rank,
                                      comm_size, datasize);
        goal.EndRank();
    }
    goal.Write();
}

void create_dissemination_rank(Goal *goal, int comm_rank, int comm_size,
                               int datasize) {
    int dist = 1 << 0;

    Goal::t_id recv = -1, send = -1;
    while (dist < comm_size) {
        send = goal->Send(datasize, (comm_rank + dist + comm_size) % comm_size);
        if (recv != -1)
            goal->Requires(send, recv);
        recv = goal->Recv(datasize, (comm_rank - dist + comm_size) % comm_size);

        dist <<= 1;
    }
}

void create_dissemination(gengetopt_args_info *args_info) {

    int comm_size = args_info->commsize_arg;
    int datasize = args_info->datasize_arg;
    Goal goal(args_info, comm_size);

    for (int comm_rank = 0; comm_rank < comm_size; comm_rank++) {
        goal.StartRank(comm_rank);
        create_dissemination_rank(&goal, comm_rank, comm_size, datasize);
        goal.EndRank();
    }
    goal.Write();
}

void create_binomial_tree_reduce_rank(Goal *goal, int root, int comm_rank,
                                      int comm_size, int datasize) {
    int vrank;
    RANK2VRANK(comm_rank, vrank, root);

    Goal::t_id send = -1, recv = -1;
    for (int r = 0; r < ceil(log2(comm_size)); r++) {
        int vpeer = vrank + (int)pow(2, r);
        int peer;
        VRANK2RANK(peer, vpeer, root);
        if ((vrank + pow(2, r) < comm_size) and (vrank < pow(2, r))) {
            recv = goal->Recv(datasize, peer);
        }
        if ((send >= 0) and (recv >= 0)) {
            goal->Requires(send, recv);
        }
        vpeer = vrank - (int)pow(2, r);
        VRANK2RANK(peer, vpeer, root);
        if ((vrank >= pow(2, r)) and (vrank < pow(2, r + 1))) {
            send = goal->Send(datasize, peer);
        }
    }
}

void create_binomial_tree_reduce(gengetopt_args_info *args_info) {

    int comm_size = args_info->commsize_arg;
    int datasize = args_info->datasize_arg;

    Goal goal(args_info, comm_size);

    for (int comm_rank = 0; comm_rank < comm_size; comm_rank++) {
        goal.StartRank(comm_rank);
        create_binomial_tree_reduce_rank(&goal, args_info->root_arg, comm_rank,
                                         comm_size, datasize);
        goal.EndRank();
    }
    goal.Write();
}

void create_pipelined_ring(gengetopt_args_info *args_info) {

    int comm_size = args_info->commsize_arg;
    int datasize = args_info->datasize_arg;
    int segmentsize = args_info->segmentsize_arg;

    Goal goal(args_info, comm_size);

    for (int comm_rank = 0; comm_rank < comm_size; comm_rank++) {
        goal.StartRank(comm_rank);
        int vrank;
        RANK2VRANK(comm_rank, vrank, args_info->root_arg);

        int num_rounds = (int)ceil((double)datasize / (double)segmentsize);

        std::vector<int> send(num_rounds, -1);
        std::vector<int> recv(num_rounds, -1);

        for (int r = 0; r < num_rounds; r++) {

            int recvpeer = vrank - 1;
            int sendpeer = vrank + 1;

            // how much data is transmitted in this round
            int psize = segmentsize;
            if (r == num_rounds - 1) {
                psize = datasize - segmentsize * r;
            }

            // recv (if we are a receiver)
            int vpeer = recvpeer;
            VRANK2RANK(recvpeer, vpeer, args_info->root_arg);
            if (recvpeer >= 0) {
                recv.at(r) = goal.Recv(psize, recvpeer);
            }

            // send (if we are a sender)
            vpeer = sendpeer;
            VRANK2RANK(sendpeer, vpeer, args_info->root_arg);
            if (sendpeer < comm_size) {
                send.at(r) = goal.Send(psize, sendpeer);
            }

            // we can not send data before we received it
            if ((send.at(r) > 0) and (recv.at(r) > 0)) {
                goal.Requires(send.at(r), recv.at(r));
            }
        }
        goal.EndRank();
        if (comm_rank == comm_size - 1)
            goal.Write();
    }
}

void create_scatter(gengetopt_args_info *args_info) {

    /**
     * In this pattern the root sends data to every other rank.
     */

    int comm_size = args_info->commsize_arg;
    int datasize = args_info->datasize_arg;

    Goal goal(args_info, comm_size);

    for (int comm_rank = 0; comm_rank < comm_size; comm_rank++) {
        goal.StartRank(comm_rank);
        int vrank;
        RANK2VRANK(comm_rank, vrank, args_info->root_arg);

        if (vrank == 0) {
            for (int i = 1; i < comm_size; i++) {
                int recvpeer = i;
                int vpeer = recvpeer;
                VRANK2RANK(recvpeer, vpeer, args_info->root_arg);
                goal.Send(datasize, recvpeer);
            }
        } else {
            int sendpeer = 0;
            int vpeer = sendpeer;
            VRANK2RANK(sendpeer, vpeer, args_info->root_arg);
            goal.Recv(datasize, sendpeer);
        }
        goal.EndRank();
    }
    goal.Write();
}

void create_gather(gengetopt_args_info *args_info) {

    /**
     * In this pattern every rank (except root) sends data to the root, which
     * has to receive it. Altough in MPI_Gather root sends data to himself, we
     * are not doing it that way here, because one can assume that a good MPI
     * imnplementation would just copy the data locally.
     */

    int comm_size = args_info->commsize_arg;
    int datasize = args_info->datasize_arg;

    Goal goal(args_info, comm_size);

    for (int comm_rank = 0; comm_rank < comm_size; comm_rank++) {
        goal.StartRank(comm_rank);
        int vrank;
        RANK2VRANK(comm_rank, vrank, args_info->root_arg);

        if (vrank == 0) {
            for (int i = 1; i < comm_size; i++) {
                int recvpeer = i;
                int vpeer = recvpeer;
                VRANK2RANK(recvpeer, vpeer, args_info->root_arg);
                goal.Recv(datasize, recvpeer);
            }
        } else {
            int sendpeer = 0;
            int vpeer = sendpeer;
            VRANK2RANK(sendpeer, vpeer, args_info->root_arg);
            goal.Send(datasize, sendpeer);
        }
        goal.EndRank();
    }
    goal.Write();
}

void create_pipelined_ring_dep(gengetopt_args_info *args_info) {

    int comm_size = args_info->commsize_arg;
    int datasize = args_info->datasize_arg;
    int segmentsize = args_info->segmentsize_arg;

    Goal goal(args_info, comm_size);

    for (int comm_rank = 0; comm_rank < comm_size; comm_rank++) {
        goal.StartRank(comm_rank);
        int vrank;
        RANK2VRANK(comm_rank, vrank, args_info->root_arg);

        int num_rounds = (int)ceil((double)datasize / (double)segmentsize);

        std::vector<int> send(num_rounds, -1);
        std::vector<int> recv(num_rounds, -1);

        for (int r = 0; r < num_rounds; r++) {

            int recvpeer = vrank - 1;
            int sendpeer = vrank + 1;

            // how much data is transmitted in this round
            int psize = segmentsize;
            if (r == num_rounds - 1) {
                psize = datasize - segmentsize * r;
            }

            // recv (if we are a receiver)
            int vpeer = recvpeer;
            VRANK2RANK(recvpeer, vpeer, args_info->root_arg);
            if (vpeer >= 0) {
                recv.at(r) = goal.Recv(psize, recvpeer);
            }

            // send (if we are a sender)
            vpeer = sendpeer;
            VRANK2RANK(sendpeer, vpeer, args_info->root_arg);
            if (vpeer < comm_size) {
                send.at(r) = goal.Send(psize, sendpeer);
            }

            // we can not send data before we received it
            if ((send.at(r) > 0) and (recv.at(r) > 0)) {
                goal.Requires(send.at(r), recv.at(r));
            }

            // ensure pipelining (only receive round r after r-1 send finished)
            if ((r > 0) and (send.at(r - 1) > 0) and (recv.at(r) > 0)) {
                goal.Requires(recv.at(r), send.at(r - 1));
            }
        }
        goal.EndRank();
        if (comm_rank == comm_size - 1)
            goal.Write();
    }
}

double mylog(double base, double x) { return log(x) / log(base); }

int max(int a, int b) {
    if (a > b)
        return a;
    return b;
}

int mymod(int a, int b) {
    // calculate a % b

    while (a < 0) {
        a += b;
    }
    return a % b;
}

void create_nway_dissemination(gengetopt_args_info *args_info) {

    int n = args_info->nway_arg;
    int comm_size = args_info->commsize_arg;
    int datasize = args_info->datasize_arg;

    Goal goal(args_info, comm_size);

    for (int comm_rank = 0; comm_rank < comm_size; comm_rank++) {

        goal.StartRank(comm_rank);

        std::vector<int> recv(n, -1);
        std::vector<int> send(n, -1);

        for (int r = 0; r < ceil(mylog(n + 1, comm_size)); r++) {
            for (int w = 1; w <= n; w++) {
                int sendpeer =
                        mymod(comm_rank + w * (int)pow(n + 1, r), comm_size);
                send[w - 1] = goal.Send(datasize, sendpeer);
            }
            if (r > 0) {
                int prev = recv[0];
                for (int w = 1; w < n; w++) {
                    int red = goal.Exec("redfunc", datasize);
                    goal.Requires(red, recv[w]);
                    goal.Requires(red, prev);
                    prev = red;
                }
                int red = goal.Exec("redfunc", datasize);
                goal.Requires(red, recv[0]);
                goal.Requires(send[n - 1], prev);
                for (int w = 1; w <= n; w++) {
                    goal.Requires(send[w - 1], red);
                }
            }
            for (int w = 1; w <= n; w++) {
                int recvpeer =
                        mymod(comm_rank - w * (int)pow(n + 1, r), comm_size);
                recv[w - 1] = goal.Recv(datasize, recvpeer);
            }
        }
        goal.EndRank();
    }
    goal.Write();
}

void create_double_ring(gengetopt_args_info *args_info) {

    int comm_size = args_info->commsize_arg;
    int datasize = args_info->datasize_arg;

    Goal goal(args_info, comm_size);

    for (int comm_rank = 0; comm_rank < comm_size; comm_rank++) {
        goal.StartRank(comm_rank);

        /* forward ring */
        int send =
                goal.Send(datasize, ((comm_rank + 1) + comm_size) % comm_size);
        int recv =
                goal.Recv(datasize, ((comm_rank - 1) + comm_size) % comm_size);
        if (comm_rank > 0)
            goal.Requires(send, recv);

        /* backward ring */
        send = goal.Send(datasize, ((comm_rank - 1) + comm_size) % comm_size);
        recv = goal.Recv(datasize, ((comm_rank + 1) + comm_size) % comm_size);
        if (comm_rank > 0)
            goal.Requires(send, recv);

        goal.EndRank();
    }
    goal.Write();
}

/* creates a linaer (central counter based) barrier algoritm */
void create_linbarrier(gengetopt_args_info *args_info) {

    int comm_size = args_info->commsize_arg;
    Goal goal(args_info, comm_size);

    // each rank
    for (int comm_rank = 0; comm_rank < comm_size; comm_rank++) {
        // beware - this goal object is stateful! only one rank at a time!
        goal.StartRank(comm_rank);

        if (comm_rank == 0) {
            // dummy operation to reduce number of dependencies from n*n to n+n
            int dummy = goal.Exec("redfunc", 0);
            for (int i = 1; i < comm_size; i++) {
                // rank 0 receives from all - and sends to all
                int recv = goal.Recv(1, i);
                int send = goal.Send(1, i);
                goal.Requires(dummy, recv);
                goal.Requires(send, dummy);
            }
        } else {
            goal.Send(1, 0);
            goal.Recv(1, 0);
        }

        // beware - this is also stateful and finishes a rank!
        goal.EndRank();
    }
    goal.Write();
}

void create_random_bisect(gengetopt_args_info *args_info) {

    int comm_size = args_info->commsize_arg;
    if (comm_size % 2 == 1)
        comm_size--;
    int datasize = args_info->datasize_arg;
    Goal goal(args_info, comm_size);

    MTRand mtrand;
    std::vector<int> peer(
            comm_size); // save the pairs (peer[i] is the peer of host i)
    std::vector<bool> used(comm_size, false); // mark the used peers

    // quick method to create a random pairing
    for (int counter = 0; counter < comm_size; counter++) {
        int myrand = mtrand.randInt(comm_size - counter - 1);
        int pos = 0;
        while (true) {
            // walk the used array (only the entries that are not used)
            if (used[pos] == false) {
                if (myrand == 0) {
                    used[pos] = true;
                    peer[counter] = pos; // save random value
                    break;
                }
                myrand--;
            }
            pos++;
            assert(pos < comm_size);
        }
    }

    // create the inverse array ...
    std::vector<int> inverse_peer(
            comm_size); // the inverse peer table (know who to receive from)
    for (int comm_rank = 0; comm_rank < comm_size; comm_rank++) {
        inverse_peer[peer[comm_rank]] = comm_rank;
    }

    for (int comm_rank = 0; comm_rank < comm_size; comm_rank++) {
        goal.StartRank(comm_rank);

        int dist = comm_size / 2;

        if (inverse_peer[comm_rank] < dist) {
            // this host is a sender
            goal.Send(datasize, peer[inverse_peer[comm_rank] + dist]);
        } else {
            // this host is a receiver
            goal.Recv(datasize, peer[inverse_peer[comm_rank] - dist]);
        }

        goal.EndRank();
    }
    goal.Write();
}

void create_random_bisect_fd_sym(gengetopt_args_info *args_info) {

    int comm_size = args_info->commsize_arg;
    if (comm_size % 2 == 1)
        comm_size--;
    int datasize = args_info->datasize_arg;
    Goal goal(args_info, comm_size);

    MTRand mtrand;
    std::vector<int> peer(
            comm_size); // save the pairs (peer[i] is the peer of host i)
    std::vector<bool> used(comm_size, false); // mark the used peers

    // quick method to create a random pairing
    for (int counter = 0; counter < comm_size; counter++) {
        int myrand = mtrand.randInt(comm_size - counter - 1);
        int pos = 0;
        while (true) {
            // walk the used array (only the entries that are not used)
            if (used[pos] == false) {
                if (myrand == 0) {
                    used[pos] = true;
                    peer[counter] = pos; // save random value
                    break;
                }
                myrand--;
            }
            pos++;
            assert(pos < comm_size);
        }
    }

    // create the inverse array ...
    std::vector<int> inverse_peer(
            comm_size); // the inverse peer table (know who to receive from)
    for (int comm_rank = 0; comm_rank < comm_size; comm_rank++) {
        inverse_peer[peer[comm_rank]] = comm_rank;
    }

    for (int comm_rank = 0; comm_rank < comm_size; comm_rank++) {
        goal.StartRank(comm_rank);

        int dist = comm_size / 2;

        if (inverse_peer[comm_rank] < dist) {
            // this host is a sender
            goal.Send(datasize, peer[inverse_peer[comm_rank] + dist]);
            goal.Recv(datasize, peer[inverse_peer[comm_rank] + dist]);
        } else {
            // this host is a receiver
            goal.Recv(datasize, peer[inverse_peer[comm_rank] - dist]);
            goal.Send(datasize, peer[inverse_peer[comm_rank] - dist]);
        }

        goal.EndRank();
    }
    goal.Write();
}

void create_linear_alltoall_rank(Goal *goal, int src_rank, int comm_size,
                                 int datasize) {
    for (int step = 1; step < comm_size; step++) {
        int send_to = (src_rank + step) % comm_size;
        int recv_from = mymod(src_rank - step, comm_size);
        goal->Send(datasize, send_to);
        goal->Recv(datasize, recv_from);
    }
}

void create_linear_alltoall(gengetopt_args_info *args_info) {
    int comm_size = args_info->commsize_arg;
    int datasize = args_info->datasize_arg;

    Goal goal(args_info, comm_size);

    for (int src_rank = 0; src_rank < comm_size; src_rank++) {
        goal.StartRank(src_rank);
        create_linear_alltoall_rank(&goal, src_rank, comm_size, datasize);
        goal.EndRank();
    }
    goal.Write();
}

void create_linear_alltoallv_rank(Goal *goal, int src_rank, int comm_size,
                                  std::vector<std::vector<int>> &sizes) {
    for (int step = 1; step < comm_size; step++) {
        int send_to = (src_rank + step) % comm_size;
        int recv_from = mymod(src_rank - step, comm_size);
        goal->Send(sizes[src_rank][send_to], send_to);
        goal->Recv(sizes[recv_from][src_rank], recv_from);
    }
}

void create_linear_alltoallv(gengetopt_args_info *args_info) {
    int comm_size = args_info->commsize_arg;
    int datasize = args_info->datasize_arg;

    Goal goal(args_info, comm_size);
    std::vector<std::vector<int>> sizes(comm_size);
    // Generate random sizes
    for (int i = 0; i < comm_size; i++) {
        sizes[i].reserve(comm_size);
        for (int j = 0; j < comm_size; j++) {
            if (j == args_info->root_arg) {
                sizes[i][j] = datasize;
            } else {
                sizes[i][j] =
                        (rand() % (datasize / args_info->a2av_skew_ratio_arg)) +
                        1; // +1 to avoid sending 0 bytes
            }
        }
    }

    for (int src_rank = 0; src_rank < comm_size; src_rank++) {
        goal.StartRank(src_rank);
        create_linear_alltoallv_rank(&goal, src_rank, comm_size, sizes);
        goal.EndRank();
    }
    goal.Write();
}

void create_allreduce_recdoub_rank(Goal *goal, int src_rank, int comm_size,
                                   int datasize, int replace_comptime) {
    int mask = 0x1;
    int next_datasize = datasize;
    int last_recv = -1;
    int send_id = -1;
    int num_steps_per_phase = log2(comm_size);
    // Reduce-scatter
    for (int step = 0; step < num_steps_per_phase; step++) {
        int dest = src_rank ^ mask;
        next_datasize /= 2;
        send_id = goal->Send(next_datasize, dest);
        if (last_recv != -1) {
            goal->Requires(send_id, last_recv);
        }
        last_recv = goal->Recv(next_datasize, dest);
        if (replace_comptime != -1) {
            // By doing so, next send will have a dependencies on the compute
            // rather than on the recv. This allows us simulating a fixed
            // intermsg gap rather than an actual dependency.
            last_recv = goal->Exec("intermsg-gap", replace_comptime, 0);
        }
        mask <<= 1;
    }

    // Allgather
    mask >>= 1;
    for (int step = 0; step < num_steps_per_phase; step++) {
        int dest = src_rank ^ mask;
        send_id = goal->Send(next_datasize, dest);
        if (last_recv != -1) {
            goal->Requires(send_id, last_recv);
        }
        last_recv = goal->Recv(next_datasize, dest);
        if (replace_comptime != -1 && step != num_steps_per_phase - 1) {
            // By doing so, next send will have a dependencies on the compute
            // rather than on the recv. This allows us simulating a fixed
            // intermsg gap rather than an actual dependency.
            last_recv = goal->Exec("intermsg-gap", replace_comptime, 0);
        }
        next_datasize *= 2;
        mask >>= 1;
    }
}

void create_allreduce_recdoub(gengetopt_args_info *args_info) {
    int comm_size = args_info->commsize_arg;
    int datasize = args_info->datasize_arg;
    int replace_comptime = args_info->rpl_dep_cmp_arg;

    Goal goal(args_info, comm_size);

    for (int src_rank = 0; src_rank < comm_size; src_rank++) {
        goal.StartRank(src_rank);
        create_allreduce_recdoub_rank(&goal, src_rank, comm_size, datasize,
                                      replace_comptime);
        goal.EndRank();
    }
    goal.Write();
}

void create_allreduce_ring_rank(Goal *goal, int src_rank, int comm_size,
                                int datasize) {
    int last_recv = -1;
    int send_id = -1;
    int chunk_size = datasize / comm_size;
    // Phase 0: reduce-scatter, phase 1: allgather
    for (int phase = 0; phase < 2; phase++) {
        for (int step = 0; step < comm_size - 1; step++) {
            int send_to = (src_rank + 1) % comm_size;
            int recv_from = mymod(src_rank - 1, comm_size);
            send_id = goal->Send(chunk_size, send_to);
            if (last_recv != -1) {
                goal->Requires(send_id, last_recv);
            }
            last_recv = goal->Recv(chunk_size, recv_from);
        }
    }
}

void create_allreduce_ring(gengetopt_args_info *args_info) {
    int comm_size = args_info->commsize_arg;
    int datasize = args_info->datasize_arg;

    Goal goal(args_info, comm_size);

    for (int src_rank = 0; src_rank < comm_size; src_rank++) {
        goal.StartRank(src_rank);
        create_allreduce_ring_rank(&goal, src_rank, comm_size, datasize);
        goal.EndRank();
    }
    goal.Write();
}

void create_resnet152(gengetopt_args_info *args_info) {

    int collsbase = 100000; // needed to create tag, must be higher than the
                            // send/recvs in this schedule (0 here)
    int nops = 0; // running count of colls for collective tag matching
    int comm = 1; // only one comm used here
    int comm_size = args_info->commsize_arg;
    Goal goal(args_info, comm_size);

// The recipe for this was taken from https://github.com/spcl/DNN-cpp-proxies
// 1d32dce allreduce sizes for gradients with message aggregation
#define NUM_B 10
    int allreduce_sizes[NUM_B] = {6511592, 6567936, 5905920, 6113280, 6176256,
                                  6112768, 6176256, 6112768, 5321216, 5194816};
    // batchsize = 128
    // Suggest world_size <= 256, which is corresponding to a global batch_size
    // <= 32 K A100 GPU runtime in us (10E-6) for each iteration
    int fwd_rt_whole_model = 119000;
    int bwd_rt_per_B = 23800;

    for (int src_rank = 0; src_rank < comm_size; src_rank++) {
        goal.StartRank(src_rank);
        int fwd_cmp =
                goal.Exec("forward_compute", fwd_rt_whole_model, 0); // compute
        for (int i = 0; i < NUM_B; i++) {
            // omitted progressing of MPI using Testany, no effect in goal
            int bkw_cmp = goal.Exec("backward_compute", bwd_rt_per_B, 0);
            goal.Requires(bkw_cmp, fwd_cmp);

            int dummy = goal.Exec("backward_compute_dummy", 0, 0);
            goal.Requires(dummy, bkw_cmp);

            // MPI_Iallreduce(allreduce_size[i], MPI_FLOAT, MPI_SUM,
            // MPI_COMM_WORLD, req[i]);
            goal.Comment("Iallreduce begin");
            goal.SetTag(MAKE_TAG(comm, (collsbase + nops)));
            goal.StartOp();
            create_dissemination_rank(&goal, src_rank, comm_size,
                                      allreduce_sizes[i] * 4);
            std::pair<Goal::locop, Goal::locop> ops = goal.EndOp();
            Goal::locop::iterator it;
            for (it = ops.second.begin(); it != ops.second.end(); it++) {
                goal.Requires(it->first, dummy);
            }
            goal.Comment("Iallreduce end");
            nops++;
        }
        // MPI_Waitall(req);
        goal.EndRank();
    }
    goal.Write();
}

void create_multiple_permutation(gengetopt_args_info *args_info) {

    int comm_size = args_info->commsize_arg;
    int datasize = args_info->datasize_arg;
    int num_perms = 6;

    Goal goal(args_info, comm_size);
    MTRand mtrand;

    int sending[comm_size][num_perms];
    int sending_tag[comm_size][num_perms];
    std::vector<std::vector<int>> receiving(comm_size);
    std::vector<std::vector<int>> receiving_tag(comm_size);

    for (int src_rank = 0; src_rank < comm_size; src_rank++) {
        for (int iter = 0; iter < num_perms; iter++) {
            sending[src_rank][iter] = -1;
            // receiving[src_rank].push_back(-1);
        }
    }

    for (int iter = 0; iter < num_perms; iter++) {
        for (int src_rank = 0; src_rank < comm_size; src_rank++) {
            int myrand = mtrand.randInt(comm_size - 1);
            while (myrand == src_rank) {
                myrand = mtrand.randInt(comm_size - 1);
            }
            sending[src_rank][iter] = myrand;
            sending_tag[src_rank][iter] = iter;
            receiving[myrand].push_back(src_rank);
            receiving_tag[myrand].push_back(iter);
        }
    }

    for (int src_rank = 0; src_rank < comm_size; src_rank++) {
        goal.StartRank(src_rank);
        for (int iter = 0; iter < num_perms; iter++) {
            if (sending[src_rank][iter] != -1) {
                goal.SetTag(sending_tag[src_rank][iter]);
                int send = goal.Send(datasize, sending[src_rank][iter]);
            }
        }
        for (int j = 0; j < receiving[src_rank].size(); j++) {
            printf("Src %d - From %d - Tag %d\n", src_rank,
                   receiving[src_rank][j], receiving_tag[src_rank][j]);
            if (receiving[src_rank][j] != -1) {
                goal.SetTag(receiving_tag[src_rank][j]);
                int send = goal.Recv(datasize, receiving[src_rank][j]);
            }
        }
        goal.EndRank();
    }
    goal.Write();
}

void create_permutation(gengetopt_args_info *args_info) {

    int comm_size = args_info->commsize_arg;
    int datasize = args_info->datasize_arg;

    Goal goal(args_info, comm_size);
    MTRand mtrand;

    int sending[comm_size];
    int receiving[comm_size];
    for (int i = 0; i < comm_size; i++) {
        sending[i] = -1;
        receiving[i] = -1;
    }

    for (int src_rank = 0; src_rank < comm_size; src_rank++) {
        int myrand = mtrand.randInt(comm_size - 1);
        while (myrand == src_rank || receiving[myrand] != -1) {
            myrand = mtrand.randInt(comm_size - 1);
        }
        sending[src_rank] = myrand;
        receiving[myrand] = src_rank;
    }

    for (int src_rank = 0; src_rank < comm_size; src_rank++) {
        goal.StartRank(src_rank);
        int send = goal.Send(datasize, sending[src_rank]);
        int recv = goal.Recv(datasize, receiving[src_rank]);
        goal.EndRank();
    }

    goal.Write();
}

void create_permutation_across(gengetopt_args_info *args_info) {

    int comm_size = args_info->commsize_arg;
    int datasize = args_info->datasize_arg;

    Goal goal(args_info, comm_size);
    MTRand mtrand;

    int sending[comm_size];
    int receiving[comm_size];
    for (int i = 0; i < comm_size; i++) {
        sending[i] = -1;
        receiving[i] = -1;
    }

    for (int src_rank = 0; src_rank < comm_size / 2; src_rank++) {
        int myrand = mtrand.randInt(comm_size - 1);
        while (myrand == src_rank || receiving[myrand] != -1 ||
               myrand < comm_size / 2) {
            myrand = mtrand.randInt(comm_size - 1);
        }
        sending[src_rank] = myrand;
        receiving[myrand] = src_rank;
    }

    for (int src_rank = comm_size / 2; src_rank < comm_size; src_rank++) {
        int myrand = mtrand.randInt(comm_size - 1);
        while (myrand == src_rank || receiving[myrand] != -1 ||
               myrand >= comm_size / 2) {
            myrand = mtrand.randInt(comm_size - 1);
        }
        sending[src_rank] = myrand;
        receiving[myrand] = src_rank;
    }

    for (int src_rank = 0; src_rank < comm_size; src_rank++) {
        goal.StartRank(src_rank);
        int send = goal.Send(datasize, sending[src_rank]);
        int recv = goal.Recv(datasize, receiving[src_rank]);
        goal.EndRank();
    }

    goal.Write();
}

void create_permutation_across_m(gengetopt_args_info *args_info) {

    int comm_size = args_info->commsize_arg;
    int datasize = args_info->datasize_arg;
    int perm = 3;

    Goal goal(args_info, comm_size);
    MTRand mtrand;
    int sending[comm_size][perm];
    int receiving[comm_size][perm];

    for (int j = 0; j < comm_size; j++) {
        for (int k = 0; k < perm; k++) {
            sending[j][k] = -1;
            receiving[j][k] = -1;
        }
    }

    for (int i = 0; i < perm; i++) {
        for (int src_rank = 0; src_rank < comm_size / 2; src_rank++) {
            int myrand = mtrand.randInt(comm_size - 1);
            while (myrand == src_rank || receiving[myrand][i] != -1 ||
                   myrand < comm_size / 2) {
                myrand = mtrand.randInt(comm_size - 1);
            }
            sending[src_rank][i] = myrand;
            receiving[myrand][i] = src_rank;
        }

        for (int src_rank = comm_size / 2; src_rank < comm_size; src_rank++) {
            int myrand = mtrand.randInt(comm_size - 1);
            while (myrand == src_rank || receiving[myrand][i] != -1 ||
                   myrand >= comm_size / 2) {
                myrand = mtrand.randInt(comm_size - 1);
            }
            sending[src_rank][i] = myrand;
            receiving[myrand][i] = src_rank;
        }
    }

    for (int src_rank = 0; src_rank < comm_size; src_rank++) {
        goal.StartRank(src_rank);
        for (int k = 0; k < perm; k++) {
            goal.SetTag(k);
            int send = goal.Send(datasize, sending[src_rank][k]);
            int recv = goal.Recv(datasize, receiving[src_rank][k]);
        }
        goal.EndRank();
    }

    goal.Write();
}

// Everyone sends to last node
void create_incast(gengetopt_args_info *args_info) {

    int comm_size = args_info->commsize_arg;
    int datasize = args_info->datasize_arg;

    Goal goal(args_info, comm_size);

    for (int src_rank = 0; src_rank < comm_size - 1; src_rank++) {
        goal.StartRank(src_rank);
        int send = goal.Send(datasize, comm_size - 1);
        goal.EndRank();
    }

    goal.StartRank(comm_size - 1);
    for (int src_rank = 0; src_rank < comm_size - 1; src_rank++) {
        int recv = goal.Recv(datasize, src_rank);
    }
    goal.EndRank();

    goal.Write();
}

// Everyone sends to last node
void create_special_incast(gengetopt_args_info *args_info) {

    int comm_size = args_info->commsize_arg;
    int datasize = args_info->datasize_arg;
    int active_nodes = 4;

    Goal goal(args_info, comm_size);

    for (int src_rank = 0; src_rank < comm_size / 2; src_rank++) {
        goal.StartRank(src_rank);
        if (src_rank < active_nodes) {
            int send = goal.Send(datasize, comm_size / 2);
        }
        goal.EndRank();
    }

    goal.StartRank(comm_size / 2);
    for (int src_rank = 0; src_rank < active_nodes; src_rank++) {
        int recv = goal.Recv(datasize, src_rank);
    }
    goal.EndRank();

    for (int src_rank = comm_size / 2 + 1; src_rank < comm_size; src_rank++) {
        goal.StartRank(src_rank);
        goal.EndRank();
    }

    goal.Write();
}

// Everyone sends to last node
void create_outcast_incast(gengetopt_args_info *args_info) {

    int comm_size = args_info->commsize_arg;
    int datasize = args_info->datasize_arg;
    int active_nodes = 8;

    Goal goal(args_info, comm_size);

    for (int src_rank = 0; src_rank < active_nodes; src_rank++) {
        goal.StartRank(src_rank);
        for (int j = 0; j < active_nodes; j++) {
            int send = goal.Send(datasize, comm_size / 2 + j);
        }
        goal.EndRank();
    }

    /*for (int src_rank = active_nodes; src_rank < comm_size / 2; src_rank++) {
        goal.StartRank(src_rank);
        goal.EndRank();
    }*/

    int update_every = (comm_size / 2 - active_nodes) / active_nodes;
    int start_from = 0;
    int index_2 = 0;
    for (int src_rank = active_nodes; src_rank < comm_size / 2; src_rank++) {
        goal.StartRank(src_rank);
        if (start_from >= update_every) {
            start_from = 0;
            index_2++;
        }
        int send = goal.Send(datasize, comm_size / 2 + index_2);
        start_from++;
        goal.EndRank();
    }

    update_every = (comm_size / 2 - active_nodes) / active_nodes;
    start_from = 0;
    index_2 = 0;
    for (int src_rank = 0; src_rank < active_nodes; src_rank++) {
        goal.StartRank(comm_size / 2 + src_rank);
        // Outcast ones
        for (int j = 0; j < active_nodes; j++) {
            int recv = goal.Recv(datasize, 0 + j);
        }
        // Others
        for (int j = active_nodes; j < active_nodes + update_every; j++) {
            int recv = goal.Recv(datasize, j + (src_rank * update_every));
        }
        goal.EndRank();
    }

    for (int src_rank = comm_size / 2 + active_nodes; src_rank < comm_size;
         src_rank++) {
        goal.StartRank(src_rank);
        goal.EndRank();
    }

    goal.Write();
}

// Everyone sends to last node
void create_single_flow(gengetopt_args_info *args_info) {

    int comm_size = args_info->commsize_arg;
    int datasize = args_info->datasize_arg;

    Goal goal(args_info, comm_size);

    for (int i = 0; i < comm_size; i++) {
        goal.StartRank(i);
        if (i == 0) {
            int send = goal.Send(datasize, comm_size / 2);
        } else if (i == comm_size / 2) {
            int recv = goal.Recv(datasize, 0);
        }
        goal.EndRank();
    }

    goal.Write();
}

void create_chained_dissem(gengetopt_args_info *args_info) {

    int collsbase = 100000; // needed to create tag, must be higher than the
                            // send/recvs in this schedule (0 here)
    int comm = 1;           // only one comm used here
    int comm_size = args_info->commsize_arg;
    int NUM_RUNS = 1;
    Goal goal(args_info, comm_size);

    for (int src_rank = 0; src_rank < comm_size; src_rank++) {
        goal.StartRank(src_rank);
        int oldmarker = -1;
        int nops = 0; // running count of colls for collective tag matching
        for (int i = 0; i < NUM_RUNS; i++) {
            goal.Comment("Iallreduce begin");
            goal.SetTag(MAKE_TAG(comm, (collsbase + nops)));
            goal.StartOp();
            create_dissemination_rank(&goal, src_rank, comm_size,
                                      args_info->datasize_arg);
            std::pair<Goal::locop, Goal::locop> ops = goal.EndOp();
            Goal::locop::iterator it;
            goal.Comment("Iallreduce end");
            nops++;
            int marker = goal.Send(0, 9999999);
            if (oldmarker != -1) {
                for (it = ops.first.begin(); it != ops.first.end(); it++) {
                    goal.Requires(it->first, oldmarker);
                }
            }
            for (it = ops.second.begin(); it != ops.second.end(); it++) {
                goal.Requires(marker, it->first);
            }
            oldmarker = marker;
        }
        goal.EndRank();
    }
    goal.Write();
}

int main(int argc, char **argv) {

    gengetopt_args_info args_info;

    if (cmdline_parser(argc, argv, &args_info) != 0) {
        fprintf(stderr, "Couldn't parse command line arguments!\n");
        exit(EXIT_FAILURE);
    }

    if (strcmp(args_info.ptrn_arg, "binarytreebcast") == 0) {
        create_binary_tree_bcast(&args_info);
    }
    if (strcmp(args_info.ptrn_arg, "binomialtreebcast") == 0) {
        create_binomial_tree_bcast(&args_info);
    }
    if (strcmp(args_info.ptrn_arg, "binomialtreereduce") == 0) {
        create_binomial_tree_reduce(&args_info);
    }
    if (strcmp(args_info.ptrn_arg, "nwaydissemination") == 0) {
        create_nway_dissemination(&args_info);
    }
    if (strcmp(args_info.ptrn_arg, "pipelinedring") == 0) {
        create_pipelined_ring(&args_info);
    }
    if (strcmp(args_info.ptrn_arg, "pipelinedringdep") == 0) {
        create_pipelined_ring_dep(&args_info);
    }
    if (strcmp(args_info.ptrn_arg, "doublering") == 0) {
        create_double_ring(&args_info);
    }
    if (strcmp(args_info.ptrn_arg, "gather") == 0) {
        create_gather(&args_info);
    }
    if (strcmp(args_info.ptrn_arg, "scatter") == 0) {
        create_scatter(&args_info);
    }
    if (strcmp(args_info.ptrn_arg, "linbarrier") == 0) {
        create_linbarrier(&args_info);
    }
    if (strcmp(args_info.ptrn_arg, "dissemination") == 0) {
        create_dissemination(&args_info);
    }
    if (strcmp(args_info.ptrn_arg, "random_bisect") == 0) {
        create_random_bisect(&args_info);
    }
    if (strcmp(args_info.ptrn_arg, "random_bisect_fd_sym") == 0) {
        create_random_bisect_fd_sym(&args_info);
    }
    if (strcmp(args_info.ptrn_arg, "linear_alltoall") == 0) {
        create_linear_alltoall(&args_info);
    }
    if (strcmp(args_info.ptrn_arg, "linear_alltoallv") == 0) {
        create_linear_alltoallv(&args_info);
    }
    if (strcmp(args_info.ptrn_arg, "allreduce_recdoub") == 0) {
        create_allreduce_recdoub(&args_info);
    }
    if (strcmp(args_info.ptrn_arg, "allreduce_ring") == 0) {
        create_allreduce_ring(&args_info);
    }
    if (strcmp(args_info.ptrn_arg, "resnet152") == 0) {
        create_resnet152(&args_info);
    }
    if (strcmp(args_info.ptrn_arg, "chained_dissem") == 0) {
        create_chained_dissem(&args_info);
    }
    if (strcmp(args_info.ptrn_arg, "permutation") == 0) {
        create_permutation(&args_info);
    }
    if (strcmp(args_info.ptrn_arg, "permutation_across") == 0) {
        create_permutation_across(&args_info);
    }
    if (strcmp(args_info.ptrn_arg, "incast") == 0) {
        create_incast(&args_info);
    }
    if (strcmp(args_info.ptrn_arg, "special_incast") == 0) {
        create_special_incast(&args_info);
    }
    if (strcmp(args_info.ptrn_arg, "multiple_permutation") == 0) {
        create_permutation_across_m(&args_info);
    }
    if (strcmp(args_info.ptrn_arg, "outcast_incast") == 0) {
        create_outcast_incast(&args_info);
    }
    if (strcmp(args_info.ptrn_arg, "single_flow") == 0) {
        create_single_flow(&args_info);
    }

    if (strcmp(args_info.ptrn_arg, "trace") == 0) {
        // see process_trace.cpp
        process_trace(&args_info);
    }

    cmdline_parser_free(&args_info);
    exit(EXIT_SUCCESS);
}
