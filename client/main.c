#include <stdio.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#define MAXREQUEST 80  // maximum size of request

void printError(void);  // function to display error messages

int main()
{
    WSADATA wsaData;  // create structure to hold winsock data
    int retVal, nRx, nIn;
    int stop = 0, endFound = 0;  // flags to control loops
    int nResp = 0;      // count of bytes received so far
    int filesize = 0;   //size of file to be downloaded/uploaded
    int noRequest= 0;
    char serverIP[20];      // IP address of server
    int serverPort;         // port used by server
    char request[MAXREQUEST+1];   // array to hold user input
    char response[MAXREQUEST+50];    // array to hold response from
    char fName[MAXREQUEST];     //name of file to be sent/received
    char exist;     //checks if file exists
    char command;   //upload or download
    char *pMark;        // pointer used in searching for marker
    FILE *fp; //file
    char data[50];      // array to hold file data

    // Initialise winsock, version 2.2, giving pointer to data structure
    retVal = WSAStartup(MAKEWORD(2,2), &wsaData);
    if (retVal != 0)  // check for error
    {
        printf("*** WSAStartup failed: %d\n", retVal);
        printError();
        return 1;
    }
    else printf("WSAStartup succeeded\n" );

    // Create a handle for a socket, to be used by the client
    SOCKET clientSocket = INVALID_SOCKET;  // handle called clientSocket

    // Create the socket, and assign it to the handle
    // AF_INET means IP version 4,
    // SOCK_STREAM means socket works with streams of bytes,
    // IPPROTO_TCP means TCP transport protocol.
    clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (clientSocket == INVALID_SOCKET)  // check for error
    {
        printf("*** Failed to create socket\n");
        printError();
        stop = 1;
    }
    else printf("Socket created\n" );

    // Get the details of the server from the user
    printf("Enter IP address of server: ");
    scanf("%20s", serverIP);  // get IP address as string

    printf("Enter port number: ");
    scanf("%d", &serverPort);     // get port number as integer

    gets(request);  // flush the endline from the input buffer

    // Build a structure to identify the service required
    // This has to contain the IP address and port of the server
    struct sockaddr_in service;  // IP address and port structure
    service.sin_family = AF_INET;  // specify IP version 4 family
    service.sin_addr.s_addr = inet_addr(serverIP);  // set IP address
    // function inet_addr() converts IP address string to 32-bit number
    service.sin_port = htons(serverPort);  // set port number
    // function htons() converts 16-bit integer to network format

    // Try to connect to the service required
    printf("Trying to connect to %s on port %d\n", serverIP, serverPort);
    retVal = connect(clientSocket, (SOCKADDR *) &service, sizeof(service));
    if( retVal != 0)  // check for error
    {
        printf("*** Error connecting\n");
        printError();
        stop = 1;  // make sure we do not go into the while loop
    }
    else printf("Connected!\n");

    // Main loop to send requests and receive responses
    // In this example the client will send first
    while (stop == 0)
    {
        // Get user request and send it to the server
        do
        {
            printf("\nEnter request (max %d bytes, u for upload, d for download, $ to end program)"
                   "\nFormat ($/u/d filename): ", MAXREQUEST);
            fgets(request, MAXREQUEST, stdin);  // read in the request string
            // the fgets() function reads characters until enter key is pressed,
            // or limit is reached.  If enter is pressed, the newline character
            // is included in the string.
            if((((request[0]=='d')||(request[0]=='u'))&&(request[1]==' '))||(request[0]=='$'))//if request is formatted correctly the loop is broken
            {
                break;
            }
            printf("\nImproper request format");
        }   while(1);

        sscanf(request, "%c %s", &command, fName);// parses request

        nIn = strlen(request);  //find the length of the request
        if (request[0] == '$') // check if the user wants to quit
        {
            stop = 1;  // set stop flag if $ entered
            printf("Closing connection as requested...\n");
        }
        if(request[0] == 'u')   //If user wants to upload file.
        {
            fp = fopen(fName, "rb");  // open for binary read
                    if(fp == NULL)  // check if file exists
                    {
                            printf("#404 File not found\n");
                            noRequest = 1;   //indicates not to send request to server
                    }
        }   //upload end

        if ((request[0] != '$')&&(noRequest==0))  // send the message and try to receive a reply if not leaving
        {
            // send() arguments: socket handle, array of bytes to send,
            // number of bytes to send, and last argument of 0.
            retVal = send(clientSocket, request, nIn, 0);  // send nIn bytes
            // retVal will be number of bytes sent, or error indicator

            if( retVal == SOCKET_ERROR) // check for error
            {
                printf("*** Error sending\n");
                printError();
            }
            else printf("Sent %d bytes, waiting for reply...\n", retVal);
            if(request[0] == 'u')   //ipload
            {  //upload
                retVal = fseek(fp, 0, SEEK_END);  // set current position to end of file
                int nBytes = ftell(fp);         // find out what current position is
                nResp = sprintf(response, "%d*",nBytes); //send header before tranmission '*[t-true (file exists) f-false (false)]*[fileSize]*'
                response[nResp] = 0;

                retVal = send(clientSocket, response, nResp, 0);  // send nResp bytes
                printf("Header: '%s' of size %d/%d\n",response, nResp, strlen(response));
                retVal = fseek(fp, 0, SEEK_SET);   // set current position to start of file


                printf("Opened file of size %d\n", nBytes);

                while(!feof(fp))    //sending data
                {
                    retVal = (int) fread(data, 1, 100, fp);  // read up to 40 bytes
                    send(clientSocket, data, retVal, 0);
                    printf("Sent bytes of size %d\n", retVal);
                }
                nResp = 0;  //reset response
                fclose(fp);
                printf("File closed.\n");
            }//upload end

            if(request[0] == 'd'){// download
                //recieve header
                nRx = recv(clientSocket, response, MAXREQUEST, 0);
                printf("Received %d bytes from server\n%s\n", nRx, response);
                if( nRx == SOCKET_ERROR)  // check for error
                {
                    printf("Problem receiving\n");
                    //printError();   // uncomment this to see more details of the error
                    stop = 1;  // set flag to exit the loop - no point in trying again
                }
                else if (nRx == 0)  // connection closed
                {
                    printf("Connection closed by server\n");
                    stop = 1;
                }
                else if (nRx > 0)  // we got a header
                {

                    pMark = memchr(response, 42, nRx);
                    *pMark = '\0';
                    printf("Received %d bytes from server\n%s\n", nRx, response);
                    sscanf(response, "%c%d*", &exist, &filesize);   //parse header
                }
                if (exist!='t'){
                    printf("#404 File not found\n");
                }
                else
                {
                    fp = fopen(fName, "wb");    //open/create file to be written on
                    do  // loop to receive downloaded file until recieved bytes = filesize
                    {
                        // Wait to receive bytes from the server, using the recv function
                        // recv() arguments: socket handle, array to hold received bytes,
                        // maximum number of bytes to receive, last argument 0.
                        // Normally, recv() will not return until it receives at least one byte...
                        nRx = recv(clientSocket, response, 100, 0);
                        // nRx will be number of bytes received, or error indicator

                        if( nRx == SOCKET_ERROR)  // check for error
                        {
                            printf("Problem receiving\n");
                            //printError();   // uncomment this to see more details of the error
                            stop = 1;  // set flag to exit the loop - no point in trying again
                        }
                        else if (nRx == 0)  // connection closed
                        {
                            printf("Connection closed by server\n");
                            stop = 1;
                        }
                        else if (nRx > 0)  // we got some data
                        {
                            // write whatever has been received
                            response[nRx] = 0; // convert response to a string
                            printf("Received %d bytes from server\n", nRx);
                            fwrite(response, 1, nRx, fp);
                            nResp += nRx;  // update the count of bytes received so far

                            if (nResp == filesize) // response has reached filesize
                            {
                                printf("End of file reached\n");
                                printf("Full response received, %d bytes\n", nResp);
                                nResp = 0;
                                endFound = 1;
                            }

                            // Otherwise, loop will continue to collect more of the response...

                        } // end of if data received
                    }   while((endFound == 0) && (stop == 0));
                    fclose(fp);
                    endFound = 0; //clean
                }

            }// end download

        } // end
        noRequest = 0;   //clean

    }  // end of outer loop - while stop == 0
    // When this loop exits, it is time to tidy up and end the program

    // Shut down the sending side of the TCP connection first
    retVal = shutdown(clientSocket, SD_SEND);
    if( retVal != 0)  // check for error
    {
        printf("*** Error shutting down sending\n");
        printError();
    }

    // Then close the socket
    retVal = closesocket(clientSocket);
    if( retVal != 0)  // check for error
    {
        printf("*** Error closing socket\n");
        printError();
    }
    else printf("Socket closed\n");

    // Finally clean up the winsock system
    retVal = WSACleanup();
    printf("WSACleanup returned %d\n",retVal);

    // Prompt for user input, so window stays open when run outside CodeBlocks
    printf("\nPress return to exit:");
    gets(request);
    return 0;
}

/* Function to print informative error messages
   when something goes wrong...  */
void printError(void)
{
	char lastError[1024];
	int errCode;

	errCode = WSAGetLastError();  // get the error code for the last error
	FormatMessage(
		FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL,
		errCode,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		lastError,
		1024,
		NULL);  // convert error code to error message
	printf("WSA Error Code %d = %s\n", errCode, lastError);
}
