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
    int p = -1;
    double t = -1.0;
    int e = -1;
    string filename = "";
    int m = MAX_MESSAGE;   // buffer capacity
    bool newchannel = false;

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
        FIFORequestChannel* newchan = new FIFORequestChannel(namebuf, FIFORequestChannel::CLIENT_SIDE);
        delete chan;
        chan = newchan; // use the new channel
    }

    // --- Step 3: Handle requests ---
    if (p != -1 && t >= 0.0 && e != -1) {
        // Single data point
        datamsg d(p, t, e);
        chan->cwrite(&d, sizeof(d));
        double reply;
        chan->cread(&reply, sizeof(double));
        cout << "For person " << p << ", at time " << t << ", ecg " << e
             << " = " << reply << endl;

    } else if (p != -1 && t < 0.0 && e == -1 && filename.empty()) {
        // First 1000 points -> x1.csv
        ofstream outfile("x1.csv");
        for (int i = 0; i < 1000; i++) {
            double ti = i * 0.004; // 4 ms steps
            outfile << ti << ",";
            for (int ecg = 1; ecg <= 2; ecg++) {
                datamsg d(p, ti, ecg);
                chan->cwrite(&d, sizeof(d));
                double resp;
                chan->cread(&resp, sizeof(double));
                outfile << resp;
                if (ecg == 1) outfile << ",";
            }
            outfile << "\n";
        }
        outfile.close();

    } else if (!filename.empty()) {
        // File request
        string outname = "received/" + filename;
        system("mkdir -p received"); // make sure directory exists

        // First request: file size
        filemsg fm(0,0);
        int len = sizeof(filemsg) + filename.size() + 1;
        char* buf = new char[len];
        memcpy(buf, &fm, sizeof(filemsg));
        strcpy(buf + sizeof(filemsg), filename.c_str());
        chan->cwrite(buf, len);

        __int64_t filesize;
        chan->cread(&filesize, sizeof(__int64_t));

        ofstream of(outname, ios::binary);
        __int64_t offset = 0;
        while (offset < filesize) {
            int chunk = min((__int64_t)m, filesize - offset);
            filemsg fm(offset, chunk);
            memcpy(buf, &fm, sizeof(filemsg));
            strcpy(buf + sizeof(filemsg), filename.c_str());
            chan->cwrite(buf, len);

            vector<char> response(chunk);
            chan->cread(response.data(), chunk);
            of.write(response.data(), chunk);
            offset += chunk;
        }
        of.close();
        delete[] buf;
    }

    // --- Step 4: clean up ---
    MESSAGE_TYPE q = QUIT_MSG;
    chan->cwrite(&q, sizeof(MESSAGE_TYPE));
    delete chan;

    wait(nullptr); // wait for server
    return 0;
}