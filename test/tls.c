#include "tls.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test_helper.h"

#include "../src/network/network.h"
#include "../src/network/tls.h"

#include "../src/log.h"

bool test_tls() {
	TEST_INIT();
	fprintf(stderr, "\n\ntls.o");

	TEST_FUNC_START(tls_init);
	TEST_RES(tls_init());
	TEST_FUNC_END();

	TEST_FUNC_START(tls_connect);
	TEST_RES(!tls_connect("", 42));             // invalid server, valid port
	TEST_RES(!tls_connect("narbo.de", 100000)); // valid server, invalid port
	TEST_RES(!tls_connect("narbo.de", 80));     // valid server & port, but no ssl
	TEST_RES(!tls_connect("narbo.de", 64738));  // valid server & port, ssl with self-signed cert

	{
		// valid server & port, ssl with valid cert (requires CAcert.org to be trusted)
		struct network_conn *nwc = tls_connect("narbo.de", 443);
		TEST_RES(nwc);
		nwc->disconnect(nwc);
	}
	TEST_FUNC_END();

	TEST_END();
}
