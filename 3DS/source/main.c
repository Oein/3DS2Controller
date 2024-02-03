#include <stdio.h>
#include <stdlib.h>
#include <string.h>
// #include <malloc.h>
#include <errno.h>
#include <stdarg.h>
#include <unistd.h>

#include <fcntl.h>

#include <sys/types.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <3ds.h>

#define SOC_ALIGN       0x1000
#define SOC_BUFFERSIZE  0x100000

static u32 *SOC_buffer = NULL;
s32 sock = -1, csock = -1;

int ret;

u32	clientlen;
struct sockaddr_in client;
struct sockaddr_in server;
char temp[1026];

__attribute__((format(printf,1,2)))
void failExit(const char *fmt, ...);

//---------------------------------------------------------------------------------
void socShutdown() {
//---------------------------------------------------------------------------------
	printf("waiting for socExit...\n");
	socExit();
}

char boolByteArrayToCharByte(bool *arr) {
    char ret = 0;
    for(int i = 0; i < 8; i++) {
        ret |= arr[i] << i;
    }
    return ret;
}

int max(int a, int b) {
	return a > b ? a : b;
}
int min(int a, int b) {
	return a < b ? a : b;
}

void runRealMain() {
	int frm = 0;

	char lastBuf[3] = {0,0,0};

    while (aptMainLoop())
    {
        gspWaitForVBlank();
		hidScanInput();

        // u32 kDown = hidKeysDown();
        u32 kHeld = hidKeysHeld();
        circlePosition pos;

		touchPosition touch;

		//Read the touch screen coordinates
		hidTouchRead(&touch);
        hidCircleRead(&pos);
        // u32 kUp = hidKeysUp();
		if (kHeld & KEY_TOUCH) break;
		// printf("Touch: %03d; %03d\n", touch.px, touch.py);
		
		frm++;
        if(frm >= 3) {
			frm = 0;
            bool buf1[8] = {
                kHeld & KEY_A,
                kHeld & KEY_B,
                kHeld & KEY_X,
                kHeld & KEY_Y,
                kHeld & KEY_L,
                kHeld & KEY_R,
                kHeld & KEY_ZL,
                kHeld & KEY_ZR,
            };
            bool buf2[8] = {
                kHeld & KEY_SELECT,
                kHeld & KEY_START,
                kHeld & KEY_DUP,
                kHeld & KEY_DDOWN,
                kHeld & KEY_DLEFT,
                kHeld & KEY_DRIGHT,
                kHeld & KEY_CSTICK_UP,
                kHeld & KEY_CSTICK_DOWN,
            };

			// bool x[4] = {0,0,0,0};

            bool buf3[8] = {
                kHeld & KEY_CSTICK_LEFT,
                kHeld & KEY_CSTICK_RIGHT,
				pos.dx > 60,
				pos.dx < -60,
				pos.dy > 60,
				pos.dy < -60,
                0,
                0
            };

            char buf[] = {
                boolByteArrayToCharByte(buf1),
                boolByteArrayToCharByte(buf2),
                boolByteArrayToCharByte(buf3)
            };

            // for(int i = 0; i < 3; i++) {
			// 	printf("%02X ", buf[i]);
            // }
			// printf("\n");
			if(lastBuf[0] != buf[0] || lastBuf[1] != buf[1] || lastBuf[2] != buf[2]) {
				send(csock, buf, sizeof(buf), 0);
				lastBuf[0] = buf[0];
				lastBuf[1] = buf[1];
				lastBuf[2] = buf[2];
			}
			// printf("Sent\n");
        }
	}

    close (csock);
    csock = -1;
}

//---------------------------------------------------------------------------------
int main(int argc, char **argv) {
//---------------------------------------------------------------------------------
	gfxInitDefault();

	// register gfxExit to be run when app quits
	// this can help simplify error handling
	atexit(gfxExit);

	consoleInit(GFX_BOTTOM, NULL);

	printf ("\n3DS 2 Controller v1.0.0\n");

	// allocate buffer for SOC service
	SOC_buffer = (u32*)memalign(SOC_ALIGN, SOC_BUFFERSIZE);

	if(SOC_buffer == NULL) {
		failExit("memalign: failed to allocate\n");
	}

	// Now intialise soc:u service
	if ((ret = socInit(SOC_buffer, SOC_BUFFERSIZE)) != 0) {
    	failExit("socInit: 0x%08X\n", (unsigned int)ret);
	}

	// register socShutdown to run at exit
	// atexit functions execute in reverse order so this runs before gfxExit
	atexit(socShutdown);

	// libctru provides BSD sockets so most code from here is standard
	clientlen = sizeof(client);

	sock = socket (AF_INET, SOCK_STREAM, IPPROTO_IP);

	if (sock < 0) {
		failExit("socket: %d %s\n", errno, strerror(errno));
	}

	memset(&server, 0, sizeof (server));
	memset(&client, 0, sizeof (client));

	server.sin_family = AF_INET;
	server.sin_port = htons (80);
	server.sin_addr.s_addr = gethostid();

	printf("Point to %s\n",inet_ntoa(server.sin_addr));
		
	if ( (ret = bind (sock, (struct sockaddr *) &server, sizeof (server))) ) {
		close(sock);
		failExit("bind: %d %s\n", errno, strerror(errno));
	}

	// Set socket non blocking so we can still read input to exit
	fcntl(sock, F_SETFL, fcntl(sock, F_GETFL, 0) | O_NONBLOCK);

	if ( (ret = listen( sock, 5)) ) {
		failExit("listen: %d %s\n", errno, strerror(errno));
	}

	while (aptMainLoop()) {
		gspWaitForVBlank();
		hidScanInput();

		csock = accept (sock, (struct sockaddr *) &client, &clientlen);

		if (csock<0) {
			if(errno != EAGAIN) {
				failExit("accept: %d %s\n", errno, strerror(errno));
			}
		} else {
			// set client socket to blocking to simplify sending data back
			// fcntl(csock, F_SETFL, fcntl(csock, F_GETFL, 0) & ~O_NONBLOCK);
			printf("Connecting port %d from %s\n", client.sin_port, inet_ntoa(client.sin_addr));
			memset (temp, 0, 1026);

			ret = recv (csock, temp, 1024, 0);

			runRealMain();
		}

		u32 kDown = hidKeysDown();
		if (kDown & KEY_START) break;
	}

	close(sock);

	return 0;
}

//---------------------------------------------------------------------------------
void failExit(const char *fmt, ...) {
//---------------------------------------------------------------------------------

	if(sock>0) close(sock);
	if(csock>0) close(csock);

	va_list ap;

	printf(CONSOLE_RED);
	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);
	printf(CONSOLE_RESET);
	// printf("\nPress B to exit\n");

	while (aptMainLoop()) {
		gspWaitForVBlank();
		hidScanInput();

		u32 kDown = hidKeysDown();
		if (kDown & KEY_B) exit(0);
	}
}