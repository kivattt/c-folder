# swayipc

This is a library which subscribes to the `workspace` event via Sway IPC and lets you handle those events.

I use this library for my Sway taskbar

`swayipc_receive_packet` is blocking, so this library is best used in a separate thread.

It allocates 128 kB of memory for the packet buffer.

## Usage

```c
#include <stdio.h>

#include "../swayipc.h"

int main() {
	struct SwayIPC ipc;
	swayipc_initialize(&ipc); // Allocate the 128 kB packet buffer
	int err = swayipc_connect(&ipc); // Connect, get the workspaces and subcribe to the "workspace" events
	if (err) {
		return 1;
	}

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
```

See: [demo/demo.c](demo/demo.c)
