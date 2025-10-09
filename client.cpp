/*
	Original author of the starter code
    Tanzir Ahmed
    Department of Computer Science & Engineering
    Texas A&M University
    Date: 2/8/20
	
	Please include your Name, UIN, and the date below
	Name: Amel Simon
	UIN: 234006812
	Date: 9/28/2025
*/
#include "common.h"
#include "FIFORequestChannel.h"

#include <sys/wait.h>

using namespace std;


int main (int argc, char *argv[]) {
	int opt;
    int p = -1;         //person
    double t = -1.0;    //time
    int e = -1;         //ecg number
    string filename = "";
    int m = MAX_MESSAGE;   // buffer capacity
    bool newchannel = false;

    // declare here so scope isnt limited
    FIFORequestChannel* newchan = nullptr;

    // Parse command line args
    while ((opt = getopt(argc, argv, "p:t:e:f:m:c")) != -1) {
        switch (opt) {
            case 'p': p = atoi(optarg); break;
            case 't': t = atof(optarg); break;
            case 'e': e = atoi(optarg); break;
            case 'f': filename = optarg; break;
            case 'm': m = atoi(optarg); break;
            case 'c': newchannel = true; break;
        }
    }

	// --- Step 1: fork + exec server ---
    pid_t pid = fork();
    if (pid == 0) {
        string mstr = to_string(m);
        char* args[] = {(char*)"./server", (char*)"-m", (char*)mstr.c_str(), nullptr};
        execvp(args[0], args);
        perror("execvp failed");
        exit(1);
    }

    // parent = client
    FIFORequestChannel* chan = new FIFORequestChannel("control", FIFORequestChannel::CLIENT_SIDE);

    // --- Step 2: if new channel requested ---
    if (newchannel) {
        MESSAGE_TYPE nc = NEWCHANNEL_MSG;
        chan->cwrite(&nc, sizeof(MESSAGE_TYPE));
        char namebuf[100];
        chan->cread(namebuf, sizeof(namebuf));
        newchan = new FIFORequestChannel(namebuf, FIFORequestChannel::CLIENT_SIDE);

    }

    // --- Step 3: Handle requests ---
    FIFORequestChannel* active_chan;
    if(newchan){
        active_chan = newchan;
    }
    else{
        active_chan = chan;
    }


    if (p != -1 && t >= 0.0 && e != -1) {       //if person is set, time is valid, and ecg value valid, (no filename listed)
        // Single data point
        datamsg d(p, t, e);
        active_chan->cwrite(&d, sizeof(d));
        double reply;
        active_chan->cread(&reply, sizeof(double));
        cout << "For person " << p << ", at time " << t << ", the value of ecg " << e
             << " is " << reply << endl;

    } else if (p != -1 && t < 0.0 && e == -1 && filename.empty()) {     // person is set, but time not valid, ecg invalid, and no filename (1000 points)
        // First 1000 points -> x1.csv
        ofstream outfile("received/x1.csv");
        for (int i = 0; i < 1000; i++) {
            double ti = i * 0.004; // ti is 4 ms increments
            outfile << ti << ",";
            for (int ecg = 1; ecg <= 2; ecg++) {        // person p @ time ti: ecg1 and ecg2 data
                datamsg d(p, ti, ecg);
                active_chan->cwrite(&d, sizeof(d));
                double resp;
                active_chan->cread(&resp, sizeof(double));
                outfile << resp;
                if (ecg == 1) outfile << ",";
            }
            outfile << "\n";
        }
        outfile.close();

    } else if (!filename.empty()) {     //person is not set ==> file request
        // File request
        string outname = "received/" + filename;
        system("mkdir -p received"); // make sure directory exists

        // First request: file size
        filemsg fm(0,0);
        int len = sizeof(filemsg) + filename.size() + 1;
        char* buf = new char[len];
        memcpy(buf, &fm, sizeof(filemsg));
        strcpy(buf + sizeof(filemsg), filename.c_str());
        active_chan->cwrite(buf, len);

        __int64_t filesize;
        active_chan->cread(&filesize, sizeof(__int64_t));

        ofstream of(outname, ios::binary);
        __int64_t offset = 0;
        while (offset < filesize) {
            int chunk = min((__int64_t)m, filesize - offset);
            filemsg fm(offset, chunk);
            memcpy(buf, &fm, sizeof(filemsg));
            strcpy(buf + sizeof(filemsg), filename.c_str());
            active_chan->cwrite(buf, len);

            vector<char> response(chunk);
            active_chan->cread(response.data(), chunk);
            of.write(response.data(), chunk);
            offset += chunk;
        }
        of.close();
        delete[] buf;
    }

    // --- Step 4: clean up ---
    MESSAGE_TYPE q = QUIT_MSG;

    // quit new channel first (if it exists)
    if (newchan) {
        newchan->cwrite(&q, sizeof(MESSAGE_TYPE));
        delete newchan;
    }

    // quit control channel last
    chan->cwrite(&q, sizeof(MESSAGE_TYPE));
    delete chan;

    wait(nullptr); // wait for server
    return 0;
}


// chan (FIFORequestChannel): bidirectional pipe
// fifo_control1: Server writes → Client reads
// fifo_control2: Client writes → Server reads
