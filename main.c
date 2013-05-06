#include "winsock2.h"
#include "ws2tcpip.h"
#include <stdio.h>
#include <time.h>
#include <string.h>
#include "libs/strlib.h"
#include <sys/types.h>
#include <errno.h>
//#include &lt;dir.h&gt;
#include <direct.h>
#include <sys/stat.h>

#define PORT 6789

string getHeader(string filename, int length, int Ecode, string Estring, string protocol);
string getTime();
string getModTime(string filename);
string getMime(string filename);
int getFileSize(FILE * file);
string checkConGET(char *get_http);
string getHost();
void GetCurrentPath(char* buffer);

int main( int argc, const char* argv[]){

	WSADATA wsaData;
	WORD wVersionRequested = MAKEWORD( 2, 2 );
	int err = WSAStartup( wVersionRequested, &wsaData );
	struct sockaddr_in sin;
	struct sockaddr_in clientAddr;
	struct in_addr client_ip_addr;
	struct stat sb;
	string directoryName = "wwwroot";
	FILE *in_file;
	FILE *log_file;
	int clientAddrLen, ch;
	int ok, i, strLength, iResult;
	string header, filename = "";
	char cmdHTTP[80], filenameHTTP[80], protocolHTTP[80];
	SOCKET s;
	SOCKET s1;
	char CurrentPath[_MAX_PATH];
	string test = "";
	string test2 = "";
	int e;
	
	//Directory stuff
	e = stat(directoryName, &sb);
	if (e != 0)
	{
		printf("The directory does not exist. Creating new directory...\n");
		e = mkdir((char*)directoryName);
		if (e != 0)
		{
			printf("Could not create a directory. Exiting...");
			return 1;
		}
	}
	
	//Initialize socket
	s = socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
	sin.sin_family = AF_INET;
	sin.sin_port = htons(PORT);
	sin.sin_addr.s_addr = INADDR_ANY;
	bind(s, (struct sockaddr *)&sin, sizeof(sin));
	if( listen(s,SOMAXCONN)==SOCKET_ERROR){
		printf("\n\nERROR\n\nFailure to listen. Please restart the server.\n\n");
		getchar();
		return 1;
	}
	
	// Hämtar våran nuvarande working directory. För att använda en mapp så måste vi
	// ha den absoluta sökvägen eller liknande.
	GetCurrentPath(CurrentPath);
	test = Concat (CurrentPath,"\\wwwroot\\" );
	printf("PATH: %s\n", test);
	printf("Server started. Waiting for connections on %s at port %d\n\n", getHost(),PORT);
	clientAddrLen = sizeof(clientAddr);
	
	while(TRUE){
	    //Skapar loggfil samt felhantering
		char in_buf[2048];
		test2 = Concat(test,"webserver_log.txt");
		log_file = fopen(test2, "a+");
		if (log_file==NULL){
			printf("You must create a wwwroot folder. Restart the server.\n");
			getchar();
			return 0;
		}
		
		//Börjar vänta på klienter.
		s1 = accept(s, (struct sockaddr *)&clientAddr, &clientAddrLen);
		recv(s1, in_buf, sizeof(in_buf), 0);
		printf("\nRecieved from browser: \n%s",in_buf);
		// Fixar till clientents ipadress från byte&gt;ip-struktur
		memcpy(&client_ip_addr, &clientAddr.sin_addr.s_addr, 4);
		printf("\nAccept completed (IP address of client = %s port = %d) \n",
		inet_ntoa(client_ip_addr), ntohs(clientAddr.sin_port));
		ok = sscanf(in_buf,"%s %s %s",cmdHTTP,filenameHTTP,protocolHTTP);
		//printf("Recived: %s %s %s\n",cmdHTTP,filenameHTTP,protocolHTTP);
		// Tar bort '\' från filnamnet
		strLength = strlen(filenameHTTP);
		for(i = 1; i <= strLength+1; i++)
		filenameHTTP[i-1] = filenameHTTP[i];
		test2 = Concat(test,filenameHTTP);
		// Öppnar en stream till filen genom readbyte-metoden.
		in_file = fopen(test2, "rb");
		// Om filen finns eller en ogiltig mime typ så skickar den en 404 ERROR, annars så går den vidare
		// med att skicka filen.
		if (in_file!=NULL && getMime(test2) != NULL){
			//Conditional GET variabler initeras
			char modTimeMonth[10], modTimeTime[10], modTimeWDay[10];
			char conTimeMonth[10], conTimeTime[10], conTimeWDay[10];
			int modTimeYear, modTimeDay, conTimeYear, conTimeDay;
			//Läser in Last-Modified
			sscanf(getModTime(test2),"%s %d %s %d %s",modTimeWDay,&modTimeDay,modTimeMonth,&modTimeYear,modTimeTime);
			printf("Last-Modified: %s %d %d %s\n\n",modTimeMonth,modTimeDay,modTimeYear,modTimeTime);
			//Läser in If-Modified-Since
			if(checkConGET(in_buf) != ""){
				sscanf(checkConGET(in_buf),"%s %d %s %d %s",conTimeWDay,&conTimeDay,conTimeMonth,&conTimeYear,conTimeTime);
				printf("If-Modified-Since: %s %d %d %s\n\n",conTimeMonth,conTimeDay,conTimeYear,conTimeTime);
				//Kollar om If-Modified-Since är nyare än Last-Modified
				if (conTimeYear >= modTimeYear && strcmp(conTimeMonth, modTimeMonth)>=0 && conTimeDay >= modTimeDay && strcmp(conTimeTime, modTimeTime)>=0){
					printf("Reply: 304 NOT MODIFIED, Sending only the header.");
					header = getHeader(test2, 0, 304, "Not Modified", protocolHTTP);
					send(s1,header,strlen(header),0);
				}
			}
			if(checkConGET(in_buf) == ""){
				printf("No conditional GET found..\n");
				//Skickar normal header.
				header = getHeader(test2, getFileSize(in_file), 200, "OK", protocolHTTP);
				printf("Sending header: \n%s",header);
				send(s1,header,strlen(header),0);
				printf("Reply: 200 OK, sending %s which is %d bytes long.\n",filenameHTTP, getFileSize(in_file));
				//Skickar filen
				while(!feof(in_file)) {
					char buffer[256];
					int len = fread(buffer,1,256,in_file);
					send(s1,buffer,len,0);
				}
			}
			fclose(in_file);
		}
		else {
			send(s1,"HTTP/1.1 404 Not Found\nConnection: close\n\n&lt;html&gt;&lt;body&gt;&lt;h1&gt;ERROR 404&lt;/h1&gt;</br>FILE NOT FOUND.&lt;/body&gt;&lt;/html&gt;",strlen("HTTP/1.1 404 Not Found\nConnection: close\n\n&lt;html&gt;&lt;body&gt;&lt;h1&gt;ERROR 404&lt;/h1&gt;</br>FILE NOT FOUND.&lt;/body&gt;&lt;/html&gt;"),0);
			printf("Reply: 404 NOT FOUND, Requested file %s not found.\n",filenameHTTP);
		}
		fprintf(log_file,"Client %s on port %d sent %s /%s %s \n",inet_ntoa(client_ip_addr), ntohs(clientAddr.sin_port),cmdHTTP,filenameHTTP,protocolHTTP);
		fprintf(log_file,"Server %s on port %d answered with \n%s %s \n",getHost(),PORT,header,filenameHTTP);
		fprintf(log_file,"\n-----------------------------------------------------------\n");
		fclose(log_file);
		closesocket(s1);
	}
	closesocket(s);
	WSACleanup();
	getchar();
}
string getHeader(string filename, int length, int Ecode, string Estring, string protocol){
	string header = "";
	header = Concat(header, protocol);
	header = Concat(header, Concat(" ",IntegerToString(Ecode)));
	header = Concat(header, Concat(" ",Estring));
	header = Concat(header, Concat("\nDate: ",getTime()));
	header = Concat(header, "\nServer: OscKenWEB/1.0");
	header = Concat(header, Concat("\nLast-Modified: ", getModTime(filename)));
	header = Concat(header, Concat("\nContent-Length: ", IntegerToString(length+4)));
	header = Concat(header, "\nConnection: close\n");
	header = Concat(header, Concat("Content-Type: ", getMime(filename)));
	header = Concat(header, "\n\n");
	//printf("HEADER\n%s\n", header);
	return header;
}
string getTime(){
	time_t now;
	char timebuf[100];
	string time1 = "";
	int i;
	now = time(NULL);
	strftime(timebuf ,30,"%a, %d %b %Y %X GMT",gmtime(&now));
	for (i = 0; i<30;i++)
	time1 = Concat(time1, CharToString(timebuf[i]));
	return time1 ;
}
string getModTime(string filename){
	char timeStr[100];
	string time1 = "";
	int i;
	struct stat test;
	if (!stat(filename, &test))
	{
		strftime(timeStr, 30, "%a, %d %b %Y %X GMT", localtime( &test.st_mtime));
		//printf("\nLast modified date and time = %s\n", timeStr);
	}
	else
	{
		printf("error getting mtime\n");
		return NULL;
	}
	for (i = 0; i<30;i++)
	time1 = Concat(time1, CharToString(timeStr[i]));
	return time1;
}
string getMime(string name){
	string dot = strrchr(name, '.');
	if (dot==NULL) return NULL;
	if (strcmp(dot, ".html") == 0 || strcmp(dot, ".htm") == 0) return "text/html";
	if (strcmp(dot, ".jpg") == 0 || strcmp(dot, ".jpeg") == 0) return "image/jpeg";
	if (strcmp(dot, ".gif") == 0) return "image/gif";
	if (strcmp(dot, ".png") == 0) return "image/png";
	if (strcmp(dot, ".css") == 0) return "text/css";
	if (strcmp(dot, ".au") == 0) return "audio/basic";
	if (strcmp(dot, ".wav") == 0) return "audio/wav";
	if (strcmp(dot, ".avi") == 0) return "video/x-msvideo";
	if (strcmp(dot, ".mpeg") == 0 || strcmp(dot, ".mpg") == 0) return "video/mpeg";
	if (strcmp(dot, ".mp3") == 0) return "audio/mpeg";
	return NULL;
}
int getFileSize(FILE * file)
{
	int fileSize = 0;
	fseek (file , 0 , SEEK_END);
	fileSize = ftell (file);
	rewind (file);
	return fileSize;
}
string checkConGET(char *in_buf){
	string HTTPinput = in_buf;
	int i = FindString("If-Modified-Since:",HTTPinput,0);
	//printf("\nINPUT:\n\n%s \n\n\n",HTTPinput);
	if(i != -1) {
		string value = SubString(HTTPinput, i+18,i+18+30);
		//printf("I got a conditional GET with the date: %s..\n",value);
		return value;
	}
	else {
		//printf("No conditional GET found..\n");
	}
	return "";
}
// Hämtar datorns WAN ipadress.
string getHost(){
	WSADATA wsaData;
	WORD wVersionRequested = MAKEWORD( 2, 2 );
	int err = WSAStartup( wVersionRequested, &wsaData );
	char ac[80];
	struct in_addr addr;
	struct hostent *phe;
	if (gethostname(ac, sizeof(ac)) == SOCKET_ERROR) {
		return "ERROR";
	}
	phe = gethostbyname(ac);
	memcpy(&addr, phe->h_addr_list[0], sizeof(struct in_addr));
	return inet_ntoa(addr);
}
void GetCurrentPath(char* buffer)
{
	getcwd(buffer, _MAX_PATH);
}