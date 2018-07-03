
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdint.h>
#include <errno.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <netinet/in.h>
#include <fcntl.h>

#define PORT 5494 
#define TIMEOUT 300
#define KEEPALIVE_INT 15

struct reservation {
	int group;
	uint32_t ip;
	int slot;
	int channel;
	uint64_t lastSeen;
	struct reservation *next;
};

struct group {
	struct group *next;
	int index;
	uint16_t lofl;
	uint16_t lofh;
	uint16_t  thresh;
	int format;
	int positions;
	int firstChannel;
	int channelCnt;
	uint16_t frequencies[1];
};

struct reservation *reservations = 0;
struct group *groups = 0;


static uint64_t getTime() {
	struct timespec tp;
	clock_gettime (CLOCK_MONOTONIC, &tp);
	return (uint64_t)tp.tv_sec * 1000 + (uint64_t)tp.tv_nsec / 1000000;
}

static int findChannel (struct group *g, uint32_t ip, int slot, struct reservation **res) {
	const int groupIndex = g->index;
	const int firstChannel = g->firstChannel;
	const int channelCnt = g->channelCnt;
	
	struct reservation *r = reservations;
	struct reservation *oldest = 0;
	uint64_t oldestTime = -1;
	
	char channelUsed[channelCnt];
	int i;
	
	memset (channelUsed, 0, channelCnt);
	
	for (; r; r = r->next) {
		if (r->group == groupIndex && r->channel < firstChannel + channelCnt) {
			if (r->ip == ip && r->slot == slot) {
				*res = r;
				return r->channel;
			}
			channelUsed[r->channel - firstChannel] = 1;
			if (r->lastSeen < oldestTime) {
				oldest = r;
				oldestTime = r->lastSeen;
			}
		}
	}
	
	for (i = 0; i < channelCnt; i++) {
		if (!channelUsed[i]) {
			*res = 0;
			return i + firstChannel;
		}
	}
	
	// no free channel, use oldest reserved channel if we
	// haven't heard from this device for more than TIMEOUT seconds
	if (oldest && oldestTime < getTime() - TIMEOUT * 1000) {
		*res = oldest;
		return oldest->channel;
	}
	
	*res = 0;
	return -1;
}

static void request (int groupIndex, uint32_t ip, int port, int slot, int lnb, int sock) {
	struct group *g = groups;
	
	for (; g && g->index != groupIndex; g = g->next) ;
	
	if (g) {
		struct reservation *r = 0;
		int channel = findChannel (g, ip, slot, &r);
		if (channel == -1)
			return;
		if (r == 0) {
			r = (struct reservation *) malloc (sizeof (struct reservation));
			if (r == 0)
				return;
			r->next = reservations;
			reservations = r;
		}
		r->group = groupIndex;
		r->ip = ip;
		r->slot = slot;
		r->channel = channel;
		r->lastSeen = getTime();
		
		uint16_t frequency = g->frequencies[channel - g->firstChannel];
		uint8_t responseString[19];
		
		responseString[ 0] = 0xa7;
		responseString[ 1] = 0xd3;
		responseString[ 2] = 2;
		responseString[ 3] = groupIndex;
		responseString[ 4] = slot;
		responseString[ 5] = lnb;
		responseString[ 6] = channel;
		responseString[ 7] = g->format;
		responseString[ 8] = frequency >> 8;
		responseString[ 9] = frequency;
		responseString[10] = g->lofl >> 8;
		responseString[11] = g->lofl;
		responseString[12] = g->lofh >> 8;
		responseString[13] = g->lofh;
		responseString[14] = g->thresh >> 8;
		responseString[15] = g->thresh;
		responseString[16] = g->positions;
		responseString[17] = KEEPALIVE_INT >> 8;
		responseString[18] = KEEPALIVE_INT;
		
		struct sockaddr_in addr;
		memset (&addr, 0, sizeof (struct sockaddr_in));
		addr.sin_family = AF_INET;
		addr.sin_port = port;
		addr.sin_addr.s_addr = ip;
		sendto (sock, responseString, sizeof (responseString), 0, (struct sockaddr *) &addr, sizeof (struct sockaddr_in));
	}
}

static void release (int groupIndex, uint32_t ip, int slot) {
	struct reservation *r, **ptr = &reservations;
	
	for (; r = *ptr; ptr = &r->next) {
		if (r->group == groupIndex && r->ip == ip && r->slot == slot) {
			*ptr = r->next;
			free (r);
		}
	}
}

static void keepalive (int groupIndex, uint32_t ip, int slot, int channel) {
	struct reservation *r;
	struct group *g;
	
	for (g = groups; g && g->index != groupIndex; g = g->next) ;
	
	if (!g || channel < g->firstChannel || channel >= g->firstChannel + g->channelCnt)
		return;
	
	for (r = reservations; r; r = r->next) {
		if (r->group == groupIndex && r->channel == channel) {
			if (r->ip == ip && r->slot == slot) {
				r->lastSeen = getTime();
			}
			else if (r->lastSeen < getTime() - TIMEOUT * 1000) {
				release (groupIndex, ip, slot);
				r->ip = ip;
				r->slot = slot;
				r->lastSeen = getTime();
			}
			return;
		}
	}
	
	// no reservation with matching group and channel found
	r = (struct reservation *) malloc (sizeof (struct reservation));
	if (!r)
		return;
	r->group = groupIndex;
	r->ip = ip;
	r->slot = slot;
	r->channel = channel;
	r->lastSeen = getTime();
	r->next = reservations;
	reservations = r;
}

