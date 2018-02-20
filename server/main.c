#include <stdio.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <conio.h>
#include <unistd.h>
void printError(void);  // function to display error messages

#define SERV_PORT 32980  // port to be used by server
#define MAXREQUEST 80   // maximum size of request, in bytes
#define ENDMARK 10      // byte value of request end marker (LF)
#define BLOCKSIZE 255    // block size for both uploading and downloading
#define ENDMARK_HEADER 42 //asterisks marker in ASCII table

int main()
{
    WSADATA wsaData;    // create structure to hold winsock data
    SOCKET cSocket;     // create structure for connection socket
    int retVal;         // return value from many different functions
    int nRx;            // number of bytes received
    int header = 0;     // flag to indicate when header was received
    int nReq = 0;		// number of bytes in request so far
    int nResp = 0;      // number of bytes in response string
    int stop = 0;       // flag to control loop
    int i = 0;
    int initialise = 1; //initialise the connection in the beginning
    int endFound=0;     // flag to indicate when file was received
    char *pMark;        // pointer used in searching for marker
    char digit[1];      // single char to store a single byte from header
    char request[MAXREQUEST+1];   // array to hold received bytes
    char response[BLOCKSIZE]; // array to hold our response
    char req[1]; //allocate memory for a single byte, agreed with client that it will never exceed 1 byte
    char *temp; //temporary pointer to store parts of the request
    struct in_addr clientIP;

    char data[BLOCKSIZE];      // array to hold file data
    FILE *fp;          // file handle for file
    long nBytes;       // file size

    // Initialise winsock, version 2.2, giving pointer to data structure
    retVal = WSAStartup(MAKEWORD(2,2), &wsaData);
    if (retVal != 0)  // check for error
    {
        printf("*** WSAStartup failed: %d\n", retVal);
        printError();
        return 1;
    }
    printf("WSAStartup succeeded\n" );

    // Create a handle for a socket, to be used by the server for listening
    SOCKET serverSocket = INVALID_SOCKET;  // handle called serverSocket

    // Create the socket, and assign it to the handle
    // AF_INET means IP version 4,
    // SOCK_STREAM means socket works with streams of bytes,
    // IPPROTO_TCP means TCP transport protocol.
    serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (serverSocket == INVALID_SOCKET)  // check for error
    {
        printf("*** Failed to create socket\n");
        printError();
    }
    else printf("Socket created\n" );

    // Build a structure to identify the service offered
    struct sockaddr_in service;  // IP address and port structure
    service.sin_family = AF_INET;  // specify IP version 4 family
    service.sin_addr.s_addr = htonl(INADDR_ANY);  // set IP address
    // function htonl() converts 32-bit integer to network format
    // INADDR_ANY means we accept connection on any IP address
    service.sin_port = htons(SERV_PORT);  // set port number on which to listen
    // function htons() converts 16-bit integer to network format

    // Bind the socket to the IP address and port just defined
    retVal = bind(serverSocket, (SOCKADDR *) &service, sizeof(service));
    if( retVal == SOCKET_ERROR)  // check for error
    {
        printf("*** Error binding to socket\n");
        printError();
    }
    else printf("Socket bound\n");

    // Setting up log file
    if(access( "log.txt", F_OK ) == -1 ) { //if file doesnt exist create one
        FILE *logfp = fopen("log.txt", "w");
        fprintf(logfp, "Client IP\tRequest\tFilename\tFile Size\n"); //Writing header to the file.
        fclose(logfp);
    }



    // Main loop to receive requests from the client and send responses
    // We cannot assume that an entire request will arrive in one call to the recv function,
    // so we keep trying to receive bytes, adding them to the array, until the request is complete.
    // This simple example uses an end marker byte to mark the end of a request.

    do
    {

        if(initialise == 1){ // if inititialise is 1, we need to initialise
            // Listen for connection requests on this socket,
            retVal = listen(serverSocket, 2);
            if( retVal == SOCKET_ERROR)  // check for error
            {
                printf("*** Error trying to listen\n");
                printError();
            }
            else printf("Listening on port %d\n", SERV_PORT);

            // Create a new socket for the connection we expect
            // The serverSocket stays listening for more connection requests,
            // so we need another socket to connect with the client...
            cSocket = INVALID_SOCKET;

            // Create a structure to identify the client (optional)
            struct sockaddr_in client;  // IP address and port structure
            int len = sizeof(client);  // initial length of structure

            // Wait until a connection is requested, then accept the connection.
            // If we do not need to know who is connecting, arguments 2 and 3 can be NULL
            cSocket = accept(serverSocket, (SOCKADDR *) &client, &len );
            if( cSocket == INVALID_SOCKET)  // check for error
            {
                printf("*** Failed to accept connection\n");
                printError();
            }
            else  // we have a connection, report who it is (if we care)
            {
                int clientPort = client.sin_port;  // get port number
                clientIP = client.sin_addr;  // get IP address
                // in_addr is a structure to hold an IP address
                printf("\nAccepted connection from %s using port %d\n",
                       inet_ntoa(clientIP), ntohs(clientPort));
                // function inet_ntoa() converts IP address structure to string
                // function ntohs() converts 16-bit integer from network form to normal
            }

            initialise = 0;
    }




        // First wait to receive bytes from the client, using the recv function
        // recv() arguments: socket handle, array to hold received bytes,
        // maximum number of bytes to receive, last argument 0.
        // Normally, this function will not return until it receives at least one byte...
        nRx = recv(cSocket, request+nReq, MAXREQUEST-nReq, 0);
        // nRx will be number of bytes received, or an error indicator (negative value)

        if( nRx < 0)  // check for error
        {
            printf("Problem receiving, maybe connection closed by client?\n");
            //printError();   // uncomment this if you want more details of the error

            retVal = shutdown(cSocket, SD_SEND); //close the connection with the client
            retVal = closesocket(cSocket); //close the socket with the current client

            initialise = 1; // Intiailise again to accept new connections.
        }
        else if (nRx == 0)  // indicates connection closing (but not an error)
        {
            printf("Connection closing\n");

            retVal = shutdown(cSocket, SD_SEND); //close the connection with the client
            retVal = closesocket(cSocket); //close the socket with the current client

            initialise = 1; // Intiailise again to accept new connections.
        }
        else // we got some data from the client
        {
            printf("Received %d bytes from client\n", nRx); // print message

            pMark = memchr(request+nReq, ENDMARK, nRx); // look for end marker

            nReq += nRx;  // update the count of bytes received so far
            if (pMark != NULL)  // if end marker found
            {
                request[nReq] = 0;  // add 0 byte to make request into a string
                *pMark = '\0';      // replace last character of request to NULL
                // Print details of the request (optional)
                printf("\nRequest received, %d bytes: |%s|\n", nReq, request);

				temp = strtok(request, " "); //take everything before ' '
                strcpy(req,temp); //copy char from temp to req
				temp = strtok (NULL, " "); //take what is left in request

                if(strcmp(req, "d") == 0 ){ //Syntax "[operation d/u] [filename]"
                    char file_path[MAXREQUEST+10] = "downloads/"; //we dont need a null at the end since it is used to open the file

                    strcat(file_path, temp); //Add folder name before filename, temp is everything after ‘d’ or ‘u’ in the request
                    printf("Opening file in file destination %s\n", file_path);

                    fp = fopen(file_path, "rb");  // open for binary read
                    if(fp == NULL){ //Checking for errors
                        nResp = sprintf(response, "f0*"); //send header before tranmission '*[t-true (file exists) f-false (false)]*[fileSize]*'
                        retVal = send(cSocket, response, nResp, 0);  // send nResp bytes
                        printf("#404 - File not found!\n");
                    } else {
                        retVal = fseek(fp, 0, SEEK_END); // set current position to end of file
                        nBytes = ftell(fp); // find out what current position is
                        nResp = sprintf(response, "t%li*",nBytes); //send header before tranmission '*[t-true (file exists) f-false (false)]*[fileSize]*'
                        retVal = send(cSocket, response, nResp, 0);  // send nResp bytes

                        retVal = fseek(fp, 0, SEEK_SET);   // set current position to start of file

                        printf("Opened file of size %li\n", nBytes);


                        // Loop to send data bytes
                        while(!feof(fp)){
                            retVal = (int) fread(data, 1, BLOCKSIZE, fp);  // read up to 40 bytes
                            send(cSocket, data, retVal, 0); // send data to client using cSocket
                            nResp += retVal;
                            printf("Sent bytes of size %d to the client [%.2f%%]\n", retVal,((float)nResp/(float)nBytes)*100.00);
                        }
                        printf("File of %d bytes, have been successfully sent\n", nResp);
                        fclose(fp);
                        writeLog(inet_ntoa(clientIP), "d", file_path ,nBytes); // write a log of current operation
                        printf("File closed.\n");
                    }
                }
                else if(strcmp(req, "u") == 0 ){ // If the client wishes to upload a file
                    char file_path[MAXREQUEST+8] = "uploads/"; //allocate memory for file_path
                    strcat(file_path, temp); //Add folder name before filename, temp is everything after ‘d’ or ‘u’ in the request

                    printf("Opening file in directory %s for upload...\n\n",file_path);


                    // Receive header from user.
                    while (stop == 0)
                    {
                        retVal = recv(cSocket, digit, 1, 0); //receive header one byte at the time
                        if( retVal == SOCKET_ERROR)  // check for error
                        {
                            printf("Problem receiving\n");
                            //printError();   // uncomment this to see more details of the error
                            stop = 1;  // set flag to exit the loop - no point in trying again
                        }
                        else if (retVal == 0)  // connection closed
                        {
                            printf("Connection closed by client\n");
                            stop = 1;
                        }
                        else if (retVal > 0)  // we got the header
                        {
                            //printf("Received %d bytes from client\nHeader '%s'\n", nRx, response); //?
                            response[i] = digit[0];
                            i++;
                            if (digit[0] == ENDMARK_HEADER)//end marker for header
                            {
                                printf("Received %d header bytes from client\n", strlen(response)); //?
                                sscanf(response, "%d*", &nBytes);
                                fp = fopen(file_path, "wb");    //open file to receive data
                                header = 1; //header received
                                break;
                            }
                        }
                    }

                        if(header == 1)
                        {
                            nResp = 0; //reset total count before the operation
                            do  // loop to receive entire reply, terminated by end marker
                            {

                                // Wait to receive bytes from the server, using the recv function
                                // recv() arguments: socket handle, array to hold received bytes,
                                // maximum number of bytes to receive, last argument 0.
                                // Normally, recv() will not return until it receives at least one byte...
                                nRx = recv(cSocket, response, BLOCKSIZE, 0); // receive to BLOCKSIZE bytes
                                // nRx will be number of bytes received, or error indicator

                                if( nRx == SOCKET_ERROR)  // check for error
                                {
                                    printf("Problem receiving\n");
                                    printError();   // uncomment this to see more details of the error
                                    stop = 1;  // set flag to exit the loop - no point in trying again
                                }
                                else if (nRx == 0)  // connection closed
                                {
                                    printf("Connection closed by client\n");
                                    stop = 1;
                                }
                                else if (nRx > 0)  // we got some data
                                {
                                    // write whatever has been received
                                    response[nRx] = 0; // convert response to a string

                                    fwrite(response, 1, nRx, fp); // write received data into a file



                                    nResp += nRx;  // update the count of bytes received so far
                                    printf("Received %d bytes from client [%.2f%%]\n", nRx,((float)nResp/(float)nBytes)*100.00);

                                    // filesize has been reached
                                    if (nResp >= nBytes) // response has reached limit (or exceeded, so it wouldn't get stuck forever
                                    {
                                        printf("File of %d bytes, have been successfully received\n", nResp);
                                        writeLog(inet_ntoa(clientIP), "u", file_path , nBytes); //log the successful operation
                                        nResp = 0; //zero count, ready for next request
                                        endFound = 1; //file end was reached
                                    }

                                    // Otherwise, loop will continue to collect more of the response...

                                } // end of if data received

                            }   while((endFound == 0) && (stop == 0));

                            // Cleaning up values and get them ready for next request
                            endFound = 0;
                            header = 0;
                            i = 0;
                            memset(response, 0, sizeof(response));
                            fclose(fp);
                            printf("File closed.\n");
                        }
				}
				else
				{
                   printf("ERROR: Unknown request received from the client\n"); //Just in case so it wouldnt get stuck in the loop

				}

                nReq = 0;   // zero count, ready for next request
            } // end of if end marker found

            // If end marker not found in this part of request, check if
            // maximum request length has been reached - if so, give up.
            // A proper server should handle this better, and stay working...
            else if (nReq == MAXREQUEST) // request has reached limit
            {
                printf("Max request length reached, but no end marker!!\n");
                stop = 1;

                // close the connection and the initialise
            }

            // Otherwise, loop will continue to collect more data...

        } // end of if data received

    } while (stop == 0);  // repeat until told to stop
    // This loop continues until the client disconnects or there is an error

    // When the loop ends, it is time to close the connection and tidy up
    printf("Connection closing...\n");

    // Shut down the sending side of the TCP connection first
    retVal = shutdown(cSocket, SD_SEND);
    if( retVal != 0)  // check for error
    {
        printf("*** Error shutting down sending\n");
        printError();
    }

    // Then close the client socket
    retVal = closesocket(cSocket);
    if( retVal != 0)  // check for error
    {
        printf("*** Error closing client socket\n");
        printError();
    }
    else printf("Client socket closed\n");

    // Then close the server socket
    retVal = closesocket(serverSocket);
    if( retVal != 0)  // check for error
    {
        printf("*** Error closing server socket\n");
        printError();
    }
    else printf("Server socket closed\n");

    // Finally clean up the winsock system
    retVal = WSACleanup();
    printf("WSACleanup returned %d\n",retVal);

    // Prompt for user input, so window stays open when run outside CodeBlocks
    printf("\nPress return to exit:");
    gets(response);
    return 0;
}

/* Function to write request details into a log */
void writeLog(char* clientIP, char* requestType, char* filename, int filesize){
    FILE *logfp = fopen("log.txt", "a"); //write from the end of the file
    if(logfp == NULL){  //if unable to open the file
        printf("[ERROR] Failed to write to log\n");
    }else{
    //Writing Data
    printf("Writing to log file\n"); //let server know that it was logged
    fprintf(logfp, "%s\t%s\t%s\t%d\n", clientIP, requestType, filename, filesize);//writing into log file into given format
    fclose(logfp); //close file after each log
    }
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
