#include <cstdlib>
#include <iostream>
#include <string.h>
#include <fstream>
#include <boost/crc.hpp>
#include <sstream>
#include <vector>
#include <netdb.h>
#include <ctime>

int MAX_MESSAGES = 800;
clock_t sentTicks;
time_t sentSecs;

using namespace std;


void message_to_CRC(std::string messagesCRC[],long messagesCRCvalues[]){
	std::string my_string;
	long res;
	std::ostringstream res2;
	boost::crc_32_type result;

	for(int i=0;i<MAX_MESSAGES;i++){
		result.reset();
		my_string = (string)messagesCRC[i];
	    result.process_bytes(my_string.data(), my_string.length());
	    res = result.checksum();
	    messagesCRCvalues[i] = res;
	}
}

int read_to_array(const char *filename, std::string messages[]){
		string inputFileContents;

		std::ifstream in( filename, std::ios::in | std::ios::binary );
		if(in)
		{
			inputFileContents = std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
		}
		else
		{
			cout << "Unable to read input file";
			exit(0);
		}

		size_t pos = 0;
		string msg;
		string seperator = "-END OF MESSAGE-\n";

		int msgNum = 0;
		while((pos = inputFileContents.find(seperator)) != std::string::npos){
			msg = inputFileContents.substr(0,pos);
			messages[msgNum] = msg;
			inputFileContents.erase(0, pos+seperator.length());
			msgNum++;
		}
		return (msgNum);
}

