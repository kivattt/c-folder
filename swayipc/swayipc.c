#include "swayipc.h"

void swayipc_initialize(struct SwayIPC *ipc) {
	memset(ipc, 0, sizeof(struct SwayIPC));
	ipc->packetBufferSize = 128000; // 128 kB
	ipc->packetBuffer = malloc(ipc->packetBufferSize);

	ipc->initialWorkspacesPacket = malloc(ipc->packetBufferSize);
}

void swayipc_deinitialize(struct SwayIPC *ipc) {
	free(ipc->packetBuffer);
	ipc->packetBuffer = NULL;

	free(ipc->initialWorkspacesPacket);
	ipc->initialWorkspacesPacket = NULL;

	close(ipc->socketFileDescriptor);
	ipc->socketFileDescriptor = -1;
}

// Returns non-zero on error
int swayipc_connect(struct SwayIPC *ipc) {
	if (ipc->packetBuffer == NULL || ipc->initialWorkspacesPacket == NULL) {
		printf("swayipc_connect(): Buffers were NULL. Did you forget to call swayipc_initialize()?\n");
		assert(0);
	}

	char *socketPath = getenv("SWAYSOCK");
	if (socketPath == NULL) {
		printf("SWAYSOCK env variable was not set\n");
		return 1;
	}

	ipc->socketFileDescriptor = socket(AF_UNIX, SOCK_STREAM, 0);
	if (ipc->socketFileDescriptor == -1) {
		printf("Unable to create socket\n");
		return 2;
	}

	// Connect to the Sway IPC
	struct sockaddr_un addr;
	memset(&addr, 0, sizeof(struct sockaddr_un));
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, socketPath, sizeof(addr.sun_path) - 1);
	if (connect(ipc->socketFileDescriptor, (struct sockaddr*)&addr, sizeof(struct sockaddr_un)) == -1) {
		printf("Unable to connect to Sway IPC\n");
		return 3;
	}

	// Send GET_WORKSPACES message
	//                     0 (payload len)    1 (payload type)
	char *command = "i3-ipc\x00\x00\x00\x00\x01\x00\x00\x00";
	size_t commandLength = 14; // 6 + 4 + 4
	if (write(ipc->socketFileDescriptor, command, commandLength) != commandLength) {
		return 4;
	}

	// Receive GET_WORKSPACES response, and put it in ipc->initialWorkspacesPacket
	memset(ipc->packetBuffer, 0, ipc->packetBufferSize);
	int count = read(ipc->socketFileDescriptor, ipc->initialWorkspacesPacket, ipc->packetBufferSize);
	ipc->initialWorkspacesPacketSize = count;
	assert(count <= ipc->packetBufferSize);
	assert(count != -1); // error
	assert(count != 0); // end of file

	// Send subscribe to "workspace" message
	//                 13 (payload len) 2 (payload type SUBSCRIBE)
	command = "i3-ipc\x0d\x00\x00\x00\x02\x00\x00\x00[\"workspace\"]";
	commandLength = 27; // 6 + 4 + 4 + 13
	if (write(ipc->socketFileDescriptor, command, commandLength) != commandLength) {
		return 5;
	}

	// Receive subscribe response (success or not). Ignored for now
	memset(ipc->packetBuffer, 0, ipc->packetBufferSize);
	count = read(ipc->socketFileDescriptor, ipc->packetBuffer, ipc->packetBufferSize);
	assert(count <= ipc->packetBufferSize);
	assert(count != -1); // error
	assert(count != 0); // end of file

	return 0;
}

// workspace_num has to be 1..10 inclusive. Returns non-zero on error.
int swayipc_switch_workspace(struct SwayIPC *ipc, int workspace_num) {
	assert(workspace_num >= 1);
	assert(workspace_num <= 10);

	int headerSize = 14;

	int payloadLen = snprintf(NULL, 0, "workspace %d", workspace_num);
	char *command = malloc(headerSize + payloadLen + 1);
	memcpy(command, "i3-ipc", 6);
	memcpy(command + 6, &payloadLen, 4); // Payload length
	memset(command + 10, 0, 4); // Paylot type of 0 is RUN_COMMAND

	snprintf(command + headerSize, payloadLen + 1, "workspace %d", workspace_num);

	// Send the "workspace ..." command
	if (write(ipc->socketFileDescriptor, command, headerSize+payloadLen) != headerSize + payloadLen) {
		return 1;
	}

	free(command);

	// We should really receive ( read() ) and ignore the answer here, but it blocks like crazy for some reason.

	return 0;
}

void swayipc_receive_packet(struct SwayIPC *ipc, char **packet, int *packetSize) {
	if (ipc->packetBuffer == NULL || ipc->initialWorkspacesPacket == NULL) {
		printf("swayipc_receive_packet(): Buffers were NULL. Did you forget to call swayipc_initialize()?\n");
		assert(0);
	}

	const int headerSize = 14; // 6 + 4 + 4

	// If we haven't reached the end of the previous packet, return with next message contained within.
	if (ipc->messagePtr != NULL && ipc->messagePtr < ipc->packetBuffer + ipc->packetSize) {
		int32_t payloadLength = *((int32_t*)(ipc->messagePtr+6));
		*packet = ipc->messagePtr;
		*packetSize = headerSize + payloadLength;
		ipc->messagePtr += headerSize + payloadLength; // Advance to the next message
		return;
	}

	// Need to read from the Sway IPC socket
	memset(ipc->packetBuffer, 0, ipc->packetBufferSize);
	ipc->packetSize = read(ipc->socketFileDescriptor, ipc->packetBuffer, ipc->packetBufferSize);
	assert(ipc->packetSize <= ipc->packetBufferSize);
	assert(ipc->packetSize != -1); // On error
	assert(ipc->packetSize != 0); // On end of file

	int32_t payloadLength = *((int32_t*)(ipc->packetBuffer+6));
	ipc->messagePtr = ipc->packetBuffer;
	*packet = ipc->messagePtr;
	*packetSize = headerSize + payloadLength;

	ipc->messagePtr += headerSize + payloadLength; // Advance to the next message
	return;
}
