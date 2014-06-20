/*
 * auth.c - deal with authentication.
 *
 * This file implements the VNC authentication protocol when setting up an RFB
 * connection.
 */

/*
 *  Copyright (C) 1997, 1998 Olivetti & Oracle Research Laboratory
 *
 *  This is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This software is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this software; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307,
 *  USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include "windowstr.h"
#include "rfb.h"


char *rfbAuthPasswdFile = NULL;


/*
 * rfbAuthNewClient is called when we reach the point of authenticating
 * a new client.  If authentication isn't being used then we simply send
 * rfbNoAuth.  Otherwise we send rfbVncAuth plus the challenge.
 */

void
rfbAuthNewClient(cl)
    rfbClientPtr cl;
{
    char buf[4 + CHALLENGESIZE];
    int len;

    cl->state = RFB_AUTHENTICATION;

    if (rfbAuthPasswdFile && !cl->reverseConnection) {

	*(CARD32 *)buf = Swap32IfLE(rfbVncAuth);
	vncRandomBytes(cl->authChallenge);
	memcpy(&buf[4], (char *)cl->authChallenge, CHALLENGESIZE);
	len = 4 + CHALLENGESIZE;

    } else {

	*(CARD32 *)buf = Swap32IfLE(rfbNoAuth);
	len = 4;
	cl->state = RFB_INITIALISATION;
    }

    if (WriteExact(cl->sock, buf, len) < 0) {
	rfbLogPerror("rfbAuthNewClient: write");
	rfbCloseSock(cl->sock);
	return;
    }
}


/*
 * rfbAuthProcessClientMessage is called when the client sends its
 * authentication response.
 */

void
rfbAuthProcessClientMessage(cl)
    rfbClientPtr cl;
{
    char *passwd;
    Bool ok;
    int i, n;
    CARD8 response[CHALLENGESIZE];
    CARD32 authResult;

    if ((n = ReadExact(cl->sock, (char *)response, CHALLENGESIZE)) <= 0) {
	if (n != 0)
	    rfbLogPerror("rfbAuthProcessClientMessage: read");
	rfbCloseSock(cl->sock);
	return;
    }

    passwd = vncDecryptPasswdFromFile(rfbAuthPasswdFile);

    if (passwd == NULL) {
	rfbLog("rfbAuthProcessClientMessage: could not get password from %s\n",
	       rfbAuthPasswdFile);

	authResult = Swap32IfLE(rfbVncAuthFailed);

	if (WriteExact(cl->sock, (char *)&authResult, 4) < 0) {
	    rfbLogPerror("rfbAuthProcessClientMessage: write");
	}
	rfbCloseSock(cl->sock);
	return;
    }

    vncEncryptBytes(cl->authChallenge, passwd);

    /* Lose the password from memory */
    for (i=0; i<strlen(passwd); i++) {
	passwd[i] = '\0';
    }

    free((char *)passwd);

    if (memcmp(cl->authChallenge, response, CHALLENGESIZE) != 0) {
	rfbLog("rfbAuthProcessClientMessage: authentication failed from %s\n",
	       cl->host);

	authResult = Swap32IfLE(rfbVncAuthFailed);

	if (WriteExact(cl->sock, (char *)&authResult, 4) < 0) {
	    rfbLogPerror("rfbAuthProcessClientMessage: write");
	}
	rfbCloseSock(cl->sock);
	return;
    }

    authResult = Swap32IfLE(rfbVncAuthOK);

    if (WriteExact(cl->sock, (char *)&authResult, 4) < 0) {
	rfbLogPerror("rfbAuthProcessClientMessage: write");
	rfbCloseSock(cl->sock);
	return;
    }

    cl->state = RFB_INITIALISATION;
}
