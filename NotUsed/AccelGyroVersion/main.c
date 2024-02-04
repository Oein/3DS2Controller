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

	angularRate lastGyro;
	accelVector lastAccel;

	lastAccel.x = 0;
	lastAccel.y = 0;
	lastAccel.z = 0;
	lastGyro.x = 0;
	lastGyro.y = 0;
	lastGyro.z = 0;

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

			angularRate gyro;
			hidGyroRead(&gyro);
			accelVector accel;
			hidAccelRead(&accel);

			// printf("ACC: %d %d %d | GYRO: %d %d %d\n", accel.x, accel.y, accel.z, gyro.x, gyro.y, gyro.z);

			char cont1 = boolByteArrayToCharByte(buf1);
			char cont2 = boolByteArrayToCharByte(buf2);
			char cont3 = boolByteArrayToCharByte(buf3);

			char sendBuf[1024] = {0,};
			int lastBufIdx = 0;

			if(cont1 != lastBuf[0] || cont2 != lastBuf[1] || cont3 != lastBuf[2]) {
				// char newBuf[4] = {0x00, cont1, cont2, cont3};

				sendBuf[0] = 0x00;
				sendBuf[1] = cont1;
				sendBuf[2] = cont2;
				sendBuf[3] = cont3;

				lastBufIdx += 4;

				lastBuf[0] = cont1;
				lastBuf[1] = cont2;
				lastBuf[2] = cont3;

				printf("C ");
			}

			if(lastGyro.x != gyro.x || lastGyro.y != gyro.y || lastGyro.z != gyro.z) {
				char cmd = 0x00;
				if(lastGyro.x != gyro.x) cmd |= 0x01;
				if(lastGyro.y != gyro.y) cmd |= 0x02;
				if(lastGyro.z != gyro.z) cmd |= 0x04;
				if(cmd != 0x00) {
					char newBuf[7] = {cmd,};
					int sidx = 1;
					if(lastGyro.x != gyro.x) {
						newBuf[sidx] = gyro.x >> 8;
						newBuf[sidx+1] = gyro.x & 0xFF;
						sidx += 2;
					}
					if(lastGyro.y != gyro.y) {
						newBuf[sidx] = gyro.y >> 8;
						newBuf[sidx+1] = gyro.y & 0xFF;
						sidx += 2;
					}
					if(lastGyro.z != gyro.z) {
						newBuf[sidx] = gyro.z >> 8;
						newBuf[sidx+1] = gyro.z & 0xFF;
						sidx += 2;
					}
					strncpy(sendBuf + lastBufIdx, newBuf, sidx);
					lastBufIdx += sidx;

					lastGyro.x = gyro.x;
					lastGyro.y = gyro.y;
					lastGyro.z = gyro.z;

					printf("G%d ", (int)cmd);
				}
			}

			if(lastAccel.x != accel.x || lastAccel.y != accel.y || lastAccel.z != accel.z) {
				char cmd = 1 << 3;
				if(lastAccel.x != accel.x) cmd |= 0x01;
				if(lastAccel.y != accel.y) cmd |= 0x02;
				if(lastAccel.z != accel.z) cmd |= 0x04;

				if(cmd != 1 << 3) {
					char newBuf[7] = {cmd,0,0,0};
					int sidx = 1;
					if(lastAccel.x != accel.x) {
						newBuf[sidx] = accel.x >> 8;
						newBuf[sidx+1] = accel.x & 0xFF;
						sidx += 2;
					}
					if(lastAccel.y != accel.y) {
						newBuf[sidx] = accel.y >> 8;
						newBuf[sidx+1] = accel.y & 0xFF;
						sidx += 2;
					}
					if(lastAccel.z != accel.z) {
						newBuf[sidx] = accel.z >> 8;
						newBuf[sidx+1] = accel.z & 0xFF;
						sidx += 2;
					}

					strncpy(sendBuf + lastBufIdx, newBuf, sidx);
					lastBufIdx += strlen(sidx);

					lastAccel.x = accel.x;
					lastAccel.y = accel.y;
					lastAccel.z = accel.z;

					printf("A%d ", (int)cmd - (1 << 3));
				}
			}
        
			// print sendBuf

			if(lastBufIdx > 0) {
				send(csock, sendBuf, lastBufIdx, 0);

				// printf(" S%d | ", lastBufIdx);
				// for(int i = 0; i < lastBufIdx; i++) {
				// 	printf("%02X ", sendBuf[i]);
				// }
				printf("\n");
			}
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