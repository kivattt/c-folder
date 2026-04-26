#include "swayipc.h"

void swayipc_initialize(struct SwayIPC *ipc) {
	memset(ipc, 0, sizeof(struct SwayIPC));
	ipc->packetBufferSize = 128000; // 128 kB
	ipc->packetBuffer = malloc(ipc->packetBufferSize);
}

void swayipc_deinitialize(struct SwayIPC *ipc) {
	free(ipc->packetBuffer);
	ipc->packetBuffer = NULL;
}

// Returns non-zero on error
int swayipc_connect(struct SwayIPC *ipc) {
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

	// Create the subscribe request data
	const int headerSize = 14; // 6 + 4 + 4
	int32_t payloadLength = 13;

	char command[headerSize + payloadLength]; // Exactly enough for the subscribe to workspace command
	char *pointer = command;

	// i3-ipc
	int len = strlen("i3-ipc");
	memcpy(pointer, "i3-ipc", len);
	pointer += len;

	// Payload length, 32-bit signed integer
	memcpy(pointer, &payloadLength, 4);
	pointer += 4;

	// Payload type, 32-bit signed integer
	int32_t payloadType = 2;
	memcpy(pointer, &payloadType, 4);
	pointer += 4;

	// ["workspace"]
	len = strlen("[\"workspace\"]");
	memcpy(pointer, "[\"workspace\"]", len);
	pointer += len;

	// Send the subscribe request
	size_t length = pointer - command;
	if (write(ipc->socketFileDescriptor, command, length) != length) {
		return 4;
	}

	memset(ipc->packetBuffer, 0, ipc->packetBufferSize);
	int count = read(ipc->socketFileDescriptor, ipc->packetBuffer, ipc->packetBufferSize);
	assert(count <= ipc->packetBufferSize);
	assert(count != -1); // error
	assert(count != 0); // end of file

	return 0;
}

void swayipc_receive_packet(struct SwayIPC *ipc, char **packet, int *packetSize) {
	const int headerSize = 14; // 6 + 4 + 4

	// If we haven't reached the end of the previous packet, return with next message contained within.
	if (ipc->messagePtr != NULL && ipc->messagePtr < ipc->packetBuffer + ipc->packetSize) {
		int32_t payloadLength = *((int32_t*)(ipc->messagePtr+6));
		int32_t payloadType = *((int32_t*)(ipc->messagePtr+10));
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
	int32_t payloadType = *((int32_t*)(ipc->packetBuffer+10));
	ipc->messagePtr = ipc->packetBuffer;
	*packet = ipc->messagePtr;
	*packetSize = headerSize + payloadLength;

	ipc->messagePtr += headerSize + payloadLength; // Advance to the next message
	return;

	// Read message(s) in the packet
	/*char *message = buf;
	for (int maxIter = 0; maxIter < 10; maxIter++) {
		assert(strncmp(message, "i3-ipc", 6) == 0);
		int32_t payloadLength = *((int32_t*)(message+6));
		int32_t payloadType = *((int32_t*)(message+10));
		//printf("    type: 0x%x, len: %i\n", payloadType, payloadLength);

		// Handle the payload body
		handle_i3_ipc_payload(message+headerSize, payloadLength);

		// Advance to next message in the packet
		message += headerSize + payloadLength;
		if (message >= ipc->packetBuffer + ipc->packetSize) {
			break; // Reached the end of the packet
		}
	}*/
}
