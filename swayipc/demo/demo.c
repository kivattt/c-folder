#include <stdio.h>

#include "../swayipc.h"

int main() {
	struct SwayIPC ipc;
	swayipc_initialize(&ipc); // Allocate the 128 kB packet buffer
	int err = swayipc_connect(&ipc); // Connect, get the workspaces and subcribe to the "workspace" events
	if (err) {
		return 1;
	}

	// Switch to workspace 1
	swayipc_switch_workspace(&ipc, 1);

	// Print the initial GET_WORKSPACES response
	write(1, ipc.initialWorkspacesPacket, ipc.initialWorkspacesPacketSize);
	printf("\n");

	while (1) {
		char *packet;
		int packetSize;
		swayipc_receive_packet(&ipc, &packet, &packetSize); // Give me the next packet

		// Print the event
		write(1, packet, packetSize);
		printf("\n");
	}

	swayipc_deinitialize(&ipc);
}
