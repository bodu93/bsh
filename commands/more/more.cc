#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/ioctl.h>
#include "../include/error.h"

class TtyConsole {
private:
    mutable struct termios oldconf_;
    int ttyfd_ = -1;

    void set() const {
        struct termios conf;
        tcgetattr(ttyfd_, &conf);
        oldconf_ = conf;
        conf.c_lflag &= ~(ICANON | ECHO);
        tcsetattr(ttyfd_, TCSANOW, &conf);
    }

    void reset() const {
        tcsetattr(ttyfd_, TCSANOW, &oldconf_);
    }
        

public:
    explicit TtyConsole(const char* ttypath) {
        ttyfd_ = open(ttypath, O_RDONLY);
        set();
    }

    ~TtyConsole() {
        tcsetattr(ttyfd_, TCSANOW, &oldconf_);
        close(ttyfd_);
    }

    TtyConsole(const TtyConsole&) = delete;
    TtyConsole& operator=(const TtyConsole&) = delete;

    int readInt() const {
        int result = 0;
        read(ttyfd_, &result, 1);
        return result;
    }

    void writeBell() const {
        reset();
        
        int c = '\07';
        write(ttyfd_, &c, 1);

        set();
    }
};

struct WindowSize { 
    // char for unit
    unsigned short row = 0;
    unsigned short column = 0;
};

struct WindowSize getWindowSize(int fd) {
    struct winsize ws; 
    if (ioctl(fd, TIOCGWINSZ, (char*)&ws) < 0)
        err_sys("TIOCGWINSZ error");

    WindowSize windowSize;
    windowSize.row = ws.ws_row;
    windowSize.column = ws.ws_col;
    return windowSize;
}

int wantMore(int c, int pagelen) {
    int reply = 0;
    if (c == ' ') // space output just one line
        reply = pagelen;
    else if (c == '\n')
        reply = 1;
    else if (c == 'q')
        reply = -1;
    else
        reply = 0;

    return reply;
}

int howMuchOutputed(FILE* fp) {
    return 80;
}

void printMore(const TtyConsole& tc, FILE* fp) {
    struct WindowSize ws = getWindowSize(STDIN_FILENO);

    const int pagerow = ws.row;
    const int pagecol = ws.column;

    char* linebuf = new char[pagecol];
    int outlines = 0;
    while (fgets(linebuf, pagecol + 1, fp)) {
        if (outlines == pagerow) {
            printf("\33[7m--More--\33[m");
            int readFromUser = tc.readInt();
            int reply;
            while ((reply = wantMore(readFromUser, pagerow)) == 0)
                readFromUser = tc.readInt();
            if (-1 == reply) break;

            outlines -= reply;
        }

        //printf("\r");
        fputs(linebuf, stdout);
        ++outlines;
    }

    delete[] linebuf;
}

int main(int argc, char *argv[]) {
    TtyConsole tc("/dev/tty");

    if (argc == 1) {
        printMore(tc, stdin);
        return 0;
    }

    for (int i = 1; i != argc; i++) {
        FILE* fp = fopen(argv[i], "r");
        if (!fp) {
            err_quit("open %s error", argv[1]);
        }
        printMore(tc, fp);
        fclose(fp);
    }

    return 0;
}