int main(int argc, char* argv[]) {

	bool client = false;
	bool server = false;

	switch(argc){
		case 5:
			cout << "running client" << endl;
			client = true;
			break;
		case 3:
			cout << "running server" << endl;
			server = true;
			break;
		default:
			cout << """Please run as a client or server with the options\n"
								"client:\n"
								"prog_name name_of_machine_running_server server_port file_of_CRC32_codes file_of_message\n"
								"server:\n"
								"prog_name port_number_to_listen_on file_to_write_messages_to""" << endl;
			exit (EXIT_FAILURE);
	}

	string messages[MAX_MESSAGES];
	string messagesCRC[MAX_MESSAGES];
	long messagesCRCvalues[MAX_MESSAGES];
	int messageTicks[MAX_MESSAGES];
	int messageSecs[MAX_MESSAGES];
	int messageBytes[MAX_MESSAGES];

	struct packet{
		int32_t clientIP;
		int16_t clientPORT;
		int32_t length;
		int32_t crc32;
		string payload;
	};

	if(client){
		const char *filename;

		filename = argv[4];

		int numberOfMessages = read_to_array(filename, messages);

		filename = argv[3];
		read_to_array(filename, messagesCRC);
		message_to_CRC(messagesCRC, messagesCRCvalues);

		int sock;
		if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
		{
			cout << "Unable to create socket" << endl;
			exit(0);
		}

		int16_t serverPort = atoi(argv[2]);

		struct hostent *serverHostName;
		serverHostName = gethostbyname(argv[1]);


		struct sockaddr_in clientAddress;
		memset((char *)&clientAddress, 0, sizeof(clientAddress));
		clientAddress.sin_family = AF_INET;
		clientAddress.sin_addr.s_addr = htonl(INADDR_ANY);

		if (bind(sock, (struct sockaddr *)&clientAddress, sizeof(clientAddress)) < 0)
		{
			cout << "Unable to bind socket" << endl;
			exit(0);
		}

		//needed to write the port number back into clientAddress
		socklen_t len = sizeof(clientAddress);
		getsockname(sock, (sockaddr*) &clientAddress, &len);

		struct sockaddr_in serverAddress;
		memset((char *)&serverAddress, 0, sizeof(serverAddress));
		serverAddress.sin_family = AF_INET;
		serverAddress.sin_addr.s_addr = htonl(INADDR_ANY);

		bcopy((char*)serverHostName->h_addr,
				(char*)&serverAddress.sin_addr.s_addr,
				serverHostName->h_length);

		serverAddress.sin_port = htons(serverPort);//0


		for(int i = 0;i<numberOfMessages;i++){
			packet pk;
			vector<char> packetBuffer;
			char* returnBuffer = new char[5];

			pk.clientIP = ntohs(clientAddress.sin_addr.s_addr);
			pk.clientIP = 2130706433;
			char first  = (pk.clientIP & 0xff000000) >> 24;
			char second = (pk.clientIP & 0x00ff0000) >> 16;
			char third  = (pk.clientIP & 0x0000ff00) >> 8;
			char fourth = (pk.clientIP & 0x000000ff);
			packetBuffer.insert(packetBuffer.end(),first);
			packetBuffer.insert(packetBuffer.end(),second);
			packetBuffer.insert(packetBuffer.end(),third);
			packetBuffer.insert(packetBuffer.end(),fourth);

			int16_t port = ntohs(clientAddress.sin_port);
			pk.clientPORT = (int16_t)port;

			first = (port & 0xff00) >> 8;
			second = (port & 0xff);
			packetBuffer.insert(packetBuffer.end(),first);
			packetBuffer.insert(packetBuffer.end(),second);

			pk.length = messages[i].length(); //message length in bytes
			int32_t number = pk.length;
			first  = (number & 0xff000000) >> 24;
			second = (number & 0x00ff0000) >> 16;
			third  = (number & 0x0000ff00) >> 8;
			fourth = (number & 0x000000ff);
			packetBuffer.insert(packetBuffer.end(),first);
			packetBuffer.insert(packetBuffer.end(),second);
			packetBuffer.insert(packetBuffer.end(),third);
			packetBuffer.insert(packetBuffer.end(),fourth);

			pk.crc32 = messagesCRCvalues[i];
			number = pk.crc32;
			first  = (number & 0xff000000) >> 24;
			second = (number & 0x00ff0000) >> 16;
			third  = (number & 0x0000ff00) >> 8;
			fourth = (number & 0x000000ff);
			packetBuffer.insert(packetBuffer.end(),first);
			packetBuffer.insert(packetBuffer.end(),second);
			packetBuffer.insert(packetBuffer.end(),third);
			packetBuffer.insert(packetBuffer.end(),fourth);

			int packetPayloadLength = 0;
			pk.payload = messages[i];

			for(int x = 0;x<pk.length;x++){
				packetBuffer.insert(packetBuffer.end(),pk.payload.at(x));
				packetPayloadLength++;
			}

			int numberOfHeaderBytes = 14;
			int packetBufferLength = numberOfHeaderBytes+packetPayloadLength;

			messageBytes[i] = packetBufferLength;
			sentTicks = std::clock();
			sentSecs = std::time(0);


			int sendingIntCode;
			sendingIntCode = sendto(sock, packetBuffer.data(), packetBufferLength, 0,
									(struct sockaddr *)&serverAddress, sizeof(serverAddress));
			if (sendingIntCode < 0)
			{
				cout << "Packet Sending Error" << sendingIntCode << endl;;
			}
			else
			{
				//no sending error
			}

			socklen_t addrlen = sizeof(serverAddress);
			int recievedLength;

			recievedLength = recvfrom(sock, returnBuffer, 5, 0,
										(struct sockaddr *)&serverAddress, &addrlen);
			messageTicks[i] = (std::clock()-sentTicks);
			messageSecs[i] = std::time(0)-sentSecs;
			ofstream timeFile;
			timeFile.open("times.txt",ios::app);


			timeFile << messageBytes[i] << "," << messageTicks[i] << ",";
			timeFile << fixed << (double)messageTicks[i]/(double)CLOCKS_PER_SEC;
			timeFile << "," << fixed << (long double)messageSecs[i] << endl;
			cout << scientific;

			if ((char)returnBuffer[0] != (char)170)
			{
				cout << "ACK header mismatch" << endl;
				exit(0);
			}

			number = pk.length + numberOfHeaderBytes;
				first  = (number & 0xff000000) >> 24;
				second = (number & 0x00ff0000) >> 16;
				third  = (number & 0x0000ff00) >> 8;
				fourth = (number & 0x000000ff);

			    if ( (char)returnBuffer[1] != (char)first or
					 (char)returnBuffer[2] != (char)second or
					 (char)returnBuffer[3] != (char)third or
					 (char)returnBuffer[4] != (char)fourth
					)
				{
					cout << "ACK length mismatch" << number<<":"<<(int8_t)returnBuffer[1]<<endl;
					exit(0);
				}

		}

		close (sock);
	}//end if client

	if(server){
		int errorFreeMessages{0};
		int errorMessages{0};
		int serverPort = atoi(argv[1]);
		int sock;

		int BUFSIZE = 4096000;
		unsigned char receivedDataBuffer[BUFSIZE];
		int receivedDataBytesLength;
		vector<char> ackBuffer;

		long messageCRC[200000];
		string revievedMessage[200000];

		struct sockaddr_in clientAddress;
		memset((char *)&clientAddress, 0, sizeof(clientAddress));
		clientAddress.sin_family = AF_INET;

		if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
		{
			cout << "Unable to create socket" << endl;
			exit(0);
		}


		struct sockaddr_in serverAddress;
		memset((char *)&serverAddress, 0, sizeof(serverAddress));
		serverAddress.sin_family = AF_INET;
		serverAddress.sin_addr.s_addr = htonl(INADDR_ANY);
		serverAddress.sin_port = htons(serverPort);//0

		if (bind(sock, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) < 0)
		{
			cout << "Unable to bind socket" << endl;
			exit(0);
		}


		for (;;) {

		socklen_t addrlen = sizeof(serverAddress);
		receivedDataBytesLength = recvfrom(sock, receivedDataBuffer, BUFSIZE, 0,
											(struct sockaddr *)&clientAddress, &addrlen);

		         packet recivedPacket;
		memset((char *)&recivedPacket.clientIP+3,receivedDataBuffer[0],1);
		memset((char *)&recivedPacket.clientIP+2,receivedDataBuffer[1],1);
		memset((char *)&recivedPacket.clientIP+1,receivedDataBuffer[2],1);
		memset((char *)&recivedPacket.clientIP+0,receivedDataBuffer[3],1);

		memset((char *)&recivedPacket.clientPORT+1,receivedDataBuffer[4],1);
		memset((char *)&recivedPacket.clientPORT+0,receivedDataBuffer[5],1);

		memset((char *)&recivedPacket.length+3,receivedDataBuffer[6],1);
		memset((char *)&recivedPacket.length+2,receivedDataBuffer[7],1);
		memset((char *)&recivedPacket.length+1,receivedDataBuffer[8],1);
		memset((char *)&recivedPacket.length+0,receivedDataBuffer[9],1);

		memset((char *)&recivedPacket.crc32+3,receivedDataBuffer[10],1);
		memset((char *)&recivedPacket.crc32+2,receivedDataBuffer[11],1);
		memset((char *)&recivedPacket.crc32+1,receivedDataBuffer[12],1);
		memset((char *)&recivedPacket.crc32+0,receivedDataBuffer[13],1);

		string c;
		string msg;
		if (receivedDataBytesLength > 14)
		{
			for(int i = 14 ; i <(int)receivedDataBytesLength ; i++){
				c = receivedDataBuffer[i];
				msg.append(c);
			}
		}
		recivedPacket.payload.assign(msg);

		revievedMessage[0].assign(recivedPacket.payload);
		message_to_CRC(revievedMessage, messageCRC);

		if(messageCRC[0] == recivedPacket.crc32)
		{
			errorFreeMessages++;
			cout << "Error-free message received from "
					<< clientAddress.sin_addr.s_addr << ", " << dec << ntohs(recivedPacket.clientPORT);
		}else{
			errorMessages++;
			cout << "Error in message received from "
					<< clientAddress.sin_addr.s_addr << ", " << dec << ntohs(recivedPacket.clientPORT);
		}
		cout << " : " << errorFreeMessages << " error free messages of "<< errorFreeMessages+errorMessages << " total\n";

		ofstream outputFile;
		outputFile.open(argv[2], ios::app);
		outputFile << recivedPacket.payload;
		outputFile << "-END OF MESSAGE-\n";

		int ackBufferLength = 5;
		ackBuffer.clear();
		ackBuffer.push_back((int)170);
		int number = receivedDataBytesLength;
		char first  = (number & 0xff000000) >> 24;
		char second = (number & 0x00ff0000) >> 16;
		char third  = (number & 0x0000ff00) >> 8;
		char fourth = (number & 0x000000ff);
		ackBuffer.insert(ackBuffer.end(),first);
		ackBuffer.insert(ackBuffer.end(),second);
		ackBuffer.insert(ackBuffer.end(),third);
		ackBuffer.insert(ackBuffer.end(),fourth);
		if (sendto(sock, ackBuffer.data(), ackBufferLength, 0,
					(struct sockaddr *)&clientAddress, sizeof(clientAddress)) < 0)
		{
			cout << "ACK Sending Error";
		}
		else
		{
			//ACK sent
		}

		} //endless loop

		close(sock); //in case of erroneous loop exit
	}//end the server

	return (EXIT_SUCCESS);
}
