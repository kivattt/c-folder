#pragma once

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>

struct SwayIPC {
	int socketFileDescriptor;

	char *packetBuffer; // Gets allocated in swayipc_initialize
	size_t packetBufferSize;

	char *messagePtr;
	int packetSize;

	char *initialWorkspacesPacket; // Gets allocated in swayipc_initialize
	int initialWorkspacesPacketSize;
};

void swayipc_initialize(struct SwayIPC *ipc);
void swayipc_deinitialize(struct SwayIPC *ipc);
int swayipc_connect(struct SwayIPC *ipc); // Returns non-zero on error
void swayipc_receive_packet(struct SwayIPC *ipc, char **packet, int *packetSize);