static void clearGroups() {
	// don't clear reservations here to avoid
	// conflicts when updating tuner config
	while (groups) {
		struct group *g = groups->next;
		free (groups);
		groups = g;
	}
}

static int read_uint16 (char **str) {
	char *s = *str;
	int i = 0;
	
	// skip nonnumeric characters
	for (; *s && *s < 0x30 || *s > 0x39; s++) ;
	
	if (!*s) {
		*str = s;
		return -1;
	}
	
	for (; *s && *s >= 0x30 && *s <= 0x39; s++) {
		i = i * 10 + *s - 0x30;
		if (i >= 0x10000) {
			*str = s;
			return -1;
		}
	}
	*str = s;
	return i;
}

static void readGroupLine (char *line) {
	int i;
	int index = read_uint16 (&line);
	int format = read_uint16 (&line);
	int lofl = read_uint16 (&line);
	int lofh = read_uint16 (&line);
	int thresh = read_uint16 (&line);
	int positions = read_uint16 (&line);
	int firstChannel = read_uint16 (&line);
	int channelCnt = read_uint16 (&line);

	int ok =
		index > 0 &&
		(format == 0 || format == 1) &&
		lofl > 0 && lofh > 0 &&
		thresh > 0 &&
		positions >= 0 && positions < 256 &&
		firstChannel >= 0 &&
		channelCnt > 0 ;
	
	if (!ok)
		return;

	struct group *g = (struct group *) malloc (sizeof (struct group) + sizeof(g->frequencies[0]) * (channelCnt-1) );
	
	if (!g)
		return;
	
	g->next = 0;
	g->index = index;
	g->format = format;
	g->lofl = lofl;
	g->lofh = lofh;
	g->thresh = thresh;
	g->positions = positions;
	g->firstChannel = firstChannel;
	g->channelCnt = channelCnt;
	
	for (i = 0; i < channelCnt; i++) {
		int frequency = read_uint16 (&line);
		if (frequency < 0) {
			free (g);
			return;
		}
		g->frequencies[i] = frequency;
	}
	
	struct group *gold, **ptr = &groups;
	
	for (; gold = *ptr; ptr = &gold->next) {
		if (gold->index == index) {
			*ptr = gold->next;
			free (gold);
		}
	}
	
	*ptr = g;
}

int set_nonblock (int fd) {
	int flags = fcntl(fd, F_GETFL,0);
    return (flags != -1 && fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0) ? 1 : 0;
}

int main() {
	const uint64_t startupTime = getTime();
	int allowRequests = 0;
	struct sockaddr_in addr;
	int sock;
	int daemon = 0;
	fd_set read_fds;
	
	if ( (sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0 )
		return 1;
	
	memset (&addr, 0, sizeof (struct sockaddr_in));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(PORT);
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	
	if (bind(sock, (struct sockaddr *) &addr, sizeof (addr)) < 0)
		return 1;
	
	if (!set_nonblock (0) || !set_nonblock (sock))
		return 1;

	for (;;) {
		char line[2048];
		int line_len = 0;
		
		FD_ZERO (&read_fds);
		if (!daemon)	
			FD_SET (0, &read_fds);
		FD_SET (sock, &read_fds);
		
		if (select (sock + 1, &read_fds, 0, 0, 0) < 0) {
			close (sock);
			return 1;
		}
	
		if (FD_ISSET(sock, &read_fds)) {
			// receive udp packet
			uint8_t pkt[20];
			int addrlen = sizeof (struct sockaddr_in);
			int pktlen = recvfrom (sock, pkt, sizeof(pkt), 0, (struct sockaddr *) &addr, &addrlen);
			if (pktlen >= 7 && pkt[0] == 0xa7 && pkt[1] == 0xd3) {
				uint32_t ip = addr.sin_addr.s_addr;
				int cmd = pkt[2];
				int groupIndex = pkt[3];
				int slot = pkt[4];
				int lnb = pkt[5];
				int channel = pkt[6];
				if (cmd == 1) {
					// after reboot, first observe keepalives from running
					// devices before allowing new requests
					if (!allowRequests && getTime() > startupTime + KEEPALIVE_INT*1000+100)
						allowRequests = 1;
					if (allowRequests)
						request (groupIndex, ip, addr.sin_port, slot, lnb, sock);
				}
				else if (cmd == 3) {
					keepalive (groupIndex, ip, slot, channel);
				}
				else if (cmd == 4) {
					release (groupIndex, ip, slot);
				}
			}
		}
		if (FD_ISSET (0, &read_fds)) {
			// communicate with enigma using stdin
			for (;;) {
				int ret = read(0, &line[line_len], 1);
				if ((ret == 1 && line[line_len] == '\n') || (ret == 0 && line_len > 0)) {
					if (line[0] == 'c') {
						clearGroups();
					}
					else if (line[0] == 'a') {
						readGroupLine(&line[1]);
					}
					else if (line[0] == 'd') {
						// daemonize.
						// not used by enigma, but nice-to-have
						// when run from console
						if (fork())
							return 0;
						close (0);
						close (1);
						close (2);
						setsid();
						chdir("/");
						daemon = 1;
						break;
					}
					line_len = 0;
				}
				else if (ret == 1 && line_len < sizeof(line) - 1) {
					line_len++;
				}
				
				if (ret != 1) {
					if (ret == 0) {
						// enigma closed pipe
						close (sock);
						return 0;
					}
					else if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) {
						break;
					}
					else { // error
						close (sock);
						return 1;
					}
				}
			};
		}
	}
}
