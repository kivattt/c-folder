#include <stdio.h>

#include "../swayipc.h"

int main() {
	struct SwayIPC ipc;
	swayipc_initialize(&ipc); // Allocate the 128 kB packet buffer
	int err = swayipc_connect(&ipc); // Connect and subcribe to the "workspace" events
	if (err) {
		return 1;
	}

	while (1) {
		char *packet;
		int packetSize;
		swayipc_receive_packet(&ipc, &packet, &packetSize); // Give me the next packet
		printf("Packet size: %i, data preview: ", packetSize);

		// Show up to 60 bytes of the packet
		int atMost60Bytes = packetSize < 60 ? packetSize : 60;
		for (int i = 0; i < atMost60Bytes; i++) {
			printf("%c", packet[i]);
		}

		printf("\n");
	}

	swayipc_deinitialize(&ipc);
}
